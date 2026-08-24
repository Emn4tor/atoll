/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "share/sharetypes.h"

#include <QHostAddress>
#include <QObject>
#include <QSet>

class QNetworkInterface;
class QUdpSocket;

/**
 * The LocalSend discovery half: one multicast group that everybody announces
 * themselves into.
 *
 * The socket is bound with ShareAddress, so the LocalSend app can be running on
 * this machine at the same time and both hear the same announcements.
 */
class ShareDiscovery : public QObject
{
    Q_OBJECT

public:
    explicit ShareDiscovery(QObject *parent = nullptr);

    /** Join the group. Returns false if the port could not be bound at all. */
    bool start(const QHostAddress &group, quint16 port);
    void stop();
    bool isRunning() const;

    void setIdentity(const ShareIdentity &identity);

    /**
     * Say hello on every interface. With `wantReply` the announcement asks
     * everyone who hears it to answer, which is what fills the device list in
     * the moment a file is dragged onto the island.
     */
    void announce(bool wantReply);

Q_SIGNALS:
    void peerSeen(const SharePeer &peer);
    /** A peer announced itself and is waiting to be told who we are. */
    void replyWanted(const SharePeer &peer);

private:
    void readPending();
    void handle(const QByteArray &payload, const QHostAddress &from);
    static QList<QNetworkInterface> multicastInterfaces();
    void refreshLocalAddresses();

    QUdpSocket *m_socket = nullptr;
    QHostAddress m_group;
    quint16 m_port = 53317;
    ShareIdentity m_identity;
    QSet<QString> m_localAddresses;
};
