/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "claudecliprovider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>

using namespace Qt::StringLiterals;

namespace
{
/**
 * How long the client is allowed to sit inside one permission question.
 *
 * It is a person being asked, on a panel they may not be looking at, so the
 * number is generous. It is not unbounded: a hook that never returns would
 * leave the client wedged with no way for the user to reach it.
 */
constexpr int PermissionTimeoutSeconds = 600;

/** The places a user-installed client turns up, beyond the ones in PATH. */
QStringList candidatePaths()
{
    const QString home = QDir::homePath();
    return {home + u"/.local/bin/claude"_s,
            home + u"/.claude/local/claude"_s,
            home + u"/bin/claude"_s,
            u"/usr/local/bin/claude"_s,
            u"/usr/bin/claude"_s};
}

/** Effort levels the client understands; anything else is left to it. */
bool knownEffort(const QString &effort)
{
    static const QStringList levels = {u"low"_s, u"medium"_s, u"high"_s, u"xhigh"_s, u"max"_s};
    return levels.contains(effort);
}

QString shellQuote(const QString &value)
{
    QString escaped = value;
    escaped.replace(u"'"_s, u"'\\''"_s);
    return u'\'' + escaped + u'\'';
}

/** The text of a tool result, whichever shape the client sent it in. */
QString resultText(const QJsonValue &content)
{
    if (content.isString()) {
        return content.toString();
    }
    QString text;
    const QJsonArray blocks = content.toArray();
    for (const QJsonValue &block : blocks) {
        const QJsonObject object = block.toObject();
        if (object.value(u"type"_s).toString() == u"text"_s) {
            text.append(object.value(u"text"_s).toString());
        }
    }
    return text;
}
}

ClaudeCliProvider::ClaudeCliProvider(QObject *parent)
    : AiBackend(parent)
{
    m_status.path = findExecutable({});
    m_status.installed = !m_status.path.isEmpty();
}

ClaudeCliProvider::~ClaudeCliProvider()
{
    abort();
}

QString ClaudeCliProvider::findExecutable(const QString &configured)
{
    if (!configured.isEmpty()) {
        const QFileInfo info(configured);
        return info.isExecutable() ? info.absoluteFilePath() : QString();
    }

    const QString found = QStandardPaths::findExecutable(u"claude"_s);
    if (!found.isEmpty()) {
        return found;
    }
    // The official installer puts it in ~/.local/bin, which is on the PATH of
    // a login shell and very often not on the PATH of a session started by
    // systemd - which is exactly how Atoll itself is usually started.
    for (const QString &candidate : candidatePaths()) {
        if (QFileInfo(candidate).isExecutable()) {
            return candidate;
        }
    }
    return {};
}

QString ClaudeCliProvider::workspacePath()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString path = QDir(base).filePath(u"atoll/assistant"_s);
    QDir().mkpath(path);
    return path;
}

void ClaudeCliProvider::setExecutablePath(const QString &path)
{
    if (m_configuredPath == path) {
        return;
    }
    m_configuredPath = path;
    m_status.path = findExecutable(path);
    m_status.installed = !m_status.path.isEmpty();
    Q_EMIT statusChanged();
}

void ClaudeCliProvider::refreshStatus()
{
    m_status = CliStatus{};
    m_status.path = findExecutable(m_configuredPath);
    m_status.installed = !m_status.path.isEmpty();
    if (!m_status.installed) {
        Q_EMIT statusChanged();
        return;
    }

    auto *probe = new QProcess(this);
    probe->setProgram(m_status.path);
    probe->setArguments({u"auth"_s, u"status"_s});
    probe->setProcessChannelMode(QProcess::SeparateChannels);

    connect(probe, &QProcess::finished, this, [this, probe] {
        const QByteArray output = probe->readAllStandardOutput();
        probe->deleteLater();

        const QJsonObject object = QJsonDocument::fromJson(output).object();
        if (object.isEmpty()) {
            m_status.error = tr("The client did not answer when asked who is signed in.");
            Q_EMIT statusChanged();
            return;
        }
        m_status.loggedIn = object.value(u"loggedIn"_s).toBool();
        m_status.account = object.value(u"email"_s).toString();
        m_status.plan = object.value(u"subscriptionType"_s).toString();
        m_status.method = object.value(u"authMethod"_s).toString();
        Q_EMIT statusChanged();
    });
    connect(probe, &QProcess::errorOccurred, this, [this, probe] {
        m_status.error = tr("Could not run %1.").arg(m_status.path);
        probe->deleteLater();
        Q_EMIT statusChanged();
    });

    probe->start();
}

// ---- a turn --------------------------------------------------------------

bool ClaudeCliProvider::busy() const
{
    return m_turnInFlight;
}

QString ClaudeCliProvider::hookSettings()
{
    // Every tool call the client is about to make goes through this, and the
    // process it names is Atoll itself, in a mode that does nothing but ask
    // the running island what it thinks. If the hook cannot be reached the
    // client falls back to its own rules, which in this mode refuse - so a
    // broken bridge is an assistant that cannot act, never one that acts
    // unasked.
    const QJsonObject hook{{u"type"_s, u"command"_s},
                           {u"command"_s,
                            shellQuote(QCoreApplication::applicationFilePath())
                                + u" --permission-hook"_s},
                           {u"timeout"_s, PermissionTimeoutSeconds}};
    const QJsonObject matcher{{u"matcher"_s, u"*"_s}, {u"hooks"_s, QJsonArray{hook}}};
    const QJsonObject settings{
        {u"hooks"_s, QJsonObject{{u"PreToolUse"_s, QJsonArray{matcher}}}}};
    return QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Compact));
}

QStringList ClaudeCliProvider::arguments(const AiRequest &request) const
{
    QStringList tools = {u"Bash"_s, u"Read"_s, u"Write"_s, u"Edit"_s, u"Glob"_s, u"Grep"_s};
    if (request.webSearch) {
        tools << u"WebSearch"_s << u"WebFetch"_s;
    }

    QStringList args = {
        u"--print"_s,
        u"--input-format"_s,       u"stream-json"_s,
        u"--output-format"_s,      u"stream-json"_s,
        u"--include-partial-messages"_s,
        u"--verbose"_s,
        // The conversation lives in Atoll and nowhere else. Nothing about a
        // question asked on the island belongs in a transcript the user did
        // not ask for and would not think to look for.
        u"--no-session-persistence"_s,
        // Atoll's assistant is Atoll's assistant: it must behave the same on
        // every machine, so none of the client's own configuration - skills,
        // plugins, hooks, project instructions - is loaded into it.
        u"--setting-sources"_s,    QString(),
        u"--strict-mcp-config"_s,
        u"--disable-slash-commands"_s,
        u"--system-prompt"_s,      request.systemPrompt,
        u"--settings"_s,           hookSettings(),
        u"--tools"_s,              tools.join(u','),
        // The client may only reach into the user's own files by name. Any
        // path outside it is still reachable through a command, which is
        // where the broker asks about it properly.
        u"--add-dir"_s,            QDir::homePath(),
    };

    if (!request.model.isEmpty()) {
        args << u"--model"_s << request.model;
    }
    if (knownEffort(request.effort)) {
        args << u"--effort"_s << request.effort;
    }
    return args;
}

void ClaudeCliProvider::start(const AiRequest &request)
{
    m_status.path = findExecutable(m_configuredPath);
    m_status.installed = !m_status.path.isEmpty();
    if (!m_status.installed) {
        Q_EMIT failed(tr("The Claude Code client is not installed. Atoll's settings show how to "
                         "install it, or you can switch the assistant to an API key."));
        return;
    }

    m_buffer.clear();
    m_errors.clear();
    m_announced.clear();
    m_stopping = false;

    auto *process = new QProcess(this);
    m_process = process;
    process->setProgram(m_status.path);
    process->setArguments(arguments(request));
    process->setWorkingDirectory(workspacePath());

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(u"ATOLL_ASSISTANT"_s, u"1"_s);
    process->setProcessEnvironment(environment);

    connect(process, &QProcess::readyReadStandardOutput, this, &ClaudeCliProvider::consume);
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        m_errors.append(process->readAllStandardError());
        if (m_errors.size() > 8000) {
            m_errors = m_errors.right(8000);
        }
    });
    connect(process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error != QProcess::FailedToStart || m_stopping) {
            return;
        }
        m_turnInFlight = false;
        Q_EMIT failed(tr("Atoll could not start the Claude Code client."));
    });
    connect(process, &QProcess::finished, this, [this, process](int code) {
        process->deleteLater();
        if (m_process == process) {
            m_process = nullptr;
        }
        if (m_stopping || !m_turnInFlight) {
            return;
        }
        // The client left in the middle of a turn, so whatever it printed on
        // the way out is the only explanation anybody is going to get.
        m_turnInFlight = false;
        const QString detail = QString::fromUtf8(m_errors).trimmed();
        Q_EMIT failed(detail.isEmpty()
                          ? tr("The Claude Code client stopped unexpectedly (code %1).").arg(code)
                          : detail.right(400));
    });

    process->start();
    m_startedModel = request.model;
    m_startedPrompt = request.systemPrompt;
}

void ClaudeCliProvider::send(const AiRequest &request)
{
    const bool running = m_process && m_process->state() != QProcess::NotRunning;
    // A conversation is one process: the client keeps the history, and that is
    // what makes a follow-up question cost a question rather than a transcript.
    // It is restarted only when the settings it was started with no longer
    // describe what was asked for.
    if (running && (m_startedModel != request.model || m_startedPrompt != request.systemPrompt)) {
        abort();
    }
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        start(request);
    }
    if (!m_process) {
        return;
    }

    m_turnInFlight = true;
    m_announced.clear();
    if (!request.history.isEmpty()) {
        writeTurn(request.history.last());
    }
}

void ClaudeCliProvider::writeTurn(const AiTurn &turn)
{
    QJsonArray content;
    if (!turn.text.isEmpty()) {
        content.append(QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, turn.text}});
    }
    if (content.isEmpty()) {
        return;
    }

    const QJsonObject message{{u"type"_s, u"user"_s},
                              {u"message"_s,
                               QJsonObject{{u"role"_s, u"user"_s}, {u"content"_s, content}}}};
    m_process->write(QJsonDocument(message).toJson(QJsonDocument::Compact));
    m_process->write("\n");
}

void ClaudeCliProvider::abort()
{
    m_turnInFlight = false;
    if (!m_process) {
        return;
    }
    m_stopping = true;
    QProcess *process = m_process;
    m_process = nullptr;

    // Closing the input is how the client is meant to be told the conversation
    // is over; the rest is for the case where it is busy and does not notice.
    process->closeWriteChannel();
    process->terminate();
    if (!process->waitForFinished(1500)) {
        process->kill();
        process->waitForFinished(500);
    }
    process->deleteLater();
    m_buffer.clear();
    m_stopping = false;
}

// ---- reading what it says ------------------------------------------------

void ClaudeCliProvider::consume()
{
    if (!m_process) {
        return;
    }
    m_buffer.append(m_process->readAllStandardOutput());

    int newline = m_buffer.indexOf('\n');
    while (newline >= 0) {
        const QByteArray line = m_buffer.left(newline);
        m_buffer.remove(0, newline + 1);
        newline = m_buffer.indexOf('\n');
        if (!line.trimmed().isEmpty()) {
            handleLine(line);
        }
    }
}

void ClaudeCliProvider::handleLine(const QByteArray &line)
{
    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(line, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    const QJsonObject object = document.object();
    const QString type = object.value(u"type"_s).toString();

    if (type == u"stream_event"_s) {
        handleStreamEvent(object.value(u"event"_s).toObject());
    } else if (type == u"assistant"_s) {
        handleAssistantMessage(object.value(u"message"_s).toObject());
    } else if (type == u"user"_s) {
        handleUserMessage(object.value(u"message"_s).toObject());
    } else if (type == u"result"_s) {
        handleResult(object);
    }
}

void ClaudeCliProvider::handleStreamEvent(const QJsonObject &event)
{
    if (event.value(u"type"_s).toString() != u"content_block_delta"_s) {
        return;
    }
    const QJsonObject delta = event.value(u"delta"_s).toObject();
    const QString kind = delta.value(u"type"_s).toString();
    if (kind == u"text_delta"_s) {
        Q_EMIT textDelta(delta.value(u"text"_s).toString());
    } else if (kind == u"thinking_delta"_s) {
        Q_EMIT thoughtDelta(delta.value(u"thinking"_s).toString());
    }
}

void ClaudeCliProvider::handleAssistantMessage(const QJsonObject &message)
{
    // Text is taken from the deltas above, never from here: the same words
    // arrive twice, and counting them twice is how an answer ends up doubled.
    const QJsonArray content = message.value(u"content"_s).toArray();
    for (const QJsonValue &value : content) {
        const QJsonObject block = value.toObject();
        if (block.value(u"type"_s).toString() != u"tool_use"_s) {
            continue;
        }
        const QString id = block.value(u"id"_s).toString();
        if (id.isEmpty() || m_announced.contains(id)) {
            continue;
        }
        m_announced.insert(id);
        Q_EMIT toolStarted(id,
                           describe(block.value(u"name"_s).toString(),
                                    block.value(u"input"_s).toObject().toVariantMap()));
    }
}

void ClaudeCliProvider::handleUserMessage(const QJsonObject &message)
{
    const QJsonArray content = message.value(u"content"_s).toArray();
    for (const QJsonValue &value : content) {
        const QJsonObject block = value.toObject();
        if (block.value(u"type"_s).toString() != u"tool_result"_s) {
            continue;
        }
        const QString text = resultText(block.value(u"content"_s)).trimmed();
        Q_EMIT toolFinished(block.value(u"tool_use_id"_s).toString(),
                            !block.value(u"is_error"_s).toBool(),
                            text.left(200));
    }
}

void ClaudeCliProvider::handleResult(const QJsonObject &object)
{
    m_turnInFlight = false;

    if (!object.value(u"is_error"_s).toBool()) {
        Q_EMIT turnEnded(u"end_turn"_s, {}, {});
        return;
    }

    const QString subtype = object.value(u"subtype"_s).toString();
    const QString detail = object.value(u"result"_s).toString().trimmed();

    if (subtype == u"error_max_turns"_s) {
        Q_EMIT failed(tr("The assistant kept going without reaching an answer, so it was stopped."));
        return;
    }
    if (detail.contains(u"rate limit"_s, Qt::CaseInsensitive)
        || detail.contains(u"usage limit"_s, Qt::CaseInsensitive)) {
        Q_EMIT failed(tr("Your Claude account has hit its usage limit for now. It resets on its "
                         "own; until then the assistant cannot answer."));
        return;
    }
    if (detail.contains(u"not logged in"_s, Qt::CaseInsensitive)
        || detail.contains(u"authentication"_s, Qt::CaseInsensitive)) {
        Q_EMIT failed(tr("The Claude Code client is not signed in. Open Atoll's settings to sign in."));
        return;
    }
    Q_EMIT failed(detail.isEmpty() ? tr("The Claude Code client reported an error.")
                                   : detail.left(400));
}

// ---- translating the client's tools --------------------------------------

QString ClaudeCliProvider::describe(const QString &tool, const QVariantMap &input)
{
    const auto fileName = [&input] {
        return QFileInfo(input.value(u"file_path"_s).toString()).fileName();
    };

    if (tool == u"Bash"_s) {
        const QString purpose = input.value(u"description"_s).toString();
        return purpose.isEmpty() ? input.value(u"command"_s).toString().simplified() : purpose;
    }
    if (tool == u"Read"_s) {
        return tr("Read %1").arg(fileName());
    }
    if (tool == u"Write"_s) {
        return tr("Write %1").arg(fileName());
    }
    if (tool == u"Edit"_s) {
        return tr("Change %1").arg(fileName());
    }
    if (tool == u"Glob"_s || tool == u"Grep"_s) {
        return tr("Look for %1").arg(input.value(u"pattern"_s).toString());
    }
    if (tool == u"WebSearch"_s) {
        return tr("Search the web for %1").arg(input.value(u"query"_s).toString());
    }
    if (tool == u"WebFetch"_s) {
        return tr("Read %1").arg(QUrl(input.value(u"url"_s).toString()).host());
    }
    return tool;
}

AiToolCall ClaudeCliProvider::toAtollCall(const QString &id,
                                          const QString &tool,
                                          const QVariantMap &input)
{
    AiToolCall call;
    call.id = id;

    if (tool == u"Bash"_s) {
        call.name = u"run_command"_s;
        call.input.insert(u"command"_s, input.value(u"command"_s));
        call.input.insert(u"purpose"_s, input.value(u"description"_s));
        return call;
    }
    if (tool == u"Read"_s) {
        call.name = u"read_file"_s;
        call.input.insert(u"path"_s, input.value(u"file_path"_s));
        return call;
    }
    if (tool == u"Write"_s || tool == u"Edit"_s || tool == u"NotebookEdit"_s) {
        call.name = u"write_file"_s;
        call.input.insert(u"path"_s, input.value(u"file_path"_s));
        return call;
    }
    if (tool == u"Glob"_s || tool == u"Grep"_s) {
        call.name = u"list_directory"_s;
        const QString path = input.value(u"path"_s).toString();
        call.input.insert(u"path"_s, path.isEmpty() ? QDir::homePath() : path);
        return call;
    }
    if (tool == u"WebSearch"_s) {
        call.name = u"web_search"_s;
        call.input.insert(u"query"_s, input.value(u"query"_s));
        return call;
    }
    if (tool == u"WebFetch"_s) {
        call.name = u"fetch_url"_s;
        call.input.insert(u"url"_s, input.value(u"url"_s));
        return call;
    }

    // Anything the client grows that Atoll has not been taught about lands in
    // the broker's own catch-all, which asks the user rather than assuming.
    call.name = tool;
    call.input = input;
    return call;
}
