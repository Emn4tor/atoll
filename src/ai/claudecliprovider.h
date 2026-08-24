/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aiprovider.h"
#include "ai/aitypes.h"

#include <QPointer>
#include <QSet>
#include <QString>

class QProcess;

/**
 * What the local command-line client can tell us about itself.
 *
 * The three questions a first-time user actually has - is it there, am I
 * signed in, and as whom - are the three fields here, and the settings page
 * shows nothing else.
 */
struct CliStatus {
    bool installed = false;
    bool loggedIn = false;
    QString path;
    QString version;
    /** The account the client is signed in as, when it says. */
    QString account;
    /** "pro", "max", "team" - whatever the client reports. */
    QString plan;
    /** How it authenticates: a subscription login, or a key from the environment. */
    QString method;
    /** Set when the check itself went wrong, rather than the login. */
    QString error;
};

/**
 * The assistant, served by the Claude Code command-line client.
 *
 * This is the path that asks nothing of the user beyond an account they very
 * likely already have: no key to create, no billing page to find, no string to
 * paste into a settings window. The client is already signed in, so Atoll runs
 * it and reads what comes back.
 *
 * The trade is that the client runs the tool loop itself - it decides to read
 * a file, it reads the file - so Atoll cannot gate the work by simply not
 * carrying it out, the way it does with the HTTP services. Instead every tool
 * call is stopped before it happens by a hook the client is started with,
 * which calls back into the running island over D-Bus and waits for the same
 * verdict the same broker would have given. Nothing runs that the user was not
 * asked about, and the question looks identical whichever service answers it.
 */
class ClaudeCliProvider : public AiBackend
{
    Q_OBJECT

public:
    explicit ClaudeCliProvider(QObject *parent = nullptr);
    ~ClaudeCliProvider() override;

    QString id() const override
    {
        return QStringLiteral("claude-cli");
    }
    QString defaultModel() const override
    {
        // An alias rather than a pinned name: the client resolves it to
        // whatever the current model of that class is, so Atoll does not go
        // stale between releases. Sonnet rather than the largest model,
        // because a desktop question is not a research project and the
        // subscription allowance is shared with whatever else the user runs.
        return QStringLiteral("sonnet");
    }
    bool drivesTools() const override
    {
        return true;
    }

    void send(const AiRequest &request) override;
    void abort() override;
    bool busy() const override;

    /** Where the client lives, and whether it is signed in. */
    CliStatus status() const
    {
        return m_status;
    }
    /** Look again. `statusChanged` follows, always. */
    void refreshStatus();
    /** An explicit path from the config; empty means look it up. */
    void setExecutablePath(const QString &path);

    /**
     * Where the assistant is run from.
     *
     * Not the user's home: the client reads project instructions out of the
     * directory it starts in, and the assistant on the island should behave
     * the same on every machine rather than inherit whatever a home directory
     * happens to contain. It is also where a screenshot is put for the client
     * to open.
     */
    static QString workspacePath();

    /** The name of the client's executable, wherever it turns out to be. */
    static QString findExecutable(const QString &configured);

    /** One line about a tool call, in the words a person would use. */
    static QString describe(const QString &tool, const QVariantMap &input);

    /**
     * The client's own tools, restated as the calls Atoll's broker knows how
     * to judge. A shell command is a shell command whoever runs it, so the
     * rules that decide what a command costs are the same ones, unchanged.
     */
    static AiToolCall toAtollCall(const QString &id, const QString &tool, const QVariantMap &input);

Q_SIGNALS:
    void statusChanged();

private:
    void start(const AiRequest &request);
    void writeTurn(const AiTurn &turn);
    void consume();
    void handleLine(const QByteArray &line);
    void handleStreamEvent(const QJsonObject &event);
    void handleAssistantMessage(const QJsonObject &message);
    void handleUserMessage(const QJsonObject &message);
    void handleResult(const QJsonObject &object);
    QStringList arguments(const AiRequest &request) const;
    /** The settings blob that puts Atoll in front of every tool call. */
    static QString hookSettings();

    QPointer<QProcess> m_process;
    QByteArray m_buffer;
    QByteArray m_errors;
    CliStatus m_status;
    QString m_configuredPath;
    /** What the running process was started with, so a change can restart it. */
    QString m_startedModel;
    QString m_startedPrompt;
    bool m_turnInFlight = false;
    bool m_stopping = false;
    /** tool_use ids already reported, because a block can be seen twice. */
    QSet<QString> m_announced;
};
