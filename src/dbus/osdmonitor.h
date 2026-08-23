/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

struct DBusMessageInfo;
class Config;

/**
 * Translates Plasma's own on-screen-display traffic into island events.
 *
 * Whenever the user changes volume, brightness, the keyboard layout or a
 * power profile, plasmashell sends a method call to `/org/kde/osdService`.
 * Listening in means Atoll reacts to exactly the same events Plasma's OSD
 * does - including hardware keys and third-party mixers - without having to
 * poll PipeWire or guess at hotkeys.
 */
class OsdMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.osd")

    Q_PROPERTY(QString kind READ kind NOTIFY triggered)
    Q_PROPERTY(QString icon READ icon NOTIFY triggered)
    Q_PROPERTY(QString label READ label NOTIFY triggered)
    Q_PROPERTY(int percent READ percent NOTIFY triggered)
    Q_PROPERTY(int maxPercent READ maxPercent NOTIFY triggered)
    Q_PROPERTY(bool hasProgress READ hasProgress NOTIFY triggered)

public:
    explicit OsdMonitor(Config *config, QObject *parent = nullptr);

    QString kind() const
    {
        return m_kind;
    }
    QString icon() const
    {
        return m_icon;
    }
    QString label() const
    {
        return m_label;
    }
    int percent() const
    {
        return m_percent;
    }
    int maxPercent() const
    {
        return m_maxPercent;
    }
    bool hasProgress() const
    {
        return m_hasProgress;
    }

    /** Feed a message from the bus tap. Returns true if it was ours. */
    bool handleMessage(const DBusMessageInfo &message);

    /** Match rules this monitor needs. */
    static QStringList matchRules();

    /** Push a synthetic OSD, used by the D-Bus control interface. */
    Q_INVOKABLE void showText(const QString &icon, const QString &text);
    Q_INVOKABLE void showProgress(const QString &icon, int percent, const QString &text);

Q_SIGNALS:
    /** A new OSD should be shown. */
    void triggered();
    /** Plasma asked for the OSD to disappear early. */
    void dismissed();

private:
    void emitEvent(const QString &kind, const QString &icon, const QString &label, int percent, int maxPercent);
    static QString volumeIcon(int percent, int maxPercent);

    Config *m_config;
    QString m_kind;
    QString m_icon;
    QString m_label;
    int m_percent = 0;
    int m_maxPercent = 100;
    bool m_hasProgress = false;
};
