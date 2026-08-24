/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "commandrunner.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QTimer>

using namespace Qt::StringLiterals;

CommandRunner::CommandRunner(QObject *parent)
    : QObject(parent)
{
}

QString CommandRunner::condense(const QString &text, int limit)
{
    if (text.size() <= limit) {
        return text;
    }
    // Both ends matter: the head says what the command was doing, the tail says
    // how it ended. The middle of a long build log almost never does.
    const int head = limit / 3;
    const int tail = limit - head;
    return text.left(head) + u"\n… (%1 characters omitted) …\n"_s.arg(text.size() - limit)
        + text.right(tail);
}

void CommandRunner::run(const QString &token, const QString &command, bool elevated, int timeoutMs)
{
    auto *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    // The child inherits the session, minus anything that would let a command
    // read the credentials the assistant itself is using.
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.remove(u"ANTHROPIC_API_KEY"_s);
    environment.remove(u"GEMINI_API_KEY"_s);
    environment.remove(u"GOOGLE_API_KEY"_s);
    environment.insert(u"ATOLL_ASSISTANT"_s, u"1"_s);
    // Package managers and anything else that would rather ask a question than
    // fail need to be told there is nobody at the keyboard.
    environment.insert(u"DEBIAN_FRONTEND"_s, u"noninteractive"_s);
    process->setProcessEnvironment(environment);

    Job job;
    job.process = process;
    m_running.insert(token, job);

    connect(process, &QProcess::readyReadStandardOutput, this, [this, token, process] {
        auto it = m_running.find(token);
        if (it == m_running.end()) {
            return;
        }
        const QString chunk = QString::fromUtf8(process->readAllStandardOutput());
        it->buffer.append(chunk);
        const QStringList lines = chunk.split(u'\n', Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            Q_EMIT progress(token, lines.last().trimmed());
        }
    });

    connect(process, &QProcess::finished, this, [this, token] {
        collect(token);
    });
    connect(process, &QProcess::errorOccurred, this, [this, token, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            auto it = m_running.find(token);
            if (it != m_running.end()) {
                it->buffer.append(u"\natoll: cannot start a shell (%1)"_s.arg(process->errorString()));
            }
            collect(token);
        }
    });

    if (timeoutMs > 0) {
        QTimer::singleShot(timeoutMs, process, [this, token] {
            auto it = m_running.find(token);
            if (it == m_running.end() || !it->process) {
                return;
            }
            it->timedOut = true;
            it->process->kill();
        });
    }

    if (elevated) {
        const QString pkexec = QStandardPaths::findExecutable(u"pkexec"_s);
        if (pkexec.isEmpty()) {
            auto it = m_running.find(token);
            it->buffer = u"atoll: pkexec is not installed, so nothing can be run as root. "
                         "Install polkit and a polkit agent."_s;
            QTimer::singleShot(0, this, [this, token] {
                collect(token);
            });
            return;
        }
        process->start(pkexec, {u"/bin/sh"_s, u"-c"_s, command});
        return;
    }

    process->start(u"/bin/sh"_s, {u"-c"_s, command});
}

void CommandRunner::collect(const QString &token)
{
    auto it = m_running.find(token);
    if (it == m_running.end()) {
        return;
    }
    const Job job = *it;
    m_running.erase(it);

    QString output = job.buffer;
    int exitCode = -1;
    if (job.process) {
        output.append(QString::fromUtf8(job.process->readAllStandardOutput()));
        exitCode = job.process->exitCode();
        job.process->deleteLater();
    }
    if (job.timedOut) {
        exitCode = 124;
    }
    Q_EMIT finished(token, exitCode, condense(output.trimmed()), job.timedOut);
}

void CommandRunner::cancelAll()
{
    const QStringList tokens = m_running.keys();
    for (const QString &token : tokens) {
        auto it = m_running.find(token);
        if (it != m_running.end() && it->process) {
            it->process->kill();
        }
    }
}
