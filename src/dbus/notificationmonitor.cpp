/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "notificationmonitor.h"

#include "app/imagestore.h"
#include "config/config.h"

#include <QImage>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
constexpr QLatin1StringView NotifyInterface{"org.freedesktop.Notifications"};

/**
 * Rebuild a QImage from the freedesktop `image-data` hint, whose signature is
 * (iiibiiay): width, height, rowstride, has-alpha, bits-per-sample, channels
 * and the pixel buffer itself.
 */
QImage imageFromHint(const QVariant &hint)
{
    const QVariantList parts = hint.toList();
    if (parts.size() < 7) {
        return {};
    }
    const int width = parts.at(0).toInt();
    const int height = parts.at(1).toInt();
    const int rowStride = parts.at(2).toInt();
    const bool hasAlpha = parts.at(3).toBool();
    const int bitsPerSample = parts.at(4).toInt();
    const int channels = parts.at(5).toInt();
    const QByteArray pixels = parts.at(6).toByteArray();

    if (width <= 0 || height <= 0 || bitsPerSample != 8) {
        return {};
    }
    const int expectedChannels = hasAlpha ? 4 : 3;
    if (channels != expectedChannels || rowStride < width * channels) {
        return {};
    }
    if (pixels.size() < qsizetype(rowStride) * (height - 1) + qsizetype(width) * channels) {
        return {};
    }

    const QImage::Format format = hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888;
    // The buffer belongs to the QVariant, so copy before it goes away.
    return QImage(reinterpret_cast<const uchar *>(pixels.constData()), width, height, rowStride, format).copy();
}
}

NotificationMonitor::NotificationMonitor(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
}

QStringList NotificationMonitor::matchRules(bool trackIds)
{
    QStringList rules{
        u"type='method_call',interface='org.freedesktop.Notifications'"_s,
        u"type='signal',interface='org.freedesktop.Notifications'"_s,
    };
    if (trackIds) {
        // Deliberately broad: replies carry no interface to filter on. The
        // monitor thread throws away everything that is not a bare uint32.
        rules << u"type='method_return'"_s;
    }
    return rules;
}

QString NotificationMonitor::extractImage(const QVariantMap &hints, quint64 uid, QColor *accent) const
{
    // Spec order of preference, plus the legacy spellings apps still emit.
    static const QStringList dataHints{u"image-data"_s, u"image_data"_s, u"icon_data"_s};
    for (const QString &name : dataHints) {
        if (!hints.contains(name)) {
            continue;
        }
        const QImage image = imageFromHint(hints.value(name));
        if (image.isNull()) {
            continue;
        }
        if (accent) {
            *accent = ImageStore::dominantColor(image);
        }
        return ImageStore::instance().put(u"notification-%1"_s.arg(uid), image);
    }

    const QString path = hints.value(u"image-path"_s, hints.value(u"image_path"_s)).toString();
    if (path.isEmpty()) {
        return {};
    }
    if (path.startsWith(u"file://"_s)) {
        const QString local = QUrl(path).toLocalFile();
        const QImage image(local);
        if (!image.isNull() && accent) {
            *accent = ImageStore::dominantColor(image);
        }
        return path;
    }
    if (path.startsWith(u'/')) {
        const QImage image(path);
        if (!image.isNull() && accent) {
            *accent = ImageStore::dominantColor(image);
        }
        return QUrl::fromLocalFile(path).toString();
    }
    // Anything else is an icon name.
    return u"image://icon/%1"_s.arg(path);
}

bool NotificationMonitor::handleMessage(const DBusMessageInfo &message)
{
    if (message.kind == DBusMessageInfo::MethodReturn) {
        const auto it = m_pending.constFind(message.replySerial);
        if (it == m_pending.cend()) {
            return false;
        }
        if (!it->caller.isEmpty() && !message.destination.isEmpty() && it->caller != message.destination) {
            return false;
        }
        const quint64 uid = it->uid;
        m_pending.erase(it);
        if (!message.arguments.isEmpty()) {
            Q_EMIT idAssigned(uid, message.arguments.first().toUInt());
        }
        return true;
    }

    if (message.interface != NotifyInterface) {
        return false;
    }

    if (message.kind == DBusMessageInfo::Signal) {
        if (message.member == u"NotificationClosed"_s && message.arguments.size() >= 1) {
            Q_EMIT closed(message.arguments.at(0).toUInt(),
                          message.arguments.size() > 1 ? message.arguments.at(1).toInt() : 0);
        } else if (message.member == u"ActionInvoked"_s && message.arguments.size() >= 2) {
            Q_EMIT actionInvoked(message.arguments.at(0).toUInt(), message.arguments.at(1).toString());
        }
        return true;
    }

    if (message.kind != DBusMessageInfo::MethodCall) {
        return false;
    }

    if (message.member == u"CloseNotification"_s && !message.arguments.isEmpty()) {
        Q_EMIT closed(message.arguments.first().toUInt(), 3);
        return true;
    }

    if (message.member != u"Notify"_s || message.arguments.size() < 8) {
        return true;
    }
    if (!m_config->value(u"modules.notifications"_s, true).toBool()) {
        return true;
    }

    const QVariantList &a = message.arguments;
    NotificationData data;
    data.uid = m_nextUid++;
    data.appName = a.at(0).toString();
    data.daemonId = a.at(1).toUInt(); // replaces_id, refined once the reply lands
    data.appIcon = a.at(2).toString();
    data.summary = a.at(3).toString();
    data.body = a.at(4).toString();
    data.actions = a.at(5).toStringList();
    const QVariantMap hints = a.at(6).toMap();
    data.expireTimeout = a.at(7).toInt();
    data.timestamp = QDateTime::currentDateTime();

    const QStringList ignored = m_config->value(u"notifications.ignoredApps"_s).toStringList();
    data.desktopEntry = hints.value(u"desktop-entry"_s).toString();
    for (const QString &pattern : ignored) {
        if (pattern.compare(data.appName, Qt::CaseInsensitive) == 0
            || (!data.desktopEntry.isEmpty() && pattern.compare(data.desktopEntry, Qt::CaseInsensitive) == 0)) {
            return true;
        }
    }

    data.urgency = hints.contains(u"urgency"_s) ? hints.value(u"urgency"_s).toInt() : 1;
    data.category = hints.value(u"category"_s).toString();
    data.transient = hints.value(u"transient"_s).toBool();
    data.resident = hints.value(u"resident"_s).toBool();
    data.progress = hints.contains(u"value"_s) ? hints.value(u"value"_s).toInt() : -1;
    data.imageUrl = extractImage(hints, data.uid, &data.accent);

    if (m_config->value(u"notifications.trackIds"_s, true).toBool() && message.serial != 0) {
        m_pending.insert(message.serial, PendingCall{message.sender, data.uid});
        // Never let unanswered calls pile up.
        if (m_pending.size() > 64) {
            m_pending.erase(m_pending.begin());
        }
    }

    Q_EMIT posted(data);
    return true;
}
