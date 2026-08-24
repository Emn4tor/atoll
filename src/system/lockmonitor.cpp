/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lockmonitor.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView Service{"org.freedesktop.ScreenSaver"};
constexpr QLatin1StringView Path{"/org/freedesktop/ScreenSaver"};
constexpr QLatin1StringView Interface{"org.freedesktop.ScreenSaver"};
}

LockMonitor::LockMonitor(QObject *parent)
    : QObject(parent)
{
    auto bus = QDBusConnection::sessionBus();
    // Plasma exports the screensaver on two paths; the shorter one is what
    // kscreenlocker actually signals on, so both are worth listening to.
    for (const QString &path : {QString(Path), u"/ScreenSaver"_s}) {
        bus.connect(QString(Service),
                    path,
                    QString(Interface),
                    u"ActiveChanged"_s,
                    this,
                    SLOT(onActiveChanged(bool)));
    }

    queryInitialState();
}

void LockMonitor::onActiveChanged(bool active)
{
    setLocked(active);
}

void LockMonitor::queryInitialState()
{
    auto message = QDBusMessage::createMethodCall(QString(Service), QString(Path), QString(Interface), u"GetActive"_s);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<bool> reply = *self;
        if (!reply.isError()) {
            setLocked(reply.value());
        }
    });
}

void LockMonitor::setLocked(bool locked)
{
    if (m_locked == locked) {
        return;
    }
    m_locked = locked;
    if (qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0) {
        qWarning("atoll: session %s", locked ? "locked" : "unlocked");
    }
    Q_EMIT lockedChanged();
}
