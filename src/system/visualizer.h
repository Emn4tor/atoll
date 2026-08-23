/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class Config;
class QProcess;

/**
 * Drives the little spectrum next to the album art.
 *
 * If `cava` is installed it is used as a real spectrum source over its raw
 * ASCII output; otherwise the bars fall back to a synthetic envelope that
 * still reads as "sound is happening" without pretending to be analysis.
 */
class Visualizer : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.visualizer")

    Q_PROPERTY(QVariantList bars READ bars NOTIFY barsChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(QString source READ source NOTIFY sourceChanged)

public:
    explicit Visualizer(Config *config, QObject *parent = nullptr);
    ~Visualizer() override;

    QVariantList bars() const
    {
        return m_bars;
    }
    bool active() const
    {
        return m_active;
    }
    void setActive(bool active);
    QString source() const
    {
        return m_source;
    }

Q_SIGNALS:
    void barsChanged();
    void activeChanged();
    void sourceChanged();

private:
    int barCount() const;
    void start();
    void stop();
    bool startCava();
    void stepSynthetic();
    void publish(const QList<double> &values);

    Config *m_config;
    QProcess *m_cava = nullptr;
    QTimer m_syntheticTimer;
    QVariantList m_bars;
    QList<double> m_values;
    QList<double> m_targets;
    QString m_source = QStringLiteral("off");
    QString m_cavaConfigPath;
    QByteArray m_cavaBuffer;
    bool m_active = false;
    bool m_draining = false;
};
