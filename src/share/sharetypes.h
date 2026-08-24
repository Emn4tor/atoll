/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDateTime>
#include <QHostAddress>
#include <QJsonObject>
#include <QString>
#include <QVariantMap>

/**
 * The vocabulary of the LocalSend protocol, which is what Atoll speaks when
 * files are dropped on the island.
 *
 * AirDrop itself is not on the table: it rides on AWDL, Apple's own link layer,
 * and the open re-implementations of it need a wifi card that can be put into
 * monitor mode and have frames injected into it - which takes the card off the
 * network it was on. LocalSend is the protocol that is actually documented,
 * already spoken by an app on every platform, and works over ordinary wifi.
 */

/** Protocol version Atoll announces itself as. */
inline constexpr auto ShareProtocolVersion = "2.1";

/**
 * The address of a peer as this protocol wants it written: a dual-stack socket
 * hands out IPv4 peers as ::ffff:1.2.3.4, which is the same machine by a name
 * no other implementation will recognise - and no URL will resolve.
 */
inline QString shareAddressString(const QHostAddress &address)
{
    bool mapped = false;
    const quint32 asIPv4 = address.toIPv4Address(&mapped);
    QString text = mapped ? QHostAddress(asIPv4).toString() : address.toString();
    const qsizetype scope = text.indexOf(u'%');
    if (scope > 0) {
        text.truncate(scope);
    }
    return text;
}

/** How Atoll describes itself to everyone else on the network. */
struct ShareIdentity {
    QString alias;
    QString fingerprint;
    QString deviceModel;
    QString deviceType = QStringLiteral("desktop");
    /** Atoll's own server is cleartext, so peers are told to talk http to it. */
    QString protocol = QStringLiteral("http");
    quint16 port = 53317;

    QJsonObject toJson(bool withAnnounce = false, bool announce = false) const
    {
        QJsonObject object{
            {QStringLiteral("alias"), alias},
            {QStringLiteral("version"), QString::fromLatin1(ShareProtocolVersion)},
            {QStringLiteral("deviceModel"), deviceModel},
            {QStringLiteral("deviceType"), deviceType},
            {QStringLiteral("fingerprint"), fingerprint},
            {QStringLiteral("port"), int(port)},
            {QStringLiteral("protocol"), protocol},
            {QStringLiteral("download"), false},
        };
        if (withAnnounce) {
            object.insert(QStringLiteral("announce"), announce);
        }
        return object;
    }
};

/** Somebody else on the network, as they described themselves. */
struct SharePeer {
    QString alias;
    QString fingerprint;
    QString deviceModel;
    QString deviceType = QStringLiteral("desktop");
    QString protocol = QStringLiteral("http");
    QString address;
    quint16 port = 53317;
    QDateTime seen;

    bool isValid() const
    {
        return !fingerprint.isEmpty() && !address.isEmpty();
    }

    QString baseUrl() const
    {
        // A literal IPv6 address only survives a URL in brackets.
        const QString host = address.contains(u':') ? u'[' + address + u']' : address;
        return QStringLiteral("%1://%2:%3/api/localsend/v2").arg(protocol, host).arg(port);
    }

    QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("alias"), alias},
            {QStringLiteral("fingerprint"), fingerprint},
            {QStringLiteral("deviceModel"), deviceModel},
            {QStringLiteral("deviceType"), deviceType},
            {QStringLiteral("address"), address},
            {QStringLiteral("port"), int(port)},
        };
    }

    /**
     * The `address` never comes from the body: a device announces which port to
     * talk to it on, but where it is, is where the packet came from.
     */
    static SharePeer fromJson(const QJsonObject &object, const QString &address)
    {
        SharePeer peer;
        peer.alias = object.value(QStringLiteral("alias")).toString();
        peer.fingerprint = object.value(QStringLiteral("fingerprint")).toString();
        peer.deviceModel = object.value(QStringLiteral("deviceModel")).toString();
        const QString type = object.value(QStringLiteral("deviceType")).toString();
        if (!type.isEmpty()) {
            peer.deviceType = type;
        }
        const QString protocol = object.value(QStringLiteral("protocol")).toString();
        if (protocol == QLatin1String("https") || protocol == QLatin1String("http")) {
            peer.protocol = protocol;
        }
        const int port = object.value(QStringLiteral("port")).toInt(53317);
        peer.port = quint16(port > 0 && port <= 65535 ? port : 53317);
        peer.address = address;
        peer.seen = QDateTime::currentDateTimeUtc();
        if (peer.alias.isEmpty()) {
            peer.alias = address;
        }
        return peer;
    }
};

/** One file queued for sending. */
struct ShareFile {
    QString id;
    QString path;
    /** Relative path when a whole folder was dropped, plain name otherwise. */
    QString name;
    QString mimeType;
    qint64 size = 0;
};
