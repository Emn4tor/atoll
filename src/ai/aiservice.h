/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aiprovider.h"
#include "ai/aitypes.h"

#include <QObject>
#include <QQueue>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

class AiToolbox;
class AnthropicProvider;
class Config;
class CredentialStore;
class GeminiProvider;
class PermissionBroker;
class QNetworkAccessManager;

/**
 * The assistant, as the island sees it.
 *
 * One property tells the UI what to draw (`state`), one carries the answer as
 * it arrives, and one describes whatever the assistant is currently asking
 * permission for. Everything else - which service is answering, what the model
 * is allowed to touch, how a long job keeps running once the user has looked
 * away - is arranged behind those three.
 *
 * The loop itself is deliberately plain: send the conversation, let the model
 * ask for tools, put every result back in one message, send it again. What is
 * not plain is who decides whether a tool runs at all, and that lives in
 * PermissionBroker rather than here.
 */
class AiService : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.ai")

    /** Whether a provider is configured well enough to be asked anything. */
    Q_PROPERTY(bool configured READ configured NOTIFY configurationChanged)
    Q_PROPERTY(QString provider READ provider NOTIFY configurationChanged)
    Q_PROPERTY(QString providerLabel READ providerLabel NOTIFY configurationChanged)
    Q_PROPERTY(QString model READ model NOTIFY configurationChanged)

    /**
     * setup | composing | thinking | answering | permission | working | done | failed
     * Only meaningful while `engaged`.
     */
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    /** True from the long press until the user dismisses the assistant. */
    Q_PROPERTY(bool engaged READ engaged NOTIFY stateChanged)
    /** True once the user sent it away; the island keeps the progress only. */
    Q_PROPERTY(bool background READ background NOTIFY stateChanged)
    /** Whether the screen edges should be lit. */
    Q_PROPERTY(bool glowing READ glowing NOTIFY stateChanged)
    /** Whether anything is still in flight. */
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)

    Q_PROPERTY(QString question READ question NOTIFY conversationChanged)
    Q_PROPERTY(QString answer READ answer NOTIFY answerChanged)
    /** The model's own summary of what it is doing, when it offers one. */
    Q_PROPERTY(QString thought READ thought NOTIFY answerChanged)
    /** One line about the step in progress: "Installing firefox…". */
    Q_PROPERTY(QString activity READ activity NOTIFY activityChanged)
    Q_PROPERTY(QString error READ error NOTIFY stateChanged)
    /** Everything the assistant has done this turn, for the expanded view. */
    Q_PROPERTY(QVariantList steps READ steps NOTIFY stepsChanged)
    Q_PROPERTY(int exchanges READ exchanges NOTIFY conversationChanged)

    // ---- the permission the island is currently showing -------------------
    Q_PROPERTY(QString pendingSummary READ pendingSummary NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingDetail READ pendingDetail NOTIFY pendingChanged)
    Q_PROPERTY(QString pendingTier READ pendingTier NOTIFY pendingChanged)
    /** safe | user | admin */
    Q_PROPERTY(QString pendingRisk READ pendingRisk NOTIFY pendingChanged)
    /** Whether saying yes will bring up a system password prompt. */
    Q_PROPERTY(bool pendingElevated READ pendingElevated NOTIFY pendingChanged)

    /** Whether the next question carries a picture of the screen. */
    Q_PROPERTY(bool shareScreen READ shareScreen WRITE setShareScreen NOTIFY shareScreenChanged)
    Q_PROPERTY(bool screenAvailable READ screenAvailable CONSTANT)

public:
    explicit AiService(Config *config, QObject *parent = nullptr);
    ~AiService() override;

    bool configured() const;
    QString provider() const;
    QString providerLabel() const;
    QString model() const;

    QString state() const
    {
        return m_state;
    }
    bool engaged() const
    {
        return m_engaged;
    }
    bool background() const
    {
        return m_background;
    }
    bool glowing() const;
    bool busy() const;

    QString question() const
    {
        return m_question;
    }
    QString answer() const
    {
        return m_answer;
    }
    QString thought() const
    {
        return m_thought;
    }
    QString activity() const
    {
        return m_activity;
    }
    QString error() const
    {
        return m_error;
    }
    QVariantList steps() const
    {
        return m_steps;
    }
    int exchanges() const;

    QString pendingSummary() const
    {
        return m_pending.summary;
    }
    QString pendingDetail() const
    {
        return m_pending.detail;
    }
    QString pendingTier() const;
    QString pendingRisk() const
    {
        return aiRiskName(m_pending.risk);
    }
    bool pendingElevated() const
    {
        return m_pending.risk == AiRisk::Admin;
    }

    bool shareScreen() const
    {
        return m_shareScreen;
    }
    void setShareScreen(bool share);
    bool screenAvailable() const;

    // ---- what the island calls ------------------------------------------
    /** Long press: open the assistant, or the setup prompt if it has no key. */
    Q_INVOKABLE void engage();
    /** Put it away. A job already running keeps running, in the background. */
    Q_INVOKABLE void dismiss();
    Q_INVOKABLE void ask(const QString &text);
    /** Stop the current turn and everything it started. */
    Q_INVOKABLE void cancel();
    /** Hide the assistant but keep working; progress stays on the island. */
    Q_INVOKABLE void continueInBackground();
    Q_INVOKABLE void bringToFront();
    /** Answer the permission the island is showing. */
    Q_INVOKABLE void allow(bool rememberForSession);
    Q_INVOKABLE void deny();
    /** Forget the conversation and every allowance granted in it. */
    Q_INVOKABLE void startOver();

    // ---- what the settings window calls ----------------------------------
    Q_INVOKABLE bool hasKeyFor(const QString &provider) const;
    Q_INVOKABLE QString keyBackendFor(const QString &provider) const;
    Q_INVOKABLE void setKeyFor(const QString &provider, const QString &key);
    /** Ask the provider a trivial question, to prove the key works. */
    Q_INVOKABLE void testKey();
    Q_PROPERTY(QString keyTestResult READ keyTestResult NOTIFY keyTestChanged)
    QString keyTestResult() const
    {
        return m_keyTest;
    }

Q_SIGNALS:
    void configurationChanged();
    void stateChanged();
    void conversationChanged();
    void answerChanged();
    void activityChanged();
    void stepsChanged();
    void pendingChanged();
    void shareScreenChanged();
    void keyTestChanged();
    /** The assistant wants a line shown on the island. */
    void messageRequested(const QString &summary, const QString &body);
    /** The island should give the input field the keyboard. */
    void focusRequested();
    /** No provider is set up; the island offers to open the settings. */
    void setupRequested();

private:
    AiProvider *activeProvider() const;
    void setState(const QString &state);
    void setActivity(const QString &line);
    void addStep(const QString &kind, const QString &text, const QString &status);
    void updateLastStep(const QString &status, const QString &text = {});

    void runTurn();
    void onTurnEnded(const QString &stopReason, const QList<AiToolCall> &calls, const QJsonArray &raw);
    /** Take the next call off the queue: classify it, ask, or run it. */
    void advanceQueue();
    void executeNow(const AiToolCall &call, const AiVerdict &verdict);
    void onToolResult(const AiToolResult &result);
    void finishToolRound();
    void fail(const QString &reason);

    Config *m_config = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    AnthropicProvider *m_anthropic = nullptr;
    GeminiProvider *m_gemini = nullptr;
    CredentialStore *m_credentials = nullptr;
    PermissionBroker *m_broker = nullptr;
    AiToolbox *m_toolbox = nullptr;

    QList<AiTurn> m_history;
    QString m_state = QStringLiteral("composing");
    bool m_engaged = false;
    bool m_background = false;
    QString m_question;
    QString m_answer;
    QString m_thought;
    QString m_activity;
    QString m_error;
    QString m_keyTest;
    QVariantList m_steps;
    bool m_shareScreen = false;
    /** Guards against a model that keeps calling tools and never answers. */
    int m_rounds = 0;

    QQueue<AiToolCall> m_queue;
    QList<AiToolResult> m_results;
    AiToolCall m_current;
    AiVerdict m_pending;
    bool m_awaitingPermission = false;
    /** True while the turn is only a key test, so the UI stays out of it. */
    bool m_testing = false;
};
