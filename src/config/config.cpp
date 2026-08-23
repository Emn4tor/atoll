/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QStandardPaths>
#include <QTimer>
#include <QtGlobal>

using namespace Qt::StringLiterals;

Config::Config(QObject *parent)
    : QObject(parent)
{
    m_path = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
                 .filePath(u"atoll/atoll.json"_s);
    m_data = defaults();

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] {
        // Editors replace rather than rewrite; give the new inode a moment to land.
        QTimer::singleShot(120, this, &Config::reload);
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] {
        QTimer::singleShot(120, this, &Config::reload);
    });

    reload();
}

QVariantMap Config::defaults()
{
    static const char *json = R"JSON({
  "island": {
    "screen": "primary",
    "layer": "overlay",
    "topMargin": 0,
    "collapsedWidth": 168,
    "collapsedHeight": 32,
    "expandedWidth": 460,
    "maxWidth": 620,
    "cornerRadius": 0,
    "idleMode": "notch",
    "exclusiveZone": 0,
    "surfaceHeight": 520
  },
  "appearance": {
    "background": "#0b0b0e",
    "backgroundOpacity": 0.97,
    "foreground": "#f4f4f7",
    "muted": "#9a9aa6",
    "accent": "auto",
    "accentFallback": "#5aa2ff",
    "fontFamily": "",
    "fontScale": 1.0,
    "shadow": true,
    "shadowOpacity": 0.45,
    "border": true,
    "borderColor": "#1affffff"
  },
  "effects": {
    "gooey": true,
    "gooeyStrength": 0.62,
    "spring": 4.2,
    "damping": 0.36,
    "animationScale": 1.0
  },
  "modules": {
    "osd": true,
    "notifications": true,
    "media": true,
    "battery": true,
    "visualizer": true,
    "clock": true
  },
  "osd": {
    "timeout": 1700,
    "showMediaPlayerVolume": true
  },
  "notifications": {
    "timeout": 5000,
    "trackIds": true,
    "ignoredApps": [],
    "criticalStaysOpen": true,
    "maxBodyLines": 3,
    "dnd": false,
    "showActions": true
  },
  "media": {
    "showOnPlay": true,
    "peekDuration": 4200,
    "preferred": [],
    "blocked": [],
    "visualizerBars": 26,
    "cava": "auto"
  },
  "behavior": {
    "expandOnHover": false,
    "hoverPeek": true,
    "hoverDelay": 180,
    "collapseOnLeave": true,
    "scrollAdjustsVolume": true,
    "volumeStep": 5,
    "clickAction": "expand",
    "middleClickAction": "playPause"
  },
  "clock": {
    "timeFormat": "HH:mm",
    "dateFormat": "ddd d MMM"
  }
})JSON";

    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(QByteArray(json), &err);
    Q_ASSERT_X(err.error == QJsonParseError::NoError, "Config::defaults", qPrintable(err.errorString()));
    return doc.object().toVariantMap();
}

void Config::deepMerge(QVariantMap &target, const QVariantMap &overlay)
{
    for (auto it = overlay.cbegin(); it != overlay.cend(); ++it) {
        const QVariant &incoming = it.value();
        const QVariant existing = target.value(it.key());
        if (incoming.typeId() == QMetaType::QVariantMap && existing.typeId() == QMetaType::QVariantMap) {
            QVariantMap sub = existing.toMap();
            deepMerge(sub, incoming.toMap());
            target.insert(it.key(), sub);
        } else {
            target.insert(it.key(), incoming);
        }
    }
}

void Config::reload()
{
    QVariantMap merged = defaults();

    QFile file(m_path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError err{};
        const auto doc = QJsonDocument::fromJson(file.readAll(), &err);
        if (err.error != QJsonParseError::NoError) {
            qWarning("atoll: %s is not valid JSON (%s at offset %d) - using defaults",
                     qUtf8Printable(m_path), qUtf8Printable(err.errorString()), err.offset);
        } else if (doc.isObject()) {
            deepMerge(merged, doc.object().toVariantMap());
        }
    }

    applyWatch();

    if (merged != m_data) {
        m_data = merged;
        Q_EMIT changed();
    }
}

void Config::applyWatch()
{
    const QString dir = QFileInfo(m_path).absolutePath();
    if (!m_watcher.directories().contains(dir) && QDir(dir).exists()) {
        m_watcher.addPath(dir);
    }
    if (!m_watcher.files().contains(m_path) && QFile::exists(m_path)) {
        m_watcher.addPath(m_path);
    }
}

QVariant Config::value(const QString &dottedKey, const QVariant &fallback) const
{
    const QStringList parts = dottedKey.split(u'.', Qt::SkipEmptyParts);
    QVariant cursor = m_data;
    for (const QString &part : parts) {
        if (cursor.typeId() != QMetaType::QVariantMap) {
            return fallback;
        }
        const QVariantMap map = cursor.toMap();
        if (!map.contains(part)) {
            return fallback;
        }
        cursor = map.value(part);
    }
    return cursor.isValid() ? cursor : fallback;
}

void Config::ensureUserFile()
{
    if (QFile::exists(m_path)) {
        return;
    }
    const QFileInfo info(m_path);
    QDir().mkpath(info.absolutePath());

    QFile file(m_path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning("atoll: cannot write default config to %s", qUtf8Printable(m_path));
        return;
    }
    file.write(QJsonDocument(QJsonObject::fromVariantMap(defaults())).toJson(QJsonDocument::Indented));
    file.close();
    applyWatch();
}
