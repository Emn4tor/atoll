/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "share/sharecredentials.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace
{
ShareCredentials read(const QString &certificatePath, const QString &keyPath)
{
    ShareCredentials credentials;

    QFile certificateFile(certificatePath);
    QFile keyFile(keyPath);
    if (!certificateFile.open(QIODevice::ReadOnly) || !keyFile.open(QIODevice::ReadOnly)) {
        return credentials;
    }

    credentials.certificate = QSslCertificate(&certificateFile, QSsl::Pem);
    credentials.key = QSslKey(&keyFile, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey);
    if (credentials.certificate.isNull() || credentials.key.isNull()
        || credentials.certificate.expiryDate() <= QDateTime::currentDateTimeUtc()) {
        return ShareCredentials();
    }

    credentials.fingerprint = QString::fromLatin1(
        QCryptographicHash::hash(credentials.certificate.toDer(), QCryptographicHash::Sha256)
            .toHex()
            .toUpper());
    return credentials;
}

bool mint(const QString &certificatePath, const QString &keyPath)
{
    const QString openssl = QStandardPaths::findExecutable(u"openssl"_s);
    if (openssl.isEmpty()) {
        qWarning("atoll: openssl is not installed, so sharing stays unencrypted and the "
                 "LocalSend app will not talk to this machine");
        return false;
    }

    QFile::remove(certificatePath);
    QFile::remove(keyPath);

    QProcess process;
    process.start(openssl,
                  {u"req"_s, u"-x509"_s, u"-newkey"_s, u"rsa:2048"_s, u"-sha256"_s, u"-nodes"_s,
                   // The certificate is an identity rather than a claim about a
                   // name, and replacing it would make this machine a stranger
                   // again to everyone who remembered it.
                   u"-days"_s, u"7300"_s, u"-subj"_s, u"/CN=Atoll"_s, u"-keyout"_s, keyPath,
                   u"-out"_s, certificatePath});
    if (!process.waitForFinished(20'000) || process.exitStatus() != QProcess::NormalExit
        || process.exitCode() != 0) {
        qWarning("atoll: could not create a sharing certificate (%s)",
                 qUtf8Printable(QString::fromUtf8(process.readAllStandardError()).trimmed()));
        return false;
    }

    QFile::setPermissions(keyPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}
}

ShareCredentials ShareCredentials::load(const QString &directory)
{
    const QString certificatePath = directory + u"/certificate.pem"_s;
    const QString keyPath = directory + u"/key.pem"_s;

    ShareCredentials credentials = read(certificatePath, keyPath);
    if (credentials.isValid()) {
        return credentials;
    }

    QDir().mkpath(directory);
    if (!mint(certificatePath, keyPath)) {
        return ShareCredentials();
    }
    return read(certificatePath, keyPath);
}
