/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

/**
 * The scriptable side of Atoll: `org.atoll.Atoll` on the session bus.
 *
 * Everything the island can be told to do from the outside lives here, which
 * makes the island usable as a general purpose heads-up display for scripts,
 * hotkeys and other desktop tooling - not just for the events it watches.
 */
class IpcService : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.atoll.Atoll")
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.ipc")

public:
    explicit IpcService(QObject *parent = nullptr);

    /** Claim the bus name. Returns false if another instance already has it. */
    bool registerOnBus();

public Q_SLOTS:
    Q_SCRIPTABLE void expand();
    Q_SCRIPTABLE void collapse();
    Q_SCRIPTABLE void toggle();
    Q_SCRIPTABLE void showText(const QString &icon, const QString &text);
    Q_SCRIPTABLE void showProgress(const QString &icon, int percent, const QString &text);
    Q_SCRIPTABLE void reloadConfig();
    Q_SCRIPTABLE QString version() const;
    Q_SCRIPTABLE void quit();

Q_SIGNALS:
    void expandRequested();
    void collapseRequested();
    void toggleRequested();
    void textRequested(const QString &icon, const QString &text);
    void progressRequested(const QString &icon, int percent, const QString &text);
    void reloadRequested();
};
