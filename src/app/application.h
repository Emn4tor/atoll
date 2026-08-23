/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

// Every backend below is exposed as a Q_PROPERTY, and the meta type system
// needs the complete types, not forward declarations.
#include "app/shellwindow.h"
#include "dbus/dbusmonitor.h"
#include "config/config.h"
#include "dbus/mprismanager.h"
#include "dbus/notificationmodel.h"
#include "dbus/osdmonitor.h"
#include "ipc/ipcservice.h"
#include "system/battery.h"
#include "system/clock.h"
#include "system/visualizer.h"

class DBusMonitor;
class NotificationMonitor;
class QQmlEngine;
class QJSEngine;

/**
 * The one object QML talks to. It owns every backend, wires the session-bus
 * tap to the pieces that care about it, and exposes the handful of side
 * effects the UI is allowed to cause.
 */
class Application : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON

    Q_PROPERTY(Config *config READ config CONSTANT)
    Q_PROPERTY(ShellWindow *shell READ shell CONSTANT)
    Q_PROPERTY(OsdMonitor *osd READ osd CONSTANT)
    Q_PROPERTY(NotificationModel *notifications READ notifications CONSTANT)
    Q_PROPERTY(MprisManager *media READ media CONSTANT)
    Q_PROPERTY(Battery *battery READ battery CONSTANT)
    Q_PROPERTY(Clock *clock READ clock CONSTANT)
    Q_PROPERTY(Visualizer *visualizer READ visualizer CONSTANT)
    Q_PROPERTY(IpcService *ipc READ ipc CONSTANT)

    Q_PROPERTY(QString version READ version CONSTANT)
    /** ATOLL_DEBUG_SURFACE=1 paints the whole layer surface, to see its bounds. */
    Q_PROPERTY(bool debugSurface READ debugSurface CONSTANT)
    /** ATOLL_DEBUG_STATE=1 logs every state change the island goes through. */
    Q_PROPERTY(bool debugState READ debugState CONSTANT)
    /** False when the bus tap could not be established (see busError). */
    Q_PROPERTY(bool busTapActive READ busTapActive NOTIFY busTapChanged)
    Q_PROPERTY(QString busError READ busError NOTIFY busTapChanged)

public:
    static Application *instance();

    /**
     * QML singleton factory. Note that Application deliberately has no default
     * constructor: with one, the QML engine happily builds a *second*
     * Application of its own instead of calling this, and the UI then talks to
     * a backend that was never started - no bus tap, no D-Bus control, and no
     * obvious symptom beyond features quietly doing nothing.
     */
    static Application *create(QQmlEngine *engine, QJSEngine *scriptEngine);

    explicit Application(QObject *parent);
    ~Application() override;

    void start();

    Config *config() const
    {
        return m_config;
    }
    ShellWindow *shell() const
    {
        return m_shell;
    }
    OsdMonitor *osd() const
    {
        return m_osd;
    }
    NotificationModel *notifications() const
    {
        return m_notifications;
    }
    MprisManager *media() const
    {
        return m_media;
    }
    Battery *battery() const
    {
        return m_battery;
    }
    Clock *clock() const
    {
        return m_clock;
    }
    Visualizer *visualizer() const
    {
        return m_visualizer;
    }
    IpcService *ipc() const
    {
        return m_ipc;
    }

    QString version() const;
    bool debugSurface() const;
    bool debugState() const;
    bool busTapActive() const
    {
        return m_busTapActive;
    }
    QString busError() const
    {
        return m_busError;
    }

    /** Nudge the default sink and echo the result through the island's OSD. */
    Q_INVOKABLE void adjustVolume(int deltaPercent);
    Q_INVOKABLE void toggleMute();
    /** Launch a desktop entry (used when a notification is clicked). */
    Q_INVOKABLE void activateApp(const QString &desktopEntry);
    Q_INVOKABLE void openUrl(const QString &url);

Q_SIGNALS:
    void busTapChanged();

private:
    void onBusMessage(const DBusMessageInfo &message);
    void reportVolume();

    Config *m_config = nullptr;
    ShellWindow *m_shell = nullptr;
    DBusMonitor *m_monitor = nullptr;
    OsdMonitor *m_osd = nullptr;
    NotificationMonitor *m_notificationMonitor = nullptr;
    NotificationModel *m_notifications = nullptr;
    MprisManager *m_media = nullptr;
    Battery *m_battery = nullptr;
    Clock *m_clock = nullptr;
    Visualizer *m_visualizer = nullptr;
    IpcService *m_ipc = nullptr;

    bool m_busTapActive = false;
    QString m_busError;
};
