/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDateTime>
#include <QObject>
#include <QTimer>
#include <QtQml/qqmlregistration.h>

class Config;

/** A minute-aligned clock, so the idle island never ticks a second late. */
class Clock : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.clock")

    Q_PROPERTY(QString time READ time NOTIFY tick)
    Q_PROPERTY(QString date READ date NOTIFY tick)
    Q_PROPERTY(QDateTime now READ now NOTIFY tick)

public:
    explicit Clock(Config *config, QObject *parent = nullptr);

    QString time() const;
    QString date() const;
    QDateTime now() const
    {
        return QDateTime::currentDateTime();
    }

Q_SIGNALS:
    void tick();

private:
    void scheduleNext();

    Config *m_config;
    QTimer m_timer;
};
