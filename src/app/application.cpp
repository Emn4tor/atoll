/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "application.h"

#include "config/config.h"
#include "dbus/dbusmonitor.h"
#include "dbus/mprismanager.h"
#include "dbus/mprisplayer.h"
#include "dbus/notificationmodel.h"
#include "dbus/notificationmonitor.h"
#include "dbus/osdmonitor.h"
#include "ipc/ipcservice.h"
#include "shellwindow.h"
#include "system/battery.h"
#include "system/clock.h"
#include "system/visualizer.h"

#include <QDesktopServices>
#include <QProcess>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
Application *s_instance = nullptr;
}

Application *Application::instance()
{
    return s_instance;
}

Application *Application::create(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)
    Q_ASSERT(s_instance);
    QQmlEngine::setObjectOwnership(s_instance, QQmlEngine::CppOwnership);
    return s_instance;
}

Application::Application(QObject *parent)
    : QObject(parent)
{
    Q_ASSERT_X(!s_instance, "Application", "Atoll expects exactly one Application");
    s_instance = this;

    m_config = new Config(this);
    m_config->ensureUserFile();

    m_shell = new ShellWindow(m_config, this);
    m_osd = new OsdMonitor(m_config, this);
    m_notificationMonitor = new NotificationMonitor(m_config, this);
    m_notifications = new NotificationModel(m_config, this);
    m_media = new MprisManager(m_config, this);
    m_battery = new Battery(this);
    m_clock = new Clock(m_config, this);
    m_visualizer = new Visualizer(m_config, this);
    m_ipc = new IpcService(this);

    connect(m_notificationMonitor, &NotificationMonitor::posted, m_notifications, &NotificationModel::onPosted);
    connect(m_notificationMonitor, &NotificationMonitor::idAssigned, m_notifications, &NotificationModel::onIdAssigned);
    connect(m_notificationMonitor, &NotificationMonitor::closed, m_notifications, &NotificationModel::onClosed);

    connect(m_ipc, &IpcService::textRequested, m_osd, &OsdMonitor::showText);
    connect(m_ipc, &IpcService::progressRequested, m_osd, &OsdMonitor::showProgress);
    connect(m_ipc, &IpcService::reloadRequested, m_config, &Config::reload);

    // The spectrum only runs while something is actually playing.
    const auto syncVisualizer = [this] {
        MprisPlayer *player = m_media->active();
        m_visualizer->setActive(player && player->playing());
    };
    connect(m_media, &MprisManager::activeChanged, this, syncVisualizer);
}

Application::~Application()
{
    s_instance = nullptr;
}

QString Application::version() const
{
    return QStringLiteral(ATOLL_VERSION);
}

bool Application::debugSurface() const
{
    return qEnvironmentVariableIntValue("ATOLL_DEBUG_SURFACE") > 0;
}

bool Application::debugState() const
{
    return qEnvironmentVariableIntValue("ATOLL_DEBUG_STATE") > 0;
}

void Application::start()
{
    QStringList rules = OsdMonitor::matchRules();
    rules += NotificationMonitor::matchRules(m_config->value(u"notifications.trackIds"_s, true).toBool());

    m_monitor = new DBusMonitor(this);
    connect(m_monitor, &DBusMonitor::messageReceived, this, &Application::onBusMessage);
    connect(m_monitor, &DBusMonitor::failed, this, [this](const QString &reason) {
        m_busTapActive = false;
        m_busError = reason;
        qWarning("atoll: cannot observe the session bus (%s). OSD and notification "
                 "mirroring are disabled; media control still works.",
                 qUtf8Printable(reason));
        Q_EMIT busTapChanged();
    });

    if (m_monitor->start(rules)) {
        m_busTapActive = true;
        Q_EMIT busTapChanged();
    }

    m_ipc->registerOnBus();
}

void Application::onBusMessage(const DBusMessageInfo &message)
{
    if (m_osd->handleMessage(message)) {
        return;
    }
    m_notificationMonitor->handleMessage(message);
}

void Application::adjustVolume(int deltaPercent)
{
    const QString wpctl = QStandardPaths::findExecutable(u"wpctl"_s);
    if (!wpctl.isEmpty()) {
        const QString step = u"%1%%2"_s.arg(qAbs(deltaPercent)).arg(deltaPercent >= 0 ? u'+' : u'-');
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process] {
            process->deleteLater();
            reportVolume();
        });
        process->start(wpctl, {u"set-volume"_s, u"-l"_s, u"1.0"_s, u"@DEFAULT_AUDIO_SINK@"_s, step});
        return;
    }

    const QString pactl = QStandardPaths::findExecutable(u"pactl"_s);
    if (pactl.isEmpty()) {
        return;
    }
    const QString step = u"%1%2%"_s.arg(deltaPercent >= 0 ? u'+' : u'-').arg(qAbs(deltaPercent));
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        process->deleteLater();
        reportVolume();
    });
    process->start(pactl, {u"set-sink-volume"_s, u"@DEFAULT_SINK@"_s, step});
}

void Application::toggleMute()
{
    const QString wpctl = QStandardPaths::findExecutable(u"wpctl"_s);
    if (!wpctl.isEmpty()) {
        auto *process = new QProcess(this);
        connect(process, &QProcess::finished, this, [this, process] {
            process->deleteLater();
            reportVolume();
        });
        process->start(wpctl, {u"set-mute"_s, u"@DEFAULT_AUDIO_SINK@"_s, u"toggle"_s});
        return;
    }
    const QString pactl = QStandardPaths::findExecutable(u"pactl"_s);
    if (!pactl.isEmpty()) {
        QProcess::startDetached(pactl, {u"set-sink-mute"_s, u"@DEFAULT_SINK@"_s, u"toggle"_s});
    }
}

void Application::reportVolume()
{
    // Plasma only raises its OSD for changes it made itself, so when the island
    // drives the volume it also reports the result.
    const QString wpctl = QStandardPaths::findExecutable(u"wpctl"_s);
    if (wpctl.isEmpty()) {
        return;
    }
    auto *process = new QProcess(this);
    connect(process, &QProcess::finished, this, [this, process] {
        process->deleteLater();
        const QString output = QString::fromUtf8(process->readAllStandardOutput());
        // "Volume: 0.42" or "Volume: 0.42 [MUTED]"
        static const QRegularExpression pattern(u"Volume:\\s*([0-9.]+)"_s);
        const auto match = pattern.match(output);
        if (!match.hasMatch()) {
            return;
        }
        const bool muted = output.contains(u"MUTED"_s);
        const int percent = muted ? 0 : qRound(match.captured(1).toDouble() * 100.0);
        m_osd->showProgress(percent <= 0 ? u"audio-volume-muted"_s
                                         : (percent < 34 ? u"audio-volume-low"_s
                                                         : (percent < 67 ? u"audio-volume-medium"_s
                                                                         : u"audio-volume-high"_s)),
                            percent,
                            muted ? tr("Muted") : tr("Volume"));
    });
    process->start(wpctl, {u"get-volume"_s, u"@DEFAULT_AUDIO_SINK@"_s});
}

void Application::activateApp(const QString &desktopEntry)
{
    if (desktopEntry.isEmpty()) {
        return;
    }
    const QString entry = desktopEntry.endsWith(u".desktop"_s) ? desktopEntry : desktopEntry + u".desktop"_s;

    const QString kstart = QStandardPaths::findExecutable(u"kstart"_s);
    if (!kstart.isEmpty()) {
        QProcess::startDetached(kstart, {entry});
        return;
    }
    const QString gtkLaunch = QStandardPaths::findExecutable(u"gtk-launch"_s);
    if (!gtkLaunch.isEmpty()) {
        QProcess::startDetached(gtkLaunch, {entry});
    }
}

void Application::openUrl(const QString &url)
{
    QDesktopServices::openUrl(QUrl(url));
}
