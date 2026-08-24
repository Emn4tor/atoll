/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "share/shareserver.h"

#include <algorithm>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslSocket>
#include <QTcpSocket>
#include <QUuid>

using namespace Qt::StringLiterals;

namespace
{
constexpr int MaxHeaderSize = 64 * 1024;
constexpr int MaxBodySize = 8 * 1024 * 1024; // Only metadata is ever buffered.
constexpr int ApprovalTimeout = 60'000;

const auto ApiPrefix = u"/api/localsend/v2/"_s;

QString token()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QByteArray reason(int status)
{
    switch (status) {
    case 200:
        return QByteArrayLiteral("OK");
    case 204:
        return QByteArrayLiteral("No Content");
    case 400:
        return QByteArrayLiteral("Bad Request");
    case 403:
        return QByteArrayLiteral("Forbidden");
    case 404:
        return QByteArrayLiteral("Not Found");
    case 409:
        return QByteArrayLiteral("Conflict");
    case 413:
        return QByteArrayLiteral("Payload Too Large");
    case 500:
        return QByteArrayLiteral("Internal Server Error");
    default:
        return QByteArrayLiteral("Error");
    }
}

/**
 * A sender chooses the file name, so it is not to be trusted with one. Folder
 * transfers legitimately carry a relative path, which is kept; anything that
 * tries to climb out of the download directory is flattened away.
 */
QString sanitise(const QString &name)
{
    QStringList parts;
    const auto segments = QString(name).replace(u'\\', u'/').split(u'/', Qt::SkipEmptyParts);
    for (const QString &segment : segments) {
        if (segment == u"." || segment == u"..") {
            continue;
        }
        QString cleaned = segment;
        cleaned.remove(QChar(0));
        if (!cleaned.isEmpty()) {
            parts.append(cleaned);
        }
    }
    if (parts.isEmpty()) {
        return u"file"_s;
    }
    return parts.join(u'/');
}
}

ShareServer::ShareServer(QObject *parent)
    : QTcpServer(parent)
{
    m_approvalTimer.setSingleShot(true);
    m_approvalTimer.setInterval(ApprovalTimeout);
    connect(&m_approvalTimer, &QTimer::timeout, this, [this] {
        if (hasPendingApproval()) {
            resolveApproval(false);
        }
    });
}

quint16 ShareServer::listenOn(quint16 preferred, int attempts)
{
    for (int i = 0; i < attempts; ++i) {
        const quint16 port = quint16(preferred + i);
        if (listen(QHostAddress::Any, port)) {
            if (i > 0) {
                qWarning("atoll: sharing port %u was taken, listening on %u instead", preferred, port);
            }
            return port;
        }
    }
    qWarning("atoll: cannot listen for incoming files (%s)", qUtf8Printable(errorString()));
    return 0;
}

void ShareServer::setIdentity(const ShareIdentity &identity)
{
    m_identity = identity;
}

void ShareServer::setCredentials(const ShareCredentials &credentials)
{
    m_credentials = credentials;
}

void ShareServer::setDestination(const QString &directory)
{
    m_destination = directory;
}

void ShareServer::setAutoAccept(bool automatic)
{
    m_autoAccept = automatic;
}

bool ShareServer::isBusy() const
{
    return m_sessionActive || hasPendingApproval();
}

bool ShareServer::hasPendingApproval() const
{
    return !m_approvalSocket.isNull();
}

void ShareServer::incomingConnection(qintptr handle)
{
    QTcpSocket *socket = nullptr;
    if (m_credentials.isValid()) {
        // Both ends of this protocol are self-signed strangers, so a peer
        // certificate is something to record, never something to trust.
        auto *tls = new QSslSocket(this);
        tls->setLocalCertificate(m_credentials.certificate);
        tls->setPrivateKey(m_credentials.key);
        tls->setPeerVerifyMode(QSslSocket::QueryPeer);
        connect(tls, &QSslSocket::sslErrors, tls, [tls] {
            tls->ignoreSslErrors();
        });
        socket = tls;
    } else {
        socket = new QTcpSocket(this);
    }

    if (!socket->setSocketDescriptor(handle)) {
        delete socket;
        return;
    }

    auto *connection = new Connection;
    connection->socket = socket;
    m_connections.insert(socket, connection);

    connect(socket, &QTcpSocket::readyRead, this, [this, connection] {
        onReadyRead(connection);
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, connection] {
        dropConnection(connection);
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [this, connection] {
        dropConnection(connection);
    });

    if (auto *tls = qobject_cast<QSslSocket *>(socket)) {
        tls->startServerEncryption();
    }
}

void ShareServer::onReadyRead(Connection *connection)
{
    QByteArray data = connection->socket->readAll();

    if (!connection->headerParsed) {
        connection->head.append(data);
        const int end = connection->head.indexOf("\r\n\r\n");
        if (end < 0) {
            if (connection->head.size() > MaxHeaderSize) {
                respond(connection, 400);
            }
            return;
        }
        data = connection->head.mid(end + 4);
        connection->head.truncate(end);
        if (!parseHead(connection)) {
            return; // parseHead has answered already.
        }
    }

    feed(connection, data);
}

bool ShareServer::parseHead(Connection *connection)
{
    connection->headerParsed = true;

    const QList<QByteArray> lines = connection->head.split('\n');
    if (lines.isEmpty()) {
        respond(connection, 400);
        return false;
    }

    const QList<QByteArray> request = lines.first().trimmed().split(' ');
    if (request.size() < 2) {
        respond(connection, 400);
        return false;
    }
    connection->method = QString::fromLatin1(request.at(0)).toUpper();
    const QUrl url = QUrl::fromEncoded(request.at(1));
    connection->path = url.path();
    connection->query = QUrlQuery(url);

    for (qsizetype i = 1; i < lines.size(); ++i) {
        const QByteArray line = lines.at(i).trimmed();
        const int colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }
        const QByteArray key = line.left(colon).trimmed().toLower();
        const QByteArray value = line.mid(colon + 1).trimmed();
        if (key == "content-length") {
            connection->contentLength = value.toLongLong();
        } else if (key == "transfer-encoding" && value.toLower().contains("chunked")) {
            connection->chunked = true;
            connection->contentLength = -1;
        }
    }

    // From here on the header buffer doubles as the place a chunk size line
    // waits when it arrives split across two packets.
    connection->head.clear();

    if (connection->path == ApiPrefix + u"upload"_s) {
        return beginUpload(connection);
    }

    if (!connection->chunked && connection->contentLength > MaxBodySize) {
        respond(connection, 413);
        return false;
    }

    // A route with nothing to wait for can be answered right away.
    if (!connection->chunked && connection->contentLength <= 0) {
        connection->contentLength = 0;
        routeWithBody(connection);
        return false;
    }
    return true;
}

void ShareServer::feed(Connection *connection, QByteArray data)
{
    const auto sink = [this, connection](const QByteArray &chunk) {
        if (chunk.isEmpty()) {
            return;
        }
        if (!connection->toFile) {
            connection->body.append(chunk);
            return;
        }
        if (connection->file && connection->file->write(chunk) < 0) {
            qWarning("atoll: cannot write to %s (%s)",
                     qUtf8Printable(connection->file->fileName()),
                     qUtf8Printable(connection->file->errorString()));
        }
        connection->stored += chunk.size();
        m_session.received = qMin(m_session.total, m_session.received + chunk.size());
        Q_EMIT receiveProgress(m_session.received, m_session.total);
    };

    while (!data.isEmpty()) {
        if (connection->chunked) {
            if (connection->chunkRemaining < 0) {
                const int end = data.indexOf("\r\n");
                if (end < 0) {
                    connection->head.append(data); // Hold the partial size line.
                    return;
                }
                QByteArray sizeLine = connection->head + data.left(end);
                connection->head.clear();
                data = data.mid(end + 2);
                const int extension = sizeLine.indexOf(';');
                if (extension >= 0) {
                    sizeLine.truncate(extension);
                }
                bool ok = false;
                connection->chunkRemaining = sizeLine.trimmed().toLongLong(&ok, 16);
                if (!ok || connection->chunkRemaining < 0) {
                    respond(connection, 400);
                    return;
                }
                if (connection->chunkRemaining == 0) {
                    if (connection->toFile) {
                        finishUpload(connection);
                    } else {
                        routeWithBody(connection);
                    }
                    return;
                }
            }
            const qint64 take = qMin<qint64>(connection->chunkRemaining, data.size());
            sink(data.left(take));
            connection->chunkRemaining -= take;
            data = data.mid(take);
            if (connection->chunkRemaining == 0) {
                connection->chunkRemaining = -1;
                // Skip the CRLF that closes the chunk, whenever it turns up.
                if (data.startsWith("\r\n")) {
                    data = data.mid(2);
                } else if (data.startsWith("\n")) {
                    data = data.mid(1);
                }
            }
            continue;
        }

        const qint64 take = qMin<qint64>(connection->contentLength, data.size());
        sink(data.left(take));
        connection->contentLength -= take;
        data = data.mid(take);
        if (connection->contentLength <= 0) {
            if (connection->toFile) {
                finishUpload(connection);
            } else {
                routeWithBody(connection);
            }
            return;
        }
    }
}

bool ShareServer::beginUpload(Connection *connection)
{
    if (!m_sessionActive) {
        respond(connection, 409);
        return false;
    }

    const QString sessionId = connection->query.queryItemValue(u"sessionId"_s);
    const QString fileId = connection->query.queryItemValue(u"fileId"_s);
    const QString fileToken = connection->query.queryItemValue(u"token"_s);
    if (sessionId.isEmpty() || fileId.isEmpty() || fileToken.isEmpty()) {
        respond(connection, 400);
        return false;
    }

    const auto entry = m_session.files.constFind(fileId);
    if (sessionId != m_session.id || entry == m_session.files.constEnd() || entry->token != fileToken
        || shareAddressString(connection->socket->peerAddress()) != m_session.peer.address) {
        respond(connection, 403);
        return false;
    }
    if (entry->done) {
        respond(connection, 200);
        return false;
    }

    const QString path = reserve(entry->name);
    auto *file = new QFile(path);
    if (!file->open(QIODevice::WriteOnly)) {
        qWarning("atoll: cannot create %s (%s)", qUtf8Printable(path), qUtf8Printable(file->errorString()));
        delete file;
        respond(connection, 500);
        return false;
    }

    connection->toFile = true;
    connection->file = file;
    connection->fileId = fileId;
    connection->expected = entry->size;
    return true;
}

void ShareServer::finishUpload(Connection *connection)
{
    const QString path = connection->file ? connection->file->fileName() : QString();
    if (connection->file) {
        connection->file->close();
        delete connection->file;
        connection->file = nullptr;
    }
    connection->toFile = false;

    const auto entry = m_session.files.find(connection->fileId);
    if (entry == m_session.files.end()) {
        respond(connection, 403);
        return;
    }

    if (connection->expected > 0 && connection->stored != connection->expected) {
        qWarning("atoll: %s arrived as %lld bytes, %lld were announced",
                 qUtf8Printable(entry->name), connection->stored, connection->expected);
    }

    entry->done = true;
    m_session.saved.append(path);
    respond(connection, 200);

    const bool complete = std::all_of(m_session.files.cbegin(), m_session.files.cend(),
                                      [](const IncomingFile &file) {
                                          return file.done;
                                      });
    if (complete) {
        const QStringList saved = m_session.saved;
        clearSession();
        Q_EMIT receiveFinished(saved);
    }
}

void ShareServer::routeWithBody(Connection *connection)
{
    const QJsonObject payload = QJsonDocument::fromJson(connection->body).object();
    const QString address = shareAddressString(connection->socket->peerAddress());

    if (connection->path == ApiPrefix + u"info"_s) {
        respond(connection, 200, QJsonDocument(m_identity.toJson()).toJson(QJsonDocument::Compact));
        return;
    }

    if (connection->path == ApiPrefix + u"register"_s) {
        const SharePeer peer = SharePeer::fromJson(payload, address);
        if (peer.isValid() && peer.fingerprint != m_identity.fingerprint) {
            Q_EMIT peerSeen(peer);
        }
        respond(connection, 200, QJsonDocument(m_identity.toJson()).toJson(QJsonDocument::Compact));
        return;
    }

    if (connection->path == ApiPrefix + u"cancel"_s) {
        if (connection->query.queryItemValue(u"sessionId"_s) == m_session.id && isBusy()) {
            clearSession();
            Q_EMIT receiveFailed(tr("The sender cancelled the transfer."));
        }
        respond(connection, 200);
        return;
    }

    if (connection->path == ApiPrefix + u"prepare-upload"_s) {
        if (connection->method != u"POST"_s) {
            respond(connection, 400);
            return;
        }
        if (m_destination.isEmpty() || !QDir().mkpath(m_destination)) {
            respond(connection, 500);
            return;
        }
        if (isBusy()) {
            respond(connection, 409);
            return;
        }

        const SharePeer peer = SharePeer::fromJson(payload.value(u"info"_s).toObject(), address);
        const QJsonObject files = payload.value(u"files"_s).toObject();
        if (!peer.isValid() || files.isEmpty()) {
            respond(connection, files.isEmpty() ? 204 : 400);
            return;
        }

        Session session;
        session.id = token();
        session.peer = peer;
        QVariantList summary;
        for (auto it = files.constBegin(); it != files.constEnd(); ++it) {
            const QJsonObject entry = it.value().toObject();
            IncomingFile file;
            file.id = entry.value(u"id"_s).toString(it.key());
            file.token = token();
            file.name = sanitise(entry.value(u"fileName"_s).toString(file.id));
            file.size = qint64(entry.value(u"size"_s).toDouble());
            session.files.insert(file.id, file);
            session.total += qMax<qint64>(0, file.size);
            summary.append(QVariantMap{{u"name"_s, file.name}, {u"size"_s, file.size}});
        }

        m_session = session;
        m_sessionActive = false;

        if (m_autoAccept) {
            m_approvalSocket = connection->socket;
            resolveApproval(true);
            return;
        }

        connection->deferred = true;
        m_approvalSocket = connection->socket;
        m_approvalTimer.start();
        Q_EMIT approvalRequested(session.peer, summary, session.total);
        return;
    }

    respond(connection, 404);
}

void ShareServer::resolveApproval(bool accepted)
{
    m_approvalTimer.stop();

    QTcpSocket *socket = m_approvalSocket;
    m_approvalSocket = nullptr;
    Connection *connection = socket ? m_connections.value(socket) : nullptr;
    if (!connection) {
        clearSession();
        return;
    }
    connection->deferred = false;

    if (!accepted) {
        clearSession();
        respond(connection, 403);
        return;
    }

    QJsonObject tokens;
    for (const IncomingFile &file : std::as_const(m_session.files)) {
        tokens.insert(file.id, file.token);
    }
    const QJsonObject reply{
        {u"sessionId"_s, m_session.id},
        {u"files"_s, tokens},
    };

    m_sessionActive = true;
    m_session.received = 0;
    Q_EMIT receiveProgress(0, m_session.total);
    respond(connection, 200, QJsonDocument(reply).toJson(QJsonDocument::Compact));
}

void ShareServer::abortSession()
{
    if (hasPendingApproval()) {
        resolveApproval(false);
        return;
    }
    clearSession();
}

void ShareServer::clearSession()
{
    m_approvalTimer.stop();
    m_session = Session();
    m_sessionActive = false;
}

QString ShareServer::reserve(const QString &name) const
{
    QDir directory(m_destination);
    const QString relative = sanitise(name);
    const QFileInfo info(directory.filePath(relative));
    if (!info.absolutePath().isEmpty()) {
        QDir().mkpath(info.absolutePath());
    }

    QString candidate = info.absoluteFilePath();
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : u"."_s + info.suffix();
    for (int i = 2; QFile::exists(candidate) && i < 1000; ++i) {
        candidate = info.absolutePath() + u'/' + base + u" ("_s + QString::number(i) + u')' + suffix;
    }
    return candidate;
}

void ShareServer::respond(Connection *connection, int status, const QByteArray &body,
                          const QByteArray &contentType)
{
    if (connection->responded) {
        return;
    }
    connection->responded = true;

    QByteArray head = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason(status) + "\r\n";
    head += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    if (!body.isEmpty()) {
        head += "Content-Type: " + contentType + "\r\n";
    }
    head += "Connection: close\r\n\r\n";

    connection->socket->write(head);
    if (!body.isEmpty()) {
        connection->socket->write(body);
    }
    connection->socket->flush();
    connection->socket->disconnectFromHost();
}

void ShareServer::dropConnection(Connection *connection)
{
    QTcpSocket *socket = connection->socket;
    if (m_connections.remove(socket) == 0) {
        return; // Already on its way out.
    }
    socket->disconnect(this);

    if (connection->file) {
        // A half-written file is not a download; it is litter.
        const QString path = connection->file->fileName();
        connection->file->close();
        delete connection->file;
        connection->file = nullptr;
        QFile::remove(path);
        if (m_sessionActive) {
            clearSession();
            Q_EMIT receiveFailed(tr("The transfer was interrupted."));
        }
    }

    if (m_approvalSocket == socket) {
        m_approvalSocket = nullptr;
        clearSession();
        Q_EMIT receiveFailed(tr("The sender gave up waiting."));
    }

    socket->close();
    socket->deleteLater();
    delete connection;
}
