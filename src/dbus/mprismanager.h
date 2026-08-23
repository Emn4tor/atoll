/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Q_PROPERTY exposes MprisPlayer * to the meta type system, which needs the
// complete type rather than a forward declaration.
#include "mprisplayer.h"

class Config;

/**
 * Tracks every MPRIS2 player on the bus and decides which one the island
 * should speak for.
 *
 * The pick is deliberately sticky: whatever is actually playing wins, and a
 * player the user pinned by hand wins over that, so the island does not jump
 * between a browser tab and a music app mid-song.
 */
class MprisManager : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.media")

    Q_PROPERTY(MprisPlayer *active READ active NOTIFY activeChanged)
    Q_PROPERTY(int count READ count NOTIFY playersChanged)
    Q_PROPERTY(QVariantList players READ players NOTIFY playersChanged)
    Q_PROPERTY(bool anyPlaying READ anyPlaying NOTIFY activeChanged)

public:
    explicit MprisManager(Config *config, QObject *parent = nullptr);

    MprisPlayer *active() const
    {
        return m_active;
    }
    int count() const
    {
        return int(m_players.size());
    }
    QVariantList players() const;
    bool anyPlaying() const;

    /** Pin the island to one player; pass an empty string to unpin. */
    Q_INVOKABLE void pin(const QString &service);
    Q_INVOKABLE void cycle();

Q_SIGNALS:
    void activeChanged();
    void playersChanged();
    /** The active player started playing a different track. */
    void trackChanged();

private Q_SLOTS:
    void onNameOwnerChanged(const QString &name, const QString &oldOwner, const QString &newOwner);

private:
    void scanExistingPlayers();
    void addPlayer(const QString &service);
    void removePlayer(const QString &service);
    void reevaluateActive();
    bool isBlocked(const QString &service) const;

    Config *m_config;
    QList<MprisPlayer *> m_players;
    MprisPlayer *m_active = nullptr;
    QString m_pinned;
};
