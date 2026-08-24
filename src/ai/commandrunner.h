/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

class QProcess;

/**
 * Runs the shell commands the assistant asked for, one at a time, and reports
 * what they printed.
 *
 * Elevation goes through pkexec rather than sudo: polkit already owns the
 * desktop's idea of "prove it is really you", so the prompt is the same dialog
 * the rest of Plasma uses and it honours whatever the machine is configured to
 * accept - a password, a fingerprint, or a hardware key through PAM. Atoll
 * never sees, stores or types a password.
 */
class CommandRunner : public QObject
{
    Q_OBJECT

public:
    explicit CommandRunner(QObject *parent = nullptr);

    /**
     * Start `command` under /bin/sh. `token` comes back with every signal, so
     * the caller can match output to the tool call that asked for it.
     */
    void run(const QString &token, const QString &command, bool elevated, int timeoutMs);

    /** Stop everything that is still running. */
    void cancelAll();

    bool busy() const
    {
        return !m_running.isEmpty();
    }

    /** Cut a command's output down to something a model can be handed. */
    static QString condense(const QString &text, int limit = 20000);

Q_SIGNALS:
    /** A line appeared. Used to keep the island's progress text moving. */
    void progress(const QString &token, const QString &line);
    void finished(const QString &token, int exitCode, const QString &output, bool timedOut);

private:
    struct Job {
        QPointer<QProcess> process;
        QString buffer;
        bool timedOut = false;
    };

    void collect(const QString &token);

    QHash<QString, Job> m_running;
};
