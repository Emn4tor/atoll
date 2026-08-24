/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aitypes.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * One entry in the conversation, in Atoll's own shape.
 *
 * Providers disagree about almost everything except the shape of a
 * conversation, so the service keeps the history here and each provider
 * rewrites it on the way out. That is also what lets the user change provider
 * mid-session without the assistant losing the thread.
 */
struct AiTurn {
    /** "user" or "assistant". */
    QString role;
    QString text;
    /** Assistant turns: what it decided to call. */
    QList<AiToolCall> toolCalls;
    /** User turns: what those calls produced. */
    QList<AiToolResult> toolResults;
    /** User turns: a picture the user attached, or a screenshot a tool took. */
    QByteArray image;
    QString imageMediaType;

    /**
     * Assistant turns, verbatim, exactly as the provider streamed them.
     *
     * Replaying an assistant turn from reconstructed text loses whatever the
     * provider put in it that Atoll has no name for - reasoning blocks that
     * have to come back unchanged, results from the provider's own search
     * tool - and the next request is then rejected or silently degraded. So
     * the raw blocks are kept and handed straight back, and `provider` records
     * whose dialect they are in, because a conversation can change provider
     * mid-way and the old blocks then have to be dropped.
     */
    QJsonArray rawContent;
    QString rawProvider;
};

/** Everything a request needs, independent of who serves it. */
struct AiRequest {
    QString systemPrompt;
    QList<AiTurn> history;
    QJsonArray tools;
    bool webSearch = false;
    int maxTokens = 16000;
    QString model;
    QString effort;
    /**
     * Override for the service's address. Empty means the provider's own.
     * It exists for the people who reach these APIs through a company proxy or
     * a local gateway, and it is what makes the request path testable without
     * talking to anybody's servers.
     */
    QString baseUrl;
};

/**
 * One assistant, whatever it is made of.
 *
 * A turn goes in, text and tool activity come back out as signals. That is the
 * whole contract, and it is deliberately thin: it is met both by the services
 * that speak HTTP and by the command-line client, which runs the tool loop
 * itself in a process of its own.
 */
class AiBackend : public QObject
{
    Q_OBJECT

public:
    explicit AiBackend(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    /** Start a turn. Any turn already in flight is abandoned first. */
    virtual void send(const AiRequest &request) = 0;
    virtual void abort() = 0;
    virtual bool busy() const = 0;

    /** What this backend calls itself, for logs and the settings window. */
    virtual QString id() const = 0;
    /** The model used when the config names none. */
    virtual QString defaultModel() const = 0;

    /**
     * Whether the backend carries out tool calls by itself.
     *
     * The HTTP services hand a list of calls back and wait for Atoll to run
     * them; the command-line client runs them in its own process and only
     * reports what happened. The permission question is asked in the same
     * place either way - what changes is who does the work afterwards, and
     * that is what this answers.
     */
    virtual bool drivesTools() const
    {
        return false;
    }

Q_SIGNALS:
    /** More of the answer arrived. */
    void textDelta(const QString &text);
    /** A summary of the model's reasoning, when it offers one. */
    void thoughtDelta(const QString &text);
    /**
     * The turn is over. `stopReason` is normalised across backends:
     * "tool_use" (carry out `calls`), "pause_turn" (send the same history
     * straight back), "refusal", "max_tokens" or "end_turn".
     */
    void turnEnded(const QString &stopReason,
                   const QList<AiToolCall> &calls,
                   const QJsonArray &rawContent);
    void failed(const QString &reason);

    // ---- self-driving backends only ---------------------------------------
    /** A tool call started. `id` matches the one the finish signal carries. */
    void toolStarted(const QString &id, const QString &summary);
    void toolFinished(const QString &id, bool ok, const QString &detail);
};

/**
 * A streaming connection to one assistant service.
 *
 * The base class owns the socket and the server-sent-events framing, which is
 * the same everywhere; subclasses only build the request body and interpret
 * one decoded event.
 */
class AiProvider : public AiBackend
{
    Q_OBJECT

public:
    explicit AiProvider(QNetworkAccessManager *network, QObject *parent = nullptr);
    ~AiProvider() override;

    void setApiKey(const QString &key)
    {
        m_apiKey = key;
    }

    void send(const AiRequest &request) override;
    void abort() override;
    bool busy() const override;

protected:
    /** Build the HTTP request and its body. */
    virtual QNetworkRequest buildRequest(const AiRequest &request) const = 0;
    virtual QByteArray buildBody(const AiRequest &request) const = 0;
    /** Interpret one decoded `data:` payload. */
    virtual void handleEvent(const QJsonObject &event) = 0;
    /** Called before a new turn starts, to drop anything left from the last. */
    virtual void resetTurn() = 0;

    /** Pull a readable message out of a provider's error body. */
    static QString describeHttpError(int status, const QByteArray &body);

    QString m_apiKey;

private:
    void consume();

    QNetworkAccessManager *m_network = nullptr;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    bool m_aborting = false;
};
