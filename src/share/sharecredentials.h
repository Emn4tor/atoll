/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QSslCertificate>
#include <QSslKey>
#include <QString>

/**
 * Atoll's own TLS identity, kept in the data directory and made once.
 *
 * LocalSend's transport is TLS between two self-signed strangers: nobody
 * verifies a chain, and the SHA-256 of the certificate *is* the device's
 * identity. The real app's server also asks for a client certificate, so a
 * certificate is not optional for talking to it - not even for sending.
 *
 * Qt cannot mint one, so openssl does it on first run. Without openssl Atoll
 * falls back to plain HTTP, which other implementations still accept but the
 * LocalSend app will not connect to.
 */
struct ShareCredentials {
    QSslCertificate certificate;
    QSslKey key;
    /** Uppercase hex SHA-256 of the certificate, which is how peers name us. */
    QString fingerprint;

    bool isValid() const
    {
        return !certificate.isNull() && !key.isNull() && !fingerprint.isEmpty();
    }

    /** Load the pair from `directory`, creating it if it is not there yet. */
    static ShareCredentials load(const QString &directory);
};
