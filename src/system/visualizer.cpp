/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "visualizer.h"

#include "config/config.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRandomGenerator>
#include <QStandardPaths>

using namespace Qt::StringLiterals;

namespace
{
constexpr int SyntheticIntervalMs = 33;
}

Visualizer::Visualizer(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
{
    m_syntheticTimer.setInterval(SyntheticIntervalMs);
    connect(&m_syntheticTimer, &QTimer::timeout, this, &Visualizer::stepSynthetic);

    connect(m_config, &Config::changed, this, [this] {
        if (m_active) {
            stop();
            start();
        }
    });
}

Visualizer::~Visualizer()
{
    stop();
}

int Visualizer::barCount() const
{
    return qBound(4, m_config->value(u"media.visualizerBars"_s, 26).toInt(), 96);
}

void Visualizer::setActive(bool active)
{
    if (m_active == active) {
        return;
    }
    m_active = active;
    Q_EMIT activeChanged();

    if (active) {
        start();
    } else if (m_source == u"cava"_s) {
        stop();
    } else {
        // Let the synthetic bars fall gracefully instead of snapping to zero.
        m_draining = true;
    }
}

void Visualizer::start()
{
    const int count = barCount();
    m_values.assign(count, 0.0);
    m_targets.assign(count, 0.0);
    m_draining = false;

    if (!m_config->value(u"modules.visualizer"_s, true).toBool()) {
        m_source = u"off"_s;
        Q_EMIT sourceChanged();
        return;
    }

    const QString mode = m_config->value(u"media.cava"_s, u"auto"_s).toString();
    if (mode != u"never"_s && startCava()) {
        return;
    }

    m_source = u"synthetic"_s;
    Q_EMIT sourceChanged();
    m_syntheticTimer.start();
}

bool Visualizer::startCava()
{
    const QString binary = QStandardPaths::findExecutable(u"cava"_s);
    if (binary.isEmpty()) {
        return false;
    }

    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QDir().mkpath(runtime);
    m_cavaConfigPath = QDir(runtime).filePath(u"atoll-cava.conf"_s);

    QFile config(m_cavaConfigPath);
    if (!config.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    config.write(u"[general]\n"
                 "framerate = 60\n"
                 "bars = %1\n"
                 "autosens = 1\n"
                 "\n"
                 "[input]\n"
                 "method = pulse\n"
                 "source = auto\n"
                 "\n"
                 "[output]\n"
                 "method = raw\n"
                 "raw_target = /dev/stdout\n"
                 "data_format = ascii\n"
                 "ascii_max_range = 100\n"
                 "channels = mono\n"_s.arg(barCount())
                     .toUtf8());
    config.close();

    m_cava = new QProcess(this);
    m_cava->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_cava, &QProcess::readyReadStandardOutput, this, [this] {
        m_cavaBuffer.append(m_cava->readAllStandardOutput());
        int newline = -1;
        QByteArray lastLine;
        while ((newline = m_cavaBuffer.indexOf('\n')) >= 0) {
            lastLine = m_cavaBuffer.left(newline);
            m_cavaBuffer.remove(0, newline + 1);
        }
        if (lastLine.isEmpty()) {
            return;
        }
        const QList<QByteArray> parts = lastLine.split(';');
        QList<double> values;
        values.reserve(parts.size());
        for (const QByteArray &part : parts) {
            if (part.isEmpty()) {
                continue;
            }
            values.append(qBound(0.0, part.toDouble() / 100.0, 1.0));
        }
        if (!values.isEmpty()) {
            publish(values);
        }
    });
    connect(m_cava, &QProcess::errorOccurred, this, [this] {
        // cava died or was never usable: silently degrade.
        if (m_source == u"cava"_s) {
            m_source = u"synthetic"_s;
            Q_EMIT sourceChanged();
            m_syntheticTimer.start();
        }
    });

    m_cava->start(binary, {u"-p"_s, m_cavaConfigPath});
    if (!m_cava->waitForStarted(1500)) {
        m_cava->deleteLater();
        m_cava = nullptr;
        return false;
    }

    m_source = u"cava"_s;
    Q_EMIT sourceChanged();
    return true;
}

void Visualizer::stop()
{
    m_syntheticTimer.stop();
    if (m_cava) {
        m_cava->disconnect(this);
        m_cava->terminate();
        if (!m_cava->waitForFinished(500)) {
            m_cava->kill();
            m_cava->waitForFinished(300);
        }
        m_cava->deleteLater();
        m_cava = nullptr;
    }
    if (!m_cavaConfigPath.isEmpty()) {
        QFile::remove(m_cavaConfigPath);
        m_cavaConfigPath.clear();
    }
    m_cavaBuffer.clear();

    if (m_source != u"off"_s) {
        m_source = u"off"_s;
        Q_EMIT sourceChanged();
    }
    if (!m_bars.isEmpty()) {
        m_bars.clear();
        Q_EMIT barsChanged();
    }
}

void Visualizer::stepSynthetic()
{
    const int count = m_values.size();
    if (count == 0) {
        m_syntheticTimer.stop();
        return;
    }

    auto *random = QRandomGenerator::global();
    bool anyEnergy = false;

    for (int i = 0; i < count; ++i) {
        if (m_draining) {
            m_targets[i] = 0.0;
        } else if (random->bounded(100) < 12) {
            // A bass-weighted curve keeps the shape musical rather than noisy.
            const double position = double(i) / double(qMax(1, count - 1));
            const double weight = 0.45 + 0.55 * qExp(-3.2 * position);
            m_targets[i] = random->bounded(1.0) * weight;
        }
        const double delta = m_targets.at(i) - m_values.at(i);
        m_values[i] += delta * (delta > 0 ? 0.35 : 0.12);
        if (m_values.at(i) > 0.004) {
            anyEnergy = true;
        } else {
            m_values[i] = 0.0;
        }
    }

    publish(m_values);

    if (m_draining && !anyEnergy) {
        stop();
    }
}

void Visualizer::publish(const QList<double> &values)
{
    m_bars.clear();
    m_bars.reserve(values.size());
    for (double value : values) {
        m_bars.append(value);
    }
    Q_EMIT barsChanged();
}
