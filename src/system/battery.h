/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

/**
 * UPower's aggregated display device, which is exactly what a status readout
 * wants: one battery, already merged across whatever the machine actually has.
 * On a desktop without a battery `present` simply stays false and the island
 * leaves the slot out.
 */
class Battery : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.battery")

    Q_PROPERTY(bool present READ present NOTIFY changed)
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
    Q_PROPERTY(bool full READ full NOTIFY changed)
    Q_PROPERTY(QString iconName READ iconName NOTIFY changed)
    /** Remaining or until-full time in seconds, 0 when unknown. */
    Q_PROPERTY(qint64 remainingSeconds READ remainingSeconds NOTIFY changed)

public:
    explicit Battery(QObject *parent = nullptr);

    bool present() const
    {
        return m_present;
    }
    int percent() const
    {
        return m_percent;
    }
    bool charging() const
    {
        return m_state == 1 || m_state == 5;
    }
    bool full() const
    {
        return m_state == 4;
    }
    QString iconName() const;
    qint64 remainingSeconds() const;

Q_SIGNALS:
    void changed();

private Q_SLOTS:
    void onPropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    void refresh();
    void apply(const QVariantMap &properties);

    bool m_present = false;
    int m_percent = 0;
    uint m_state = 0;
    qint64 m_timeToEmpty = 0;
    qint64 m_timeToFull = 0;
    QString m_iconName;
};
