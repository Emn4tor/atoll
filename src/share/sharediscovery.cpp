/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "share/sharediscovery.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QUdpSocket>

using namespace Qt::StringLiterals;

ShareDiscovery::ShareDiscovery(QObject *parent)
    : QObject(parent)
{
}

bool ShareDiscovery::isRunning() const
{
    return m_socket != nullptr;
}

void ShareDiscovery::setIdentity(const ShareIdentity &identity)
{
    m_identity = identity;
}

QList<QNetworkInterface> ShareDiscovery::multicastInterfaces()
{
    QList<QNetworkInterface> usable;
    const auto all = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : all) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp) || !flags.testFlag(QNetworkInterface::IsRunning)
            || !flags.testFlag(QNetworkInterface::CanMulticast) || flags.testFlag(QNetworkInterface::IsLoopBack)) {
            continue;
        }
        // An interface without an IPv4 address cannot carry this group.
        const auto entries = interface.addressEntries();
        for (const QNetworkAddressEntry &entry : entries) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                usable.append(interface);
                break;
            }
        }
    }
    return usable;
}

void ShareDiscovery::refreshLocalAddresses()
{
    m_localAddresses.clear();
    const auto addresses = QNetworkInterface::allAddresses();
    for (const QHostAddress &address : addresses) {
        m_localAddresses.insert(address.toString());
    }
}

bool ShareDiscovery::start(const QHostAddress &group, quint16 port)
{
    stop();

    m_group = group;
    m_port = port;
    refreshLocalAddresses();

    m_socket = new QUdpSocket(this);
    // Sharing the port is what lets Atoll and the LocalSend app coexist.
    if (!m_socket->bind(QHostAddress::AnyIPv4, port,
                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qWarning("atoll: cannot bind the discovery port %u (%s); nearby devices will not be listed",
                 port, qUtf8Printable(m_socket->errorString()));
        delete m_socket;
        m_socket = nullptr;
        return false;
    }

    m_socket->setSocketOption(QAbstractSocket::MulticastTtlOption, 1);
    // Loopback on: another sharing app on this very machine is a valid peer,
    // and it is also the only way to try any of this without a second device.
    m_socket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

    int joined = 0;
    const auto interfaces = multicastInterfaces();
    for (const QNetworkInterface &interface : interfaces) {
        if (m_socket->joinMulticastGroup(group, interface)) {
            ++joined;
        }
    }
    if (joined == 0 && !m_socket->joinMulticastGroup(group)) {
        qWarning("atoll: no interface would join %s", qUtf8Printable(group.toString()));
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &ShareDiscovery::readPending);
    return true;
}

void ShareDiscovery::stop()
{
    if (!m_socket) {
        return;
    }
    delete m_socket;
    m_socket = nullptr;
}

void ShareDiscovery::announce(bool wantReply)
{
    if (!m_socket) {
        return;
    }

    const QJsonObject payload = m_identity.toJson(true, wantReply);
    const QByteArray datagram = QJsonDocument(payload).toJson(QJsonDocument::Compact);

    refreshLocalAddresses();

    // One write per interface: with several networks up, a single write only
    // ever reaches whichever one the routing table happens to prefer.
    const auto interfaces = multicastInterfaces();
    if (interfaces.isEmpty()) {
        m_socket->writeDatagram(datagram, m_group, m_port);
        return;
    }
    for (const QNetworkInterface &interface : interfaces) {
        m_socket->setMulticastInterface(interface);
        m_socket->writeDatagram(datagram, m_group, m_port);
    }
}

void ShareDiscovery::readPending()
{
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        handle(datagram.data(), datagram.senderAddress());
    }
}

void ShareDiscovery::handle(const QByteArray &payload, const QHostAddress &from)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }

    const QJsonObject object = document.object();
    const SharePeer peer = SharePeer::fromJson(object, shareAddressString(from));
    if (!peer.isValid() || peer.fingerprint == m_identity.fingerprint) {
        return; // Our own announcement, bounced back by the loopback option.
    }

    Q_EMIT peerSeen(peer);

    if (object.value(u"announce"_s).toBool()) {
        Q_EMIT replyWanted(peer);
    }
}
