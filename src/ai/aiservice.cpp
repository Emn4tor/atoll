/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "aiservice.h"

#include "ai/aitools.h"
#include "ai/anthropicprovider.h"
#include "ai/credentialstore.h"
#include "ai/geminiprovider.h"
#include "ai/permissionbroker.h"
#include "ai/screencapture.h"
#include "config/config.h"

#include <QNetworkAccessManager>
#include <QTimer>

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

    for (AiProvider *provider : {static_cast<AiProvider *>(m_anthropic), static_cast<AiProvider *>(m_gemini)}) {
        connect(provider, &AiProvider::textDelta, this, [this](const QString &text) {
            if (m_testing) {
                return;
            }
            if (m_answer.isEmpty() && !text.trimmed().isEmpty()) {
                setState(u"answering"_s);
            }
            m_answer.append(text);
            Q_EMIT answerChanged();
        });
        connect(provider, &AiProvider::thoughtDelta, this, [this](const QString &text) {
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
        connect(provider, &AiProvider::turnEnded, this, &AiService::onTurnEnded);
        connect(provider, &AiProvider::failed, this, [this](const QString &reason) {
            if (m_testing) {
                m_testing = false;
                m_keyTest = reason;
                Q_EMIT keyTestChanged();
                return;
            }
            fail(reason);
        });
    }

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
    const QString configured = m_config->value(u"ai.provider"_s, u"anthropic"_s).toString();
    return configured == u"gemini"_s ? u"gemini"_s : u"anthropic"_s;
}

QString AiService::providerLabel() const
{
    return provider() == u"gemini"_s ? u"Gemini"_s : u"Claude"_s;
}

AiProvider *AiService::activeProvider() const
{
    return provider() == u"gemini"_s ? static_cast<AiProvider *>(m_gemini)
                                     : static_cast<AiProvider *>(m_anthropic);
}

QString AiService::model() const
{
    const QString configured = m_config->value(u"ai.model"_s, QString()).toString();
    return configured.isEmpty() ? activeProvider()->defaultModel() : configured;
}

bool AiService::configured() const
{
    return (m_config->value(u"ai.enabled"_s, true).toBool()) && m_credentials->hasKey(provider());
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
    AiProvider *target = activeProvider();
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

void AiService::addStep(const QString &kind, const QString &text, const QString &status)
{
    m_steps.append(QVariantMap{{u"kind"_s, kind}, {u"text"_s, text}, {u"status"_s, status}});
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
    m_toolbox->cancel();
    m_queue.clear();
    m_results.clear();
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
                m_history.last().image = png;
                m_history.last().imageMediaType = u"image/png"_s;
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
    AiProvider *target = activeProvider();
    target->setApiKey(m_credentials->key(provider()));

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
    turn.rawProvider = activeProvider()->id();
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
    executeNow(m_current, verdict);
}

void AiService::deny()
{
    if (!m_awaitingPermission) {
        return;
    }
    m_awaitingPermission = false;
    addStep(u"tool"_s,
            m_pending.summary.isEmpty() ? m_current.name : m_pending.summary,
            u"denied"_s);
    m_pending = {};
    Q_EMIT pendingChanged();

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

void AiService::fail(const QString &reason)
{
    m_error = reason;
    m_queue.clear();
    m_results.clear();
    m_awaitingPermission = false;
    m_pending = {};
    setActivity({});
    setState(u"failed"_s);
    Q_EMIT pendingChanged();
    if (m_background) {
        Q_EMIT messageRequested(tr("%1 stopped").arg(providerLabel()), reason);
    }
}
