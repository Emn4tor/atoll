/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "aiservice.h"

#include "ai/aitools.h"
#include "ai/anthropicprovider.h"
#include "ai/claudecliprovider.h"
#include "ai/credentialstore.h"
#include "ai/geminiprovider.h"
#include "ai/permissionbroker.h"
#include "ai/screencapture.h"
#include "config/config.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <memory>

using namespace Qt::StringLiterals;

namespace
{
/**
 * How many times the model may ask for tools before Atoll stops it.
 *
 * A model that has misread the situation will happily try the same thing until
 * something gives, and on a desktop the thing that gives is the user's
 * patience or their package database. The ceiling is high enough for a real
 * job - install, configure, verify - and low enough to be noticed.
 */
constexpr int MaxToolRounds = 24;
}

AiService::AiService(Config *config, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_network(new QNetworkAccessManager(this))
    , m_credentials(new CredentialStore(this))
    , m_broker(new PermissionBroker(config, this))
    , m_toolbox(new AiToolbox(config, this))
{
    m_anthropic = new AnthropicProvider(m_network, this);
    m_gemini = new GeminiProvider(m_network, this);
    m_cli = new ClaudeCliProvider(this);

    for (AiBackend *provider : {static_cast<AiBackend *>(m_anthropic),
                                static_cast<AiBackend *>(m_gemini),
                                static_cast<AiBackend *>(m_cli)}) {
        connect(provider, &AiBackend::textDelta, this, [this](const QString &text) {
            if (m_testing) {
                return;
            }
            if (m_answer.isEmpty() && !text.trimmed().isEmpty()) {
                setState(u"answering"_s);
            }
            m_answer.append(text);
            Q_EMIT answerChanged();
        });
        connect(provider, &AiBackend::thoughtDelta, this, [this](const QString &text) {
            if (m_testing) {
                return;
            }
            // Only the tail is ever shown, so there is no point keeping more.
            m_thought.append(text);
            if (m_thought.size() > 600) {
                m_thought = m_thought.right(600);
            }
            Q_EMIT answerChanged();
        });
        connect(provider, &AiBackend::turnEnded, this, &AiService::onTurnEnded);
        connect(provider, &AiBackend::failed, this, [this](const QString &reason) {
            if (m_testing) {
                m_testing = false;
                m_keyTest = reason;
                Q_EMIT keyTestChanged();
                return;
            }
            fail(reason);
        });
    }

    // A backend that runs its own tools reports them rather than handing them
    // over, so the island's list of steps is filled in from what it says.
    connect(m_cli, &AiBackend::toolStarted, this, [this](const QString &id, const QString &summary) {
        addStep(u"tool"_s, summary, u"running"_s, id);
        setActivity(summary);
        setState(u"working"_s);
    });
    connect(m_cli, &AiBackend::toolFinished, this, [this](const QString &id, bool ok, const QString &) {
        updateStepById(id, ok ? u"done"_s : u"failed"_s);
        setActivity({});
    });
    connect(m_cli, &ClaudeCliProvider::statusChanged, this, [this] {
        if (m_cliTesting) {
            m_cliTesting = false;
            m_keyTest = cliDetail();
            Q_EMIT keyTestChanged();
        }
        Q_EMIT cliChanged();
        Q_EMIT configurationChanged();
    });

    connect(m_toolbox, &AiToolbox::completed, this, &AiService::onToolResult);
    connect(m_toolbox, &AiToolbox::progress, this, [this](const QString &line) {
        if (!line.isEmpty()) {
            setActivity(line);
        }
    });
    connect(m_toolbox, &AiToolbox::messageRequested, this, &AiService::messageRequested);

    connect(m_config, &Config::changed, this, &AiService::configurationChanged);
}

AiService::~AiService() = default;

// ---- configuration -------------------------------------------------------

QString AiService::provider() const
{
    const QString configured = m_config->value(u"ai.provider"_s, u"claude-cli"_s).toString();
    if (configured == u"gemini"_s || configured == u"anthropic"_s) {
        return configured;
    }
    return u"claude-cli"_s;
}

QString AiService::providerLabel() const
{
    const QString name = provider();
    if (name == u"gemini"_s) {
        return u"Gemini"_s;
    }
    return u"Claude"_s;
}

AiBackend *AiService::activeBackend() const
{
    const QString name = provider();
    if (name == u"gemini"_s) {
        return m_gemini;
    }
    if (name == u"anthropic"_s) {
        return m_anthropic;
    }
    return m_cli;
}

QString AiService::model() const
{
    const QString configured = m_config->value(u"ai.model"_s, QString()).toString();
    return configured.isEmpty() ? activeBackend()->defaultModel() : configured;
}

bool AiService::configured() const
{
    if (!m_config->value(u"ai.enabled"_s, true).toBool()) {
        return false;
    }
    if (provider() == u"claude-cli"_s) {
        // Whether the client is signed in is a question that costs a process
        // to answer, so it is not asked here. A client that is installed is
        // treated as usable, and a login that turns out to be missing is
        // reported the moment something is actually asked of it.
        return m_cli->status().installed;
    }
    return m_credentials->hasKey(provider());
}

bool AiService::hasKeyFor(const QString &name) const
{
    return m_credentials->hasKey(name);
}

QString AiService::keyBackendFor(const QString &name) const
{
    return m_credentials->backendFor(name);
}

void AiService::setKeyFor(const QString &name, const QString &key)
{
    m_credentials->setKey(name, key.trimmed());
    m_keyTest.clear();
    Q_EMIT keyTestChanged();
    Q_EMIT configurationChanged();
}

void AiService::testKey()
{
    if (provider() == u"claude-cli"_s) {
        m_cliTesting = true;
        m_keyTest = tr("Checking…");
        Q_EMIT keyTestChanged();
        m_cli->refreshStatus();
        return;
    }

    auto *target = qobject_cast<AiProvider *>(activeBackend());
    target->setApiKey(m_credentials->key(provider()));

    m_testing = true;
    m_keyTest = tr("Checking…");
    Q_EMIT keyTestChanged();

    AiRequest request;
    request.model = model();
    request.baseUrl = m_config->value(u"ai.baseUrl"_s, QString()).toString();
    request.maxTokens = 64;
    request.systemPrompt = u"Reply with the single word: ready."_s;
    AiTurn turn;
    turn.role = u"user"_s;
    turn.text = u"Are you there?"_s;
    request.history.append(turn);
    target->send(request);
}

// ---- the command-line client ---------------------------------------------

QString AiService::cliState() const
{
    const CliStatus status = m_cli->status();
    if (!status.installed) {
        return u"missing"_s;
    }
    if (!status.loggedIn) {
        // Nothing has been asked yet, so "not signed in" would be a guess.
        return status.error.isEmpty() && status.method.isEmpty() ? u"checking"_s : u"signed-out"_s;
    }
    return u"ready"_s;
}

QString AiService::cliDetail() const
{
    const CliStatus status = m_cli->status();
    if (!status.installed) {
        return tr("The Claude Code client is not installed yet.");
    }
    if (!status.error.isEmpty()) {
        return status.error;
    }
    if (!status.loggedIn) {
        return cliState() == u"checking"_s
            ? tr("Found at %1. Check the sign-in to be sure it can answer.").arg(status.path)
            : tr("Found at %1, but nobody is signed in.").arg(status.path);
    }
    if (!status.account.isEmpty() && !status.plan.isEmpty()) {
        return tr("Signed in as %1, on a %2 plan.").arg(status.account, status.plan);
    }
    if (!status.account.isEmpty()) {
        return tr("Signed in as %1.").arg(status.account);
    }
    return tr("Signed in and ready.");
}

void AiService::refreshCli()
{
    m_cli->setExecutablePath(m_config->value(u"ai.cliPath"_s, QString()).toString());
    m_cli->refreshStatus();
    Q_EMIT cliChanged();
}

QString AiService::cliInstallCommand() const
{
    return u"curl -fsSL https://claude.ai/install.sh | bash"_s;
}

bool AiService::signInToCli()
{
    const CliStatus status = m_cli->status();
    const QString client = status.installed ? status.path : u"claude"_s;
    // Signing in is a conversation with a browser and a code to paste back, so
    // it belongs in a terminal the user can see and type into - not in a
    // process Atoll started behind them.
    const QString command = u"%1 auth login; echo; echo 'You can close this window.'; read -r _"_s
                                .arg(client);

    struct Terminal {
        const char *program;
        QStringList before;
    };
    // xdg-terminal-exec first: it opens whatever this desktop calls its
    // terminal, which on a KDE install is the one the user already knows.
    static const QList<Terminal> terminals = {
        {"xdg-terminal-exec", {}},
        {"konsole", {u"-e"_s}},
        {"ptyxis", {u"--"_s}},
        {"gnome-terminal", {u"--"_s}},
        {"alacritty", {u"-e"_s}},
        {"kitty", {}},
        {"foot", {}},
        {"xterm", {u"-e"_s}},
    };

    for (const Terminal &terminal : terminals) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(terminal.program));
        if (path.isEmpty()) {
            continue;
        }
        QStringList arguments = terminal.before;
        arguments << u"sh"_s << u"-c"_s << command;
        if (QProcess::startDetached(path, arguments)) {
            return true;
        }
    }
    return false;
}

bool AiService::screenAvailable() const
{
    return ScreenCapture::available();
}

void AiService::setShareScreen(bool share)
{
    if (m_shareScreen == share) {
        return;
    }
    m_shareScreen = share;
    Q_EMIT shareScreenChanged();
}

// ---- state ---------------------------------------------------------------

bool AiService::glowing() const
{
    return m_engaged && !m_background;
}

bool AiService::busy() const
{
    return m_state == u"thinking"_s || m_state == u"answering"_s || m_state == u"working"_s
        || m_state == u"permission"_s;
}

int AiService::exchanges() const
{
    int count = 0;
    for (const AiTurn &turn : m_history) {
        if (turn.role == u"user"_s && !turn.text.isEmpty()) {
            ++count;
        }
    }
    return count;
}

bool AiService::unattended() const
{
    return m_broker->grantedEverything();
}

QString AiService::pendingTier() const
{
    return PermissionBroker::tierTitle(m_pending.risk);
}

void AiService::setState(const QString &state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged();
}

void AiService::setActivity(const QString &line)
{
    const QString trimmed = line.left(160);
    if (m_activity == trimmed) {
        return;
    }
    m_activity = trimmed;
    Q_EMIT activityChanged();
}

void AiService::addStep(const QString &kind,
                        const QString &text,
                        const QString &status,
                        const QString &id)
{
    m_steps.append(QVariantMap{{u"kind"_s, kind},
                               {u"text"_s, text},
                               {u"status"_s, status},
                               {u"id"_s, id}});
    if (m_steps.size() > 40) {
        m_steps.removeFirst();
    }
    Q_EMIT stepsChanged();
}

void AiService::updateLastStep(const QString &status, const QString &text)
{
    if (m_steps.isEmpty()) {
        return;
    }
    QVariantMap step = m_steps.last().toMap();
    step.insert(u"status"_s, status);
    if (!text.isEmpty()) {
        step.insert(u"text"_s, text);
    }
    m_steps[m_steps.size() - 1] = step;
    Q_EMIT stepsChanged();
}

void AiService::updateStepById(const QString &id, const QString &status)
{
    if (id.isEmpty()) {
        return;
    }
    for (int index = m_steps.size() - 1; index >= 0; --index) {
        QVariantMap step = m_steps.at(index).toMap();
        if (step.value(u"id"_s).toString() != id) {
            continue;
        }
        step.insert(u"status"_s, status);
        m_steps[index] = step;
        Q_EMIT stepsChanged();
        return;
    }
}

// ---- the island's verbs --------------------------------------------------

void AiService::engage()
{
    m_engaged = true;
    m_background = false;

    if (!configured()) {
        setState(u"setup"_s);
        Q_EMIT stateChanged();
        Q_EMIT setupRequested();
        return;
    }

    if (!busy()) {
        m_error.clear();
        setState(u"composing"_s);
        Q_EMIT focusRequested();
    }
    Q_EMIT stateChanged();
}

void AiService::dismiss()
{
    m_engaged = false;
    // Work that is under way is not thrown out just because the panel closed;
    // it moves to the island, which is what "continue in background" does by
    // hand and what closing the panel should do by itself.
    m_background = busy();
    if (!busy()) {
        setState(u"composing"_s);
    }
    Q_EMIT stateChanged();
}

void AiService::continueInBackground()
{
    if (!busy()) {
        dismiss();
        return;
    }
    m_background = true;
    Q_EMIT stateChanged();
}

void AiService::bringToFront()
{
    m_engaged = true;
    m_background = false;
    Q_EMIT stateChanged();
}

void AiService::startOver()
{
    cancel();
    m_history.clear();
    m_steps.clear();
    m_answer.clear();
    m_thought.clear();
    m_question.clear();
    m_error.clear();
    m_rounds = 0;
    m_broker->revokeAll();
    setState(u"composing"_s);
    Q_EMIT stepsChanged();
    Q_EMIT answerChanged();
    Q_EMIT conversationChanged();
}

void AiService::cancel()
{
    m_anthropic->abort();
    m_gemini->abort();
    m_cli->abort();
    m_toolbox->cancel();
    m_queue.clear();
    m_results.clear();
    // Every tool call the client is holding open has to be told something, or
    // it sits there until its own patience runs out.
    if (!m_reviewToken.isEmpty()) {
        answerReview(m_reviewToken, false, tr("The user stopped the assistant."));
        m_reviewToken.clear();
    }
    while (!m_reviews.isEmpty()) {
        answerReview(m_reviews.dequeue().token, false, tr("The user stopped the assistant."));
    }
    m_awaitingPermission = false;
    m_pending = {};
    m_background = false;
    setActivity({});
    Q_EMIT pendingChanged();
    if (m_state != u"composing"_s && m_state != u"setup"_s) {
        setState(m_answer.isEmpty() ? u"composing"_s : u"done"_s);
    }
}

void AiService::ask(const QString &text)
{
    const QString question = text.trimmed();
    if (question.isEmpty()) {
        return;
    }
    if (!configured()) {
        // A question asked from outside - a shortcut, a script - should still
        // put the island in front of the user, or the request vanishes with no
        // sign that anything was wrong with it.
        engage();
        return;
    }

    m_engaged = true;
    m_background = false;
    m_error.clear();
    m_question = question;
    m_answer.clear();
    m_thought.clear();
    m_steps.clear();
    m_rounds = 0;
    Q_EMIT stepsChanged();
    Q_EMIT answerChanged();
    Q_EMIT conversationChanged();

    AiTurn turn;
    turn.role = u"user"_s;
    turn.text = question;
    m_history.append(turn);

    if (m_shareScreen) {
        // The picture has to be taken before the question is sent, and taking
        // it may put a portal dialog in front of the user first.
        setState(u"working"_s);
        setActivity(tr("Taking a picture of the screen…"));
        auto *capture = new ScreenCapture(this);
        connect(capture, &ScreenCapture::captured, this, [this, capture](const QByteArray &png) {
            capture->deleteLater();
            if (!m_history.isEmpty()) {
                if (activeBackend()->drivesTools()) {
                    // The client reads pictures off disk rather than out of a
                    // message, so the screenshot is left somewhere it can open
                    // and the question says where.
                    const QString path =
                        QDir(ClaudeCliProvider::workspacePath()).filePath(u"screen.png"_s);
                    QFile file(path);
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(png);
                        file.close();
                        m_history.last().text +=
                            tr("\n\n(A picture of the screen as it is right now has been saved to "
                               "%1. Open it to see what I am looking at.)")
                                .arg(path);
                    }
                } else {
                    m_history.last().image = png;
                    m_history.last().imageMediaType = u"image/png"_s;
                }
            }
            setShareScreen(false);
            setActivity({});
            runTurn();
        });
        connect(capture, &ScreenCapture::failed, this, [this, capture](const QString &reason) {
            capture->deleteLater();
            setShareScreen(false);
            addStep(u"note"_s, reason, u"failed"_s);
            setActivity({});
            runTurn();
        });
        capture->capture();
        return;
    }

    runTurn();
}

// ---- the loop ------------------------------------------------------------

void AiService::runTurn()
{
    AiBackend *target = activeBackend();
    if (auto *keyed = qobject_cast<AiProvider *>(target)) {
        keyed->setApiKey(m_credentials->key(provider()));
    }
    if (auto *client = qobject_cast<ClaudeCliProvider *>(target)) {
        client->setExecutablePath(m_config->value(u"ai.cliPath"_s, QString()).toString());
    }

    AiRequest request;
    request.model = model();
    request.maxTokens = m_config->value(u"ai.maxTokens"_s, 16000).toInt();
    request.effort = m_config->value(u"ai.effort"_s, u"high"_s).toString();
    request.webSearch = m_config->value(u"ai.webSearch"_s, true).toBool();
    request.baseUrl = m_config->value(u"ai.baseUrl"_s, QString()).toString();
    request.systemPrompt =
        AiToolbox::systemPrompt(m_config->value(u"ai.systemPrompt"_s, QString()).toString());
    request.tools = AiToolbox::definitions(m_config->value(u"ai.allowScreenshots"_s, true).toBool()
                                           && ScreenCapture::available());
    request.history = m_history;

    // A backend that runs its own tools was given them when it started, so the
    // catalogue above is not its business and neither is the round counter:
    // it stops itself.
    if (target->drivesTools()) {
        request.tools = {};
        request.systemPrompt +=
            AiToolbox::clientAddendum(m_config->value(u"ai.allowScreenshots"_s, true).toBool()
                                      && ScreenCapture::available());
    }

    setState(m_answer.isEmpty() ? u"thinking"_s : u"answering"_s);
    target->send(request);
}

void AiService::onTurnEnded(const QString &stopReason,
                            const QList<AiToolCall> &calls,
                            const QJsonArray &raw)
{
    if (m_testing) {
        m_testing = false;
        m_keyTest = tr("The key works.");
        Q_EMIT keyTestChanged();
        return;
    }

    AiTurn turn;
    turn.role = u"assistant"_s;
    turn.text = m_answer;
    turn.toolCalls = calls;
    turn.rawContent = raw;
    turn.rawProvider = activeBackend()->id();
    m_history.append(turn);

    if (stopReason == u"pause_turn"_s) {
        // The provider's own tools ran and it wants to keep going; the history
        // it needs is already back in place.
        runTurn();
        return;
    }

    if (stopReason == u"refusal"_s) {
        fail(tr("The assistant declined to answer that."));
        return;
    }

    if (calls.isEmpty()) {
        setActivity({});
        setState(u"done"_s);
        if (m_background) {
            Q_EMIT messageRequested(tr("%1 finished").arg(providerLabel()),
                                    m_answer.left(120));
        }
        return;
    }

    if (++m_rounds > MaxToolRounds) {
        fail(tr("The assistant kept going without reaching an answer, so Atoll stopped it."));
        return;
    }

    m_results.clear();
    m_queue.clear();
    for (const AiToolCall &call : calls) {
        m_queue.enqueue(call);
    }
    setState(u"working"_s);
    advanceQueue();
}

void AiService::advanceQueue()
{
    if (m_queue.isEmpty()) {
        finishToolRound();
        return;
    }

    m_current = m_queue.dequeue();
    const AiVerdict verdict = m_broker->classify(m_current);

    if (verdict.risk == AiRisk::Forbidden || !m_broker->tierEnabled(verdict.risk)) {
        const QString reason = verdict.risk == AiRisk::Forbidden
            ? (verdict.refusal.isEmpty() ? tr("Atoll refuses this action.") : verdict.refusal)
            : tr("The user's settings do not allow this (%1). Suggest what they could change, or "
                 "find another way.")
                  .arg(PermissionBroker::tierTitle(verdict.risk));
        addStep(u"tool"_s, verdict.summary.isEmpty() ? m_current.name : verdict.summary, u"denied"_s);

        AiToolResult result;
        result.id = m_current.id;
        result.content = reason;
        result.isError = true;
        m_results.append(result);
        advanceQueue();
        return;
    }

    if (m_broker->isPreApproved(verdict)) {
        executeNow(m_current, verdict);
        return;
    }

    m_pending = verdict;
    m_awaitingPermission = true;
    // A question is worth surfacing even when the user has looked away.
    if (m_background) {
        m_background = false;
        m_engaged = true;
    }
    setState(u"permission"_s);
    Q_EMIT pendingChanged();
    Q_EMIT stateChanged();
}

void AiService::executeNow(const AiToolCall &call, const AiVerdict &verdict)
{
    addStep(u"tool"_s,
            verdict.summary.isEmpty() ? call.name : verdict.summary,
            verdict.risk == AiRisk::Admin ? u"elevated"_s : u"running"_s);
    setActivity(verdict.summary.isEmpty() ? call.name : verdict.summary);
    setState(u"working"_s);
    m_toolbox->execute(call, verdict.risk);
}

void AiService::allow(bool rememberForSession)
{
    if (!m_awaitingPermission) {
        return;
    }
    m_awaitingPermission = false;
    if (rememberForSession) {
        m_broker->grantForSession(m_pending.grantKey);
    }
    const AiVerdict verdict = m_pending;
    m_pending = {};
    Q_EMIT pendingChanged();

    if (!m_reviewToken.isEmpty()) {
        // The client is holding the call and will make it itself; all it needs
        // from here is the word.
        const QString token = m_reviewToken;
        m_reviewToken.clear();
        answerReview(token, true, tr("Allowed on the island."));
        setState(u"working"_s);
        showNextReview();
        return;
    }

    executeNow(m_current, verdict);
}

void AiService::allowEverything()
{
    if (!m_awaitingPermission) {
        return;
    }
    m_broker->grantEverythingForSession();
    // Everything queued behind this one is now pre-approved as well, and
    // allow() drains the queue for us.
    allow(false);
    Q_EMIT stateChanged();
}

void AiService::deny()
{
    if (!m_awaitingPermission) {
        return;
    }
    m_awaitingPermission = false;
    const QString summary = m_pending.summary.isEmpty() ? m_current.name : m_pending.summary;
    m_pending = {};
    Q_EMIT pendingChanged();

    if (!m_reviewToken.isEmpty()) {
        const QString token = m_reviewToken;
        m_reviewToken.clear();
        updateStepById(m_current.id, u"denied"_s);
        answerReview(token,
                     false,
                     tr("The user did not allow this. Do not try it again. Continue without it, "
                        "or tell them what you would need."));
        setState(u"working"_s);
        showNextReview();
        return;
    }

    addStep(u"tool"_s, summary, u"denied"_s);

    AiToolResult result;
    result.id = m_current.id;
    // Phrased as a fact rather than an error: the model should carry on and
    // suggest something else, not treat the refusal as a fault to work around.
    result.content = tr("The user did not allow this. Do not try it again. Continue without it, or "
                        "tell them what you would need.");
    result.isError = true;
    m_results.append(result);

    setState(u"working"_s);
    advanceQueue();
}

void AiService::onToolResult(const AiToolResult &result)
{
    updateLastStep(result.isError ? u"failed"_s : u"done"_s);
    m_results.append(result);
    setActivity({});
    advanceQueue();
}

void AiService::finishToolRound()
{
    AiTurn turn;
    turn.role = u"user"_s;
    turn.toolResults = m_results;
    m_results.clear();
    m_history.append(turn);
    runTurn();
}

// ---- judging what the client wants to do ---------------------------------

void AiService::reviewToolCall(const QString &payload, const QString &token)
{
    const QJsonObject request = QJsonDocument::fromJson(payload.toUtf8()).object();
    const QString tool = request.value(u"tool_name"_s).toString();
    const QVariantMap input = request.value(u"tool_input"_s).toObject().toVariantMap();
    const QString useId = request.value(u"tool_use_id"_s).toString();

    // Nothing else on this machine has any business asking, and a question
    // with no conversation behind it cannot be shown to anybody either.
    if (!m_cli->busy()) {
        answerReview(token, false, tr("Atoll has no assistant session waiting for this."));
        return;
    }

    const AiToolCall call = ClaudeCliProvider::toAtollCall(useId, tool, input);
    const AiVerdict verdict = m_broker->classify(call);

    if (verdict.risk == AiRisk::Forbidden || !m_broker->tierEnabled(verdict.risk)) {
        updateStepById(useId, u"denied"_s);
        answerReview(token,
                     false,
                     verdict.risk == AiRisk::Forbidden
                         ? (verdict.refusal.isEmpty() ? tr("Atoll refuses this action.")
                                                      : verdict.refusal)
                         : tr("The user's settings do not allow this (%1). Suggest what they could "
                              "change, or find another way.")
                               .arg(PermissionBroker::tierTitle(verdict.risk)));
        return;
    }

    if (m_broker->isPreApproved(verdict)) {
        answerReview(token, true, tr("Allowed without asking, by the user's settings."));
        return;
    }

    m_reviews.enqueue(PendingReview{token, call, verdict});
    showNextReview();
}

void AiService::captureScreenFor(const QString &token)
{
    // Exactly one answer goes back, whichever way this ends. The caller is a
    // command sitting there waiting, and a screenshot that neither arrives nor
    // fails would hold the whole conversation open behind it.
    auto answered = std::make_shared<bool>(false);
    const auto answer = [this, token, answered](const QString &result) {
        if (*answered) {
            return;
        }
        *answered = true;
        setActivity({});
        Q_EMIT screenCaptureAnswered(token, result);
    };
    const auto fail = [answer](const QString &reason) {
        answer(u"error: "_s + reason);
    };

    if (!m_config->value(u"ai.allowScreenshots"_s, true).toBool()) {
        fail(tr("the user has switched off letting the assistant look at the screen."));
        return;
    }
    if (!ScreenCapture::available()) {
        fail(tr("nothing on this machine can take a screenshot."));
        return;
    }

    // Always the same file. The assistant reads it straight after asking for
    // it, so keeping one around beats leaving a trail of pictures of somebody's
    // screen in a temporary directory.
    const QString target = QDir(ClaudeCliProvider::workspacePath()).filePath(u"screen.png"_s);

    setActivity(tr("Taking a picture of the screen…"));
    auto *capture = new ScreenCapture(this);
    connect(capture, &ScreenCapture::captured, this, [capture, target, answer, fail](const QByteArray &png) {
        capture->deleteLater();
        QFile file(target);
        if (!file.open(QIODevice::WriteOnly)) {
            fail(QCoreApplication::translate("AiService",
                                             "the picture could not be written to %1.")
                     .arg(target));
            return;
        }
        file.write(png);
        file.close();
        answer(target);
    });
    connect(capture, &ScreenCapture::failed, this, [capture, fail](const QString &reason) {
        capture->deleteLater();
        fail(reason);
    });

    // The consent dialog belongs to the desktop and there is no telling whether
    // anybody is in front of it. Waiting a minute for an answer is generous;
    // waiting for ever is a hung assistant.
    QTimer::singleShot(60000, this, [fail] {
        fail(QCoreApplication::translate(
            "AiService", "nobody answered the desktop's request to share the screen."));
    });

    capture->capture();
}

void AiService::showNextReview()
{
    if (m_awaitingPermission || m_reviews.isEmpty()) {
        return;
    }

    const PendingReview review = m_reviews.dequeue();
    m_reviewToken = review.token;
    m_current = review.call;
    m_pending = review.verdict;
    m_awaitingPermission = true;
    if (m_background) {
        m_background = false;
        m_engaged = true;
    }
    setState(u"permission"_s);
    Q_EMIT pendingChanged();
    Q_EMIT stateChanged();
}

void AiService::answerReview(const QString &token, bool allowed, const QString &reason)
{
    const QJsonObject verdict{{u"decision"_s, allowed ? u"allow"_s : u"deny"_s},
                              {u"reason"_s, reason}};
    Q_EMIT toolReviewAnswered(
        token, QString::fromUtf8(QJsonDocument(verdict).toJson(QJsonDocument::Compact)));
}

void AiService::fail(const QString &reason)
{
    m_error = reason;
    m_queue.clear();
    m_results.clear();
    if (!m_reviewToken.isEmpty()) {
        answerReview(m_reviewToken, false, tr("The assistant stopped."));
        m_reviewToken.clear();
    }
    while (!m_reviews.isEmpty()) {
        answerReview(m_reviews.dequeue().token, false, tr("The assistant stopped."));
    }
    m_awaitingPermission = false;
    m_pending = {};
    setActivity({});
    setState(u"failed"_s);
    Q_EMIT pendingChanged();
    if (m_background) {
        Q_EMIT messageRequested(tr("%1 stopped").arg(providerLabel()), reason);
    }
}
