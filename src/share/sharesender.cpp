/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "share/sharesender.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslConfiguration>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

ShareSender::ShareSender(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
}

ShareSender::~ShareSender()
{
    delete m_source;
}

void ShareSender::setIdentity(const ShareIdentity &identity)
{
    m_identity = identity;
}

void ShareSender::setCredentials(const ShareCredentials &credentials)
{
    m_credentials = credentials;
}

QString ShareSender::currentFile() const
{
    if (m_index < 0 || m_index >= m_files.size()) {
        return {};
    }
    return m_files.at(m_index).name;
}

QNetworkRequest ShareSender::request(const QString &route) const
{
    QNetworkRequest request{QUrl(m_peer.baseUrl() + u'/' + route)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    request.setTransferTimeout(20'000);
    if (m_peer.protocol == u"https"_s) {
        QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
        tls.setPeerVerifyMode(QSslSocket::VerifyNone);
        // The LocalSend app's server asks for a client certificate and drops
        // the handshake without one, so this is what makes sending work at all.
        if (m_credentials.isValid()) {
            tls.setLocalCertificate(m_credentials.certificate);
            tls.setPrivateKey(m_credentials.key);
        }
        request.setSslConfiguration(tls);
    }
    return request;
}

void ShareSender::watch(QNetworkReply *reply) const
{
    if (m_peer.protocol == u"https"_s) {
        connect(reply, &QNetworkReply::sslErrors, reply, [reply] {
            reply->ignoreSslErrors();
        });
    }
}

void ShareSender::send(const SharePeer &peer, const QList<ShareFile> &files)
{
    cancel();

    m_peer = peer;
    m_files = files;
    m_tokens.clear();
    m_sessionId.clear();
    m_index = -1;
    m_completed = 0;
    m_total = 0;
    for (const ShareFile &file : files) {
        m_total += file.size;
    }
    m_active = !files.isEmpty();

    if (!m_active) {
        Q_EMIT failed(tr("Nothing to send."));
        return;
    }

    Q_EMIT progress(0, m_total);
    prepare();
}

void ShareSender::prepare()
{
    QJsonObject files;
    for (const ShareFile &file : std::as_const(m_files)) {
        files.insert(file.id,
                     QJsonObject{
                         {u"id"_s, file.id},
                         {u"fileName"_s, file.name},
                         {u"size"_s, double(file.size)},
                         {u"fileType"_s, file.mimeType},
                         // No hash: reading every file twice to save the
                         // receiver a check it can do itself is a bad trade.
                         {u"sha256"_s, QJsonValue::Null},
                         {u"preview"_s, QJsonValue::Null},
                     });
    }

    const QJsonObject payload{
        {u"info"_s, m_identity.toJson()},
        {u"files"_s, files},
    };

    QNetworkReply *reply = m_network->post(request(u"prepare-upload"_s),
                                           QJsonDocument(payload).toJson(QJsonDocument::Compact));
    watch(reply);
    m_reply = reply;

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (!m_active) {
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError && status == 0) {
            abandon(tr("%1 did not answer.").arg(m_peer.alias));
            return;
        }
        switch (status) {
        case 200:
            break;
        case 204:
            m_active = false;
            Q_EMIT finished();
            return;
        case 401:
            abandon(tr("%1 asks for a PIN.").arg(m_peer.alias));
            return;
        case 403:
            abandon(tr("%1 declined.").arg(m_peer.alias));
            return;
        case 409:
            abandon(tr("%1 is busy with another transfer.").arg(m_peer.alias));
            return;
        default:
            abandon(tr("%1 refused the transfer (%2).").arg(m_peer.alias).arg(status));
            return;
        }

        const QJsonObject answer = QJsonDocument::fromJson(reply->readAll()).object();
        m_sessionId = answer.value(u"sessionId"_s).toString();
        const QJsonObject tokens = answer.value(u"files"_s).toObject();
        for (auto it = tokens.constBegin(); it != tokens.constEnd(); ++it) {
            m_tokens.insert(it.key(), it.value().toString());
        }
        if (m_sessionId.isEmpty() || m_tokens.isEmpty()) {
            abandon(tr("%1 sent an answer Atoll could not read.").arg(m_peer.alias));
            return;
        }

        uploadNext();
    });
}

void ShareSender::uploadNext()
{
    delete m_source;
    m_source = nullptr;

    // Files the receiver did not ask for are skipped rather than failed: a
    // partial acceptance is a normal answer in this protocol.
    do {
        ++m_index;
    } while (m_index < m_files.size() && !m_tokens.contains(m_files.at(m_index).id));

    if (m_index >= m_files.size()) {
        m_active = false;
        Q_EMIT progress(m_total, m_total);
        Q_EMIT finished();
        return;
    }

    const ShareFile &file = m_files.at(m_index);
    Q_EMIT fileChanged();

    m_source = new QFile(file.path);
    if (!m_source->open(QIODevice::ReadOnly)) {
        abandon(tr("Cannot read %1.").arg(file.name));
        return;
    }

    QUrlQuery query;
    query.addQueryItem(u"sessionId"_s, m_sessionId);
    query.addQueryItem(u"fileId"_s, file.id);
    query.addQueryItem(u"token"_s, m_tokens.value(file.id));

    QNetworkRequest upload = request(u"upload?"_s + query.toString(QUrl::FullyEncoded));
    upload.setHeader(QNetworkRequest::ContentTypeHeader, u"application/octet-stream"_s);
    upload.setHeader(QNetworkRequest::ContentLengthHeader, QVariant::fromValue(file.size));
    // Large files take as long as they take; the timeout is on the handshake.
    upload.setTransferTimeout(0);

    QNetworkReply *reply = m_network->post(upload, m_source);
    watch(reply);
    m_reply = reply;

    const qint64 alreadyDone = m_completed;
    connect(reply, &QNetworkReply::uploadProgress, this, [this, alreadyDone](qint64 sent, qint64) {
        if (m_active) {
            Q_EMIT progress(qMin(m_total, alreadyDone + sent), m_total);
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        reply->deleteLater();
        if (!m_active) {
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (status != 200) {
            abandon(status == 0 ? tr("The connection to %1 broke.").arg(m_peer.alias)
                                : tr("%1 rejected a file (%2).").arg(m_peer.alias).arg(status));
            return;
        }
        m_completed += m_files.at(m_index).size;
        uploadNext();
    });
}

void ShareSender::abandon(const QString &message)
{
    const bool wasActive = m_active;
    cancel();
    if (wasActive) {
        Q_EMIT failed(message);
    }
}

void ShareSender::cancel()
{
    const bool hadSession = m_active && !m_sessionId.isEmpty();
    m_active = false;

    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply = nullptr;
    }
    delete m_source;
    m_source = nullptr;

    if (hadSession) {
        QUrlQuery query;
        query.addQueryItem(u"sessionId"_s, m_sessionId);
        QNetworkReply *reply = m_network->post(request(u"cancel?"_s + query.toString(QUrl::FullyEncoded)),
                                               QByteArray());
        watch(reply);
        connect(reply, &QNetworkReply::finished, reply, &QNetworkReply::deleteLater);
    }
    m_sessionId.clear();
    m_index = -1;
}
