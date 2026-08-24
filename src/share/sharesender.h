/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "share/sharecredentials.h"
#include "share/sharetypes.h"

#include <QHash>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

/**
 * One outgoing transfer: ask the other side whether it wants the files, then
 * push them up one by one.
 *
 * Peers usually speak HTTPS with a certificate they signed themselves, which
 * is the whole point - the fingerprint of that certificate *is* their identity
 * in this protocol, so the usual chain of trust has nothing to say here and the
 * verification is turned off deliberately rather than by accident.
 */
class ShareSender : public QObject
{
    Q_OBJECT

public:
    explicit ShareSender(QObject *parent = nullptr);
    ~ShareSender() override;

    void setIdentity(const ShareIdentity &identity);
    void setCredentials(const ShareCredentials &credentials);

    bool isActive() const
    {
        return m_active;
    }
    /** Name of the file currently going out. */
    QString currentFile() const;

    void send(const SharePeer &peer, const QList<ShareFile> &files);
    /** Stop, and tell the receiver to forget the session. */
    void cancel();

Q_SIGNALS:
    void progress(qint64 sent, qint64 total);
    void fileChanged();
    void finished();
    void failed(const QString &message);

private:
    void prepare();
    void uploadNext();
    void abandon(const QString &message);
    QNetworkRequest request(const QString &route) const;
    void watch(QNetworkReply *reply) const;

    QNetworkAccessManager *m_network = nullptr;
    ShareIdentity m_identity;
    ShareCredentials m_credentials;
    SharePeer m_peer;
    QList<ShareFile> m_files;
    QHash<QString, QString> m_tokens;
    QString m_sessionId;
    QPointer<QNetworkReply> m_reply;
    QFile *m_source = nullptr;
    int m_index = -1;
    qint64 m_total = 0;
    qint64 m_completed = 0;
    bool m_active = false;
};
