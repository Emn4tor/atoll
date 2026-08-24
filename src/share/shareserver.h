/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "share/sharecredentials.h"
#include "share/sharetypes.h"

#include <QHash>
#include <QMap>
#include <QPointer>
#include <QStringList>
#include <QTcpServer>
#include <QTimer>
#include <QUrlQuery>
#include <QVariantList>

class QFile;
class QTcpSocket;

/**
 * The receiving half of LocalSend: a small HTTP server that answers the five
 * routes the protocol defines.
 *
 * It is deliberately hand-rolled rather than pulled in from a framework - the
 * protocol is five endpoints, and the only one that needs care is the upload,
 * whose body is written straight to disk as it arrives instead of being held
 * in memory.
 *
 * One transfer at a time: a notch is a bad place to arbitrate between two
 * strangers sending files at once, and the protocol has a status code (409)
 * that says exactly this.
 */
class ShareServer : public QTcpServer
{
    Q_OBJECT

public:
    explicit ShareServer(QObject *parent = nullptr);

    /**
     * Bind `preferred`, or the next free port after it when something else -
     * typically the LocalSend app itself - already holds it. Returns the port
     * that was bound, or 0.
     */
    quint16 listenOn(quint16 preferred, int attempts = 8);

    void setIdentity(const ShareIdentity &identity);
    /** Serve TLS with these. Without them the server stays cleartext. */
    void setCredentials(const ShareCredentials &credentials);
    void setDestination(const QString &directory);
    void setAutoAccept(bool automatic);

    bool isBusy() const;
    bool hasPendingApproval() const;
    /** Answer the request the island is currently showing. */
    void resolveApproval(bool accepted);
    /** Drop whatever is in flight, deleting the half-written file. */
    void abortSession();

Q_SIGNALS:
    void peerSeen(const SharePeer &peer);
    void approvalRequested(const SharePeer &peer, const QVariantList &files, qint64 totalBytes);
    void receiveProgress(qint64 received, qint64 total);
    void receiveFinished(const QStringList &paths);
    void receiveFailed(const QString &reason);

protected:
    void incomingConnection(qintptr handle) override;

private:
    struct Connection {
        QTcpSocket *socket = nullptr;
        QByteArray head;
        bool headerParsed = false;
        QString method;
        QString path;
        QUrlQuery query;
        qint64 contentLength = -1;
        bool chunked = false;
        qint64 chunkRemaining = -1;
        QByteArray body;
        bool toFile = false;
        QFile *file = nullptr;
        QString fileId;
        qint64 expected = 0;
        qint64 stored = 0;
        bool responded = false;
        bool deferred = false;
    };

    struct IncomingFile {
        QString id;
        QString token;
        QString name;
        qint64 size = 0;
        bool done = false;
    };

    struct Session {
        QString id;
        SharePeer peer;
        QMap<QString, IncomingFile> files;
        qint64 total = 0;
        qint64 received = 0;
        QStringList saved;
        bool accepted = false;
    };

    void onReadyRead(Connection *connection);
    bool parseHead(Connection *connection);
    void feed(Connection *connection, QByteArray data);
    void routeWithBody(Connection *connection);
    bool beginUpload(Connection *connection);
    void finishUpload(Connection *connection);
    void respond(Connection *connection, int status, const QByteArray &body = {},
                 const QByteArray &contentType = QByteArrayLiteral("application/json"));
    void dropConnection(Connection *connection);
    void clearSession();
    QString reserve(const QString &name) const;

    ShareIdentity m_identity;
    ShareCredentials m_credentials;
    QString m_destination;
    bool m_autoAccept = false;

    QHash<QTcpSocket *, Connection *> m_connections;
    Session m_session;
    bool m_sessionActive = false;
    QPointer<QTcpSocket> m_approvalSocket;
    QTimer m_approvalTimer;
};
