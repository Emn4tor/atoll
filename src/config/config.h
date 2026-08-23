/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

/**
 * Runtime configuration, backed by ~/.config/atoll/atoll.json.
 *
 * The user file is deep-merged on top of the built-in defaults, so a config
 * only has to name the keys it wants to override. The file is watched and
 * re-read on change, which makes tweaking the island a live operation.
 */
class Config : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.config")

    /** The fully merged configuration tree, as nested maps. */
    Q_PROPERTY(QVariantMap data READ data NOTIFY changed)
    Q_PROPERTY(QString path READ path CONSTANT)

public:
    explicit Config(QObject *parent = nullptr);

    QVariantMap data() const
    {
        return m_data;
    }
    QString path() const
    {
        return m_path;
    }

    /** Look up a dotted path, e.g. value("island.collapsedWidth", 172). */
    Q_INVOKABLE QVariant value(const QString &dottedKey, const QVariant &fallback = {}) const;

    /** Re-read the file from disk. */
    Q_INVOKABLE void reload();

    /** Write the built-in defaults to disk if no config file exists yet. */
    void ensureUserFile();

    static QVariantMap defaults();

Q_SIGNALS:
    void changed();

private:
    void applyWatch();
    static void deepMerge(QVariantMap &target, const QVariantMap &overlay);

    QString m_path;
    QVariantMap m_data;
    QFileSystemWatcher m_watcher;
};
