/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aitypes.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>

class CommandRunner;
class Config;
class ScreenCapture;

/**
 * What the assistant is able to do, described once and translated per provider.
 *
 * The catalogue is deliberately small. One general command tool covers the
 * long tail, and the named tools above it exist where a first-class verb makes
 * the intent legible in the permission prompt: "Install firefox" reads better
 * on the island than the pacman invocation it turns into, and it is the prompt
 * the user is being asked to judge.
 */
class AiToolbox : public QObject
{
    Q_OBJECT

public:
    explicit AiToolbox(Config *config, QObject *parent = nullptr);

    /**
     * The tool list in a neutral shape: [{name, description, input_schema}].
     * Providers rewrite this into whatever they call the same idea.
     */
    static QJsonArray definitions(bool screenshotAllowed);

    /** The system prompt, with the machine's own details filled in. */
    static QString systemPrompt(const QString &userAddition);

    /**
     * What has to be said as well when the backend carries out its own tool
     * calls: it has no terminal, it starts somewhere of Atoll's choosing, and
     * the way it asks for administrator rights is not the way a script does.
     */
    static QString clientAddendum(bool screenshotAllowed);

    /** Carry out one call. Exactly one signal follows, eventually. */
    void execute(const AiToolCall &call, AiRisk risk);

    /** Abandon whatever is in flight. */
    void cancel();

Q_SIGNALS:
    void completed(const AiToolResult &result);
    /** A line worth putting on the island while a long command runs. */
    void progress(const QString &line);
    /** The assistant asked to show the user something. */
    void messageRequested(const QString &summary, const QString &body);

private:
    void finish(const QString &id, const QString &content, bool isError = false);
    void runShell(const QString &id, const QString &command, bool elevated);

    QString readFile(const QVariantMap &input) const;
    QString writeFile(const QString &id, const QVariantMap &input, bool elevated);
    QString listDirectory(const QVariantMap &input) const;
    QString remember(const QVariantMap &input) const;
    static QString packageManagerInstall(const QStringList &packages);
    static QString packageManagerUpdate();

    Config *m_config = nullptr;
    CommandRunner *m_runner = nullptr;
    ScreenCapture *m_capture = nullptr;
    /** The call a running command belongs to. */
    QString m_pending;
    /** Which output the picture being taken is of, for the line that goes with it. */
    QString m_captured;
};
