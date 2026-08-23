/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "mprismanager.h"

#include "config/config.h"
#include "mprisplayer.h"

#include <QDBusConnection>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusMessage>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView MprisPrefix{"org.mpris.MediaPlayer2."};
}

MprisManager::MprisManager(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    QDBusConnection::sessionBus().connect(u"org.freedesktop.DBus"_s,
                                          u"/org/freedesktop/DBus"_s,
                                          u"org.freedesktop.DBus"_s,
                                          u"NameOwnerChanged"_s,
                                          this,
                                          SLOT(onNameOwnerChanged(QString, QString, QString)));
    scanExistingPlayers();

    connect(m_config, &Config::changed, this, &MprisManager::reevaluateActive);
}

void MprisManager::scanExistingPlayers()
{
    auto message = QDBusMessage::createMethodCall(u"org.freedesktop.DBus"_s,
                                                  u"/org/freedesktop/DBus"_s,
                                                  u"org.freedesktop.DBus"_s,
                                                  u"ListNames"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QStringList> reply = *self;
        if (reply.isError()) {
            return;
        }
        for (const QString &name : reply.value()) {
            if (name.startsWith(MprisPrefix)) {
                addPlayer(name);
            }
        }
    });
}

void MprisManager::onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner)
{
    if (!name.startsWith(MprisPrefix)) {
        return;
    }
    if (oldOwner.isEmpty() && !newOwner.isEmpty()) {
        addPlayer(name);
    } else if (!oldOwner.isEmpty() && newOwner.isEmpty()) {
        removePlayer(name);
    }
}

bool MprisManager::isBlocked(const QString &service) const
{
    const QStringList blocked = m_config->value(u"media.blocked"_s).toStringList();
    for (const QString &pattern : blocked) {
        if (service.contains(pattern, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

void MprisManager::addPlayer(const QString &service)
{
    if (isBlocked(service)) {
        return;
    }
    for (const MprisPlayer *player : std::as_const(m_players)) {
        if (player->service() == service) {
            return;
        }
    }

    auto *player = new MprisPlayer(service, this);
    m_players.append(player);

    connect(player, &MprisPlayer::playbackChanged, this, &MprisManager::reevaluateActive);
    connect(player, &MprisPlayer::identityChanged, this, &MprisManager::playersChanged);
    connect(player, &MprisPlayer::metadataChanged, this, [this, player] {
        if (player == m_active) {
            Q_EMIT trackChanged();
        }
    });

    Q_EMIT playersChanged();
    reevaluateActive();
}

void MprisManager::removePlayer(const QString &service)
{
    for (int i = 0; i < m_players.size(); ++i) {
        if (m_players.at(i)->service() != service) {
            continue;
        }
        MprisPlayer *player = m_players.takeAt(i);
        if (m_active == player) {
            m_active = nullptr;
        }
        if (m_pinned == service) {
            m_pinned.clear();
        }
        player->deleteLater();
        Q_EMIT playersChanged();
        reevaluateActive();
        return;
    }
}

void MprisManager::reevaluateActive()
{
    MprisPlayer *chosen = nullptr;

    if (!m_pinned.isEmpty()) {
        for (MprisPlayer *player : std::as_const(m_players)) {
            if (player->service() == m_pinned) {
                chosen = player;
                break;
            }
        }
    }

    if (!chosen) {
        // A configured preference wins, in the order the user listed it.
        const QStringList preferred = m_config->value(u"media.preferred"_s).toStringList();
        for (const QString &pattern : preferred) {
            for (MprisPlayer *player : std::as_const(m_players)) {
                if (player->service().contains(pattern, Qt::CaseInsensitive)
                    || player->identity().contains(pattern, Qt::CaseInsensitive)) {
                    chosen = player;
                    break;
                }
            }
            if (chosen) {
                break;
            }
        }
    }

    if (!chosen) {
        // Otherwise: whatever is playing, most recently started first.
        for (MprisPlayer *player : std::as_const(m_players)) {
            if (!player->playing()) {
                continue;
            }
            if (!chosen || player->lastPlayedAt() > chosen->lastPlayedAt()) {
                chosen = player;
            }
        }
    }

    if (!chosen && m_active && m_players.contains(m_active)) {
        chosen = m_active; // Keep showing a paused player rather than nothing.
    }
    if (!chosen && !m_players.isEmpty()) {
        chosen = m_players.first();
    }

    if (chosen != m_active) {
        m_active = chosen;
        Q_EMIT activeChanged();
        Q_EMIT trackChanged();
    } else {
        Q_EMIT activeChanged(); // playing state may have flipped
    }
}

bool MprisManager::anyPlaying() const
{
    for (const MprisPlayer *player : m_players) {
        if (player->playing()) {
            return true;
        }
    }
    return false;
}

QVariantList MprisManager::players() const
{
    QVariantList list;
    list.reserve(m_players.size());
    for (const MprisPlayer *player : m_players) {
        list.append(QVariantMap{
            {u"service"_s, player->service()},
            {u"identity"_s, player->identity()},
            {u"icon"_s, player->iconName()},
            {u"playing"_s, player->playing()},
            {u"active"_s, player == m_active},
        });
    }
    return list;
}

void MprisManager::pin(const QString &service)
{
    m_pinned = service;
    reevaluateActive();
}

void MprisManager::cycle()
{
    if (m_players.size() < 2) {
        return;
    }
    const int current = m_active ? int(m_players.indexOf(m_active)) : -1;
    const int next = (current + 1) % int(m_players.size());
    pin(m_players.at(next)->service());
}
