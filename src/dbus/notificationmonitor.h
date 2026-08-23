/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QColor>
#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QStringList>

#include "dbusmonitor.h"

class Config;

/** A single notification as it appeared on the bus. */
struct NotificationData {
    quint64 uid = 0;
    quint32 daemonId = 0;
    QString appName;
    QString appIcon;
    QString summary;
    QString body;
    QString desktopEntry;
    QString category;
    QString imageUrl;
    QStringList actions; //!< flat [key, label, key, label, ...]
    int urgency = 1; //!< 0 low, 1 normal, 2 critical
    int expireTimeout = -1;
    int progress = -1; //!< the "value" hint, -1 when absent
    bool transient = false;
    bool resident = false;
    QColor accent;
    QDateTime timestamp;
};

/**
 * Reads notifications off the session bus without becoming the notification
 * server, so the user keeps whatever daemon they already run (Plasma's own,
 * dunst, ...) and Atoll simply mirrors it.
 *
 * The id a daemon hands back to the sending app is recovered by pairing each
 * observed `Notify` call with its reply, which is what makes closing a
 * notification from the island possible at all.
 */
class NotificationMonitor : public QObject
{
    Q_OBJECT

public:
    explicit NotificationMonitor(Config *config, QObject *parent = nullptr);

    bool handleMessage(const DBusMessageInfo &message);
    static QStringList matchRules(bool trackIds);

Q_SIGNALS:
    void posted(const NotificationData &notification);
    void idAssigned(quint64 uid, quint32 daemonId);
    void closed(quint32 daemonId, int reason);
    void actionInvoked(quint32 daemonId, const QString &actionKey);

private:
    struct PendingCall {
        QString caller;
        quint64 uid;
    };

    QString extractImage(const QVariantMap &hints, quint64 uid, QColor *accent) const;

    Config *m_config;
    quint64 m_nextUid = 1;
    QHash<quint32, PendingCall> m_pending; //!< serial -> call
};
