/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "battery.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView UPowerService{"org.freedesktop.UPower"};
constexpr QLatin1StringView DisplayDevicePath{"/org/freedesktop/UPower/devices/DisplayDevice"};
constexpr QLatin1StringView DeviceInterface{"org.freedesktop.UPower.Device"};
}

Battery::Battery(QObject *parent)
    : QObject(parent)
{
    QDBusConnection::systemBus().connect(QString(UPowerService),
                                         QString(DisplayDevicePath),
                                         u"org.freedesktop.DBus.Properties"_s,
                                         u"PropertiesChanged"_s,
                                         this,
                                         SLOT(onPropertiesChanged(QString, QVariantMap, QStringList)));
    refresh();
}

void Battery::refresh()
{
    auto message = QDBusMessage::createMethodCall(QString(UPowerService),
                                                  QString(DisplayDevicePath),
                                                  u"org.freedesktop.DBus.Properties"_s,
                                                  u"GetAll"_s);
    message << QString(DeviceInterface);
    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::systemBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *self;
        if (reply.isError()) {
            return; // No UPower, or no display device: stay absent.
        }
        apply(reply.value());
    });
}

void Battery::onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated)
{
    Q_UNUSED(invalidated)
    if (interface != DeviceInterface) {
        return;
    }
    apply(changed);
}

void Battery::apply(const QVariantMap &properties)
{
    const auto before = std::make_tuple(m_present, m_percent, m_state, m_iconName, m_timeToEmpty, m_timeToFull);

    if (properties.contains(u"IsPresent"_s)) {
        m_present = properties.value(u"IsPresent"_s).toBool();
    }
    if (properties.contains(u"Type"_s) && properties.value(u"Type"_s).toUInt() != 2) {
        m_present = false; // 2 == battery; anything else is a UPS or a peripheral.
    }
    if (properties.contains(u"Percentage"_s)) {
        m_percent = qRound(properties.value(u"Percentage"_s).toDouble());
    }
    if (properties.contains(u"State"_s)) {
        m_state = properties.value(u"State"_s).toUInt();
    }
    if (properties.contains(u"IconName"_s)) {
        m_iconName = properties.value(u"IconName"_s).toString();
    }
    if (properties.contains(u"TimeToEmpty"_s)) {
        m_timeToEmpty = properties.value(u"TimeToEmpty"_s).toLongLong();
    }
    if (properties.contains(u"TimeToFull"_s)) {
        m_timeToFull = properties.value(u"TimeToFull"_s).toLongLong();
    }

    if (before != std::make_tuple(m_present, m_percent, m_state, m_iconName, m_timeToEmpty, m_timeToFull)) {
        Q_EMIT changed();
    }
}

QString Battery::iconName() const
{
    if (!m_iconName.isEmpty() && m_iconName != u"battery-missing"_s) {
        return m_iconName;
    }
    const int step = qBound(0, (m_percent + 5) / 10 * 10, 100);
    return charging() ? u"battery-%1-charging"_s.arg(step, 3, 10, QLatin1Char('0'))
                      : u"battery-%1"_s.arg(step, 3, 10, QLatin1Char('0'));
}

qint64 Battery::remainingSeconds() const
{
    return charging() ? m_timeToFull : m_timeToEmpty;
}
