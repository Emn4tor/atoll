/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "osdmonitor.h"

#include "config/config.h"
#include "dbusmonitor.h"

#include <KLocalizedString>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView OsdInterface{"org.kde.osdService"};
}

OsdMonitor::OsdMonitor(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

QStringList OsdMonitor::matchRules()
{
    return {u"type='method_call',interface='org.kde.osdService'"_s};
}

QString OsdMonitor::volumeIcon(int percent, int maxPercent)
{
    if (percent <= 0) {
        return u"audio-volume-muted"_s;
    }
    const int normalised = maxPercent > 0 ? percent * 100 / qMax(100, maxPercent) : percent;
    if (normalised < 34) {
        return u"audio-volume-low"_s;
    }
    if (normalised < 67) {
        return u"audio-volume-medium"_s;
    }
    return u"audio-volume-high"_s;
}

void OsdMonitor::emitEvent(const QString &kind, const QString &icon, const QString &label, int percent, int maxPercent)
{
    m_kind = kind;
    m_icon = icon;
    m_label = label;
    m_percent = percent;
    m_maxPercent = maxPercent > 0 ? maxPercent : 100;
    m_hasProgress = percent >= 0;
    Q_EMIT triggered();
}

bool OsdMonitor::handleMessage(const DBusMessageInfo &message)
{
    if (message.interface != OsdInterface) {
        return false;
    }
    if (!m_config->value(u"modules.osd"_s, true).toBool()) {
        return true;
    }

    const QVariantList &args = message.arguments;
    const QString &member = message.member;
    const auto intArg = [&args](int index, int fallback = 0) {
        return args.size() > index ? args.at(index).toInt() : fallback;
    };
    const auto stringArg = [&args](int index) {
        return args.size() > index ? args.at(index).toString() : QString();
    };
    const auto boolArg = [&args](int index) {
        return args.size() > index && args.at(index).toBool();
    };

    if (member == u"volumeChanged"_s) {
        const int percent = intArg(0);
        const int maximum = args.size() > 1 ? args.at(1).toInt() : 100;
        emitEvent(u"volume"_s, volumeIcon(percent, maximum), i18n("Volume"), percent, maximum);
        return true;
    }
    if (member == u"microphoneVolumeChanged"_s) {
        const int percent = intArg(0);
        emitEvent(u"microphone"_s,
                  percent <= 0 ? u"microphone-sensitivity-muted"_s : u"microphone-sensitivity-high"_s,
                  i18n("Microphone"),
                  percent,
                  100);
        return true;
    }
    if (member == u"mediaPlayerVolumeChanged"_s) {
        if (!m_config->value(u"osd.showMediaPlayerVolume"_s, true).toBool()) {
            return true;
        }
        const int percent = intArg(0);
        const QString name = stringArg(1);
        const QString icon = stringArg(2);
        emitEvent(u"mediaVolume"_s,
                  icon.isEmpty() ? volumeIcon(percent, 100) : icon,
                  name.isEmpty() ? i18n("Player volume") : name,
                  percent,
                  100);
        return true;
    }
    if (member == u"brightnessChanged"_s || member == u"screenBrightnessChanged"_s) {
        const int percent = intArg(0);
        const QString label = member == u"screenBrightnessChanged"_s && !stringArg(2).isEmpty()
            ? stringArg(2)
            : i18n("Brightness");
        emitEvent(u"brightness"_s, u"video-display-brightness"_s, label, percent, 100);
        return true;
    }
    if (member == u"keyboardBrightnessChanged"_s) {
        emitEvent(u"keyboardBrightness"_s, u"input-keyboard-brightness"_s, i18n("Keyboard backlight"), intArg(0), 100);
        return true;
    }
    if (member == u"kbdLayoutChanged"_s) {
        emitEvent(u"keyboardLayout"_s, u"input-keyboard"_s, stringArg(0), -1, 100);
        return true;
    }
    if (member == u"powerProfileChanged"_s) {
        const QString profile = stringArg(0);
        QString icon = u"battery-profile-balanced"_s;
        QString label = profile;
        if (profile == u"power-saver"_s) {
            icon = u"battery-profile-powersave"_s;
            label = i18n("Power Save");
        } else if (profile == u"performance"_s) {
            icon = u"battery-profile-performance"_s;
            label = i18n("Performance");
        } else if (profile == u"balanced"_s) {
            label = i18n("Balanced");
        }
        emitEvent(u"powerProfile"_s, icon, label, -1, 100);
        return true;
    }
    if (member == u"touchpadEnabledChanged"_s) {
        const bool on = boolArg(0);
        emitEvent(u"touchpad"_s,
                  on ? u"input-touchpad-on"_s : u"input-touchpad-off"_s,
                  on ? i18n("Touchpad on") : i18n("Touchpad off"),
                  -1,
                  100);
        return true;
    }
    if (member == u"wifiEnabledChanged"_s) {
        const bool on = boolArg(0);
        emitEvent(u"wifi"_s,
                  on ? u"network-wireless-connected"_s : u"network-wireless-disconnected"_s,
                  on ? i18n("Wi-Fi on") : i18n("Wi-Fi off"),
                  -1,
                  100);
        return true;
    }
    if (member == u"bluetoothEnabledChanged"_s) {
        const bool on = boolArg(0);
        emitEvent(u"bluetooth"_s,
                  on ? u"bluetooth-active"_s : u"bluetooth-disabled"_s,
                  on ? i18n("Bluetooth on") : i18n("Bluetooth off"),
                  -1,
                  100);
        return true;
    }
    if (member == u"wwanEnabledChanged"_s) {
        const bool on = boolArg(0);
        emitEvent(u"wwan"_s, u"network-mobile"_s, on ? i18n("Mobile data on") : i18n("Mobile data off"), -1, 100);
        return true;
    }
    if (member == u"virtualKeyboardEnabledChanged"_s) {
        const bool on = boolArg(0);
        emitEvent(u"virtualKeyboard"_s,
                  u"input-keyboard-virtual"_s,
                  on ? i18n("Virtual keyboard on") : i18n("Virtual keyboard off"),
                  -1,
                  100);
        return true;
    }
    if (member == u"powerManagementInhibitedChanged"_s) {
        const bool inhibited = boolArg(0);
        emitEvent(u"caffeine"_s,
                  inhibited ? u"system-suspend-inhibited"_s : u"system-suspend"_s,
                  inhibited ? i18n("Sleep blocked") : i18n("Sleep allowed"),
                  -1,
                  100);
        return true;
    }
    if (member == u"virtualDesktopChanged"_s) {
        emitEvent(u"desktop"_s, u"virtual-desktops"_s, stringArg(0), -1, 100);
        return true;
    }
    if (member == u"showText"_s) {
        emitEvent(u"text"_s, stringArg(0), stringArg(1), -1, 100);
        return true;
    }
    if (member == u"hide"_s) {
        Q_EMIT dismissed();
        return true;
    }

    return true;
}

void OsdMonitor::showText(const QString &icon, const QString &text)
{
    emitEvent(u"text"_s, icon, text, -1, 100);
}

void OsdMonitor::showProgress(const QString &icon, int percent, const QString &text)
{
    emitEvent(u"progress"_s, icon, text, qBound(0, percent, 100), 100);
}
