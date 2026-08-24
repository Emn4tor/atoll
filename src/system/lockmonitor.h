/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

/**
 * Whether the session is locked, from the screensaver's own announcement.
 *
 * The island can outlive the lock screen, which means it also has to know it
 * is on one: what is fine on a desktop somebody is sitting at - notification
 * bodies, a dashboard full of history - is not fine on a locked machine.
 */
class LockMonitor : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.lock")

    Q_PROPERTY(bool locked READ locked NOTIFY lockedChanged)

public:
    explicit LockMonitor(QObject *parent = nullptr);

    bool locked() const
    {
        return m_locked;
    }

Q_SIGNALS:
    void lockedChanged();

private Q_SLOTS:
    void onActiveChanged(bool active);

private:
    void setLocked(bool locked);
    void queryInitialState();

    bool m_locked = false;
};
