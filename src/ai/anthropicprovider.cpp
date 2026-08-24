/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "anthropicprovider.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QNetworkRequest>

using namespace Qt::StringLiterals;

namespace
{
/**
 * The search tool that runs on the provider's own servers. Asking for it costs
 * nothing until the model uses it, and it is the difference between "look this
 * up for me" working and the assistant apologising.
 */
QJsonObject serverSearchTool()
{
    return QJsonObject{{u"type"_s, u"web_search_20260209"_s}, {u"name"_s, u"web_search"_s}};
}

QJsonObject textBlock(const QString &text)
{
    return QJsonObject{{u"type"_s, u"text"_s}, {u"text"_s, text}};
}

QJsonObject imageBlock(const QByteArray &data, const QString &mediaType)
{
    return QJsonObject{{u"type"_s, u"image"_s},
                       {u"source"_s,
                        QJsonObject{{u"type"_s, u"base64"_s},
                                    {u"media_type"_s, mediaType},
                                    {u"data"_s, QString::fromLatin1(data.toBase64())}}}};
}
}

AnthropicProvider::AnthropicProvider(QNetworkAccessManager *network, QObject *parent)
    : AiProvider(network, parent)
{
}

QNetworkRequest AnthropicProvider::buildRequest(const AiRequest &request) const
{
    const QString base = request.baseUrl.isEmpty() ? u"https://api.anthropic.com"_s
                                                   : QString(request.baseUrl).remove(QRegularExpression(u"/+$"_s));
    QNetworkRequest network(QUrl(base + u"/v1/messages"_s));
    network.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    network.setRawHeader("x-api-key", m_apiKey.toUtf8());
    network.setRawHeader("anthropic-version", "2023-06-01");
    network.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return network;
}

QByteArray AnthropicProvider::buildBody(const AiRequest &request) const
{
    QJsonArray messages;

    for (const AiTurn &turn : request.history) {
        if (turn.role == u"assistant"_s) {
            // Replay what the provider actually sent whenever it is available:
            // reasoning blocks have to come back untouched, and rebuilding them
            // from the text alone is not the same message.
            if (!turn.rawContent.isEmpty() && turn.rawProvider == id()) {
                messages.append(QJsonObject{{u"role"_s, u"assistant"_s}, {u"content"_s, turn.rawContent}});
                continue;
            }
            QJsonArray content;
            if (!turn.text.isEmpty()) {
                content.append(textBlock(turn.text));
            }
            for (const AiToolCall &call : turn.toolCalls) {
                content.append(QJsonObject{{u"type"_s, u"tool_use"_s},
                                           {u"id"_s, call.id},
                                           {u"name"_s, call.name},
                                           {u"input"_s, QJsonObject::fromVariantMap(call.input)}});
            }
            if (content.isEmpty()) {
                continue;
            }
            messages.append(QJsonObject{{u"role"_s, u"assistant"_s}, {u"content"_s, content}});
            continue;
        }

        QJsonArray content;
        // Every result from one round has to travel in a single user message,
        // or the model quietly stops making parallel calls.
        for (const AiToolResult &result : turn.toolResults) {
            QJsonArray inner;
            inner.append(textBlock(result.content.isEmpty() ? u"(no output)"_s : result.content));
            if (!result.image.isEmpty()) {
                inner.append(imageBlock(result.image, result.imageMediaType));
            }
            QJsonObject block{{u"type"_s, u"tool_result"_s},
                              {u"tool_use_id"_s, result.id},
                              {u"content"_s, inner}};
            if (result.isError) {
                block.insert(u"is_error"_s, true);
            }
            content.append(block);
        }
        if (!turn.image.isEmpty()) {
            content.append(imageBlock(turn.image, turn.imageMediaType));
        }
        if (!turn.text.isEmpty()) {
            content.append(textBlock(turn.text));
        }
        if (content.isEmpty()) {
            continue;
        }
        messages.append(QJsonObject{{u"role"_s, u"user"_s}, {u"content"_s, content}});
    }

    QJsonArray tools = request.tools;
    if (request.webSearch) {
        tools.append(serverSearchTool());
    }

    QJsonObject body{
        {u"model"_s, request.model.isEmpty() ? defaultModel() : request.model},
        {u"max_tokens"_s, request.maxTokens},
        {u"stream"_s, true},
        {u"messages"_s, messages},
    };

    if (!request.systemPrompt.isEmpty()) {
        // The system prompt and the tool list are the same on every turn of a
        // conversation, so they are worth caching; the questions that follow
        // are not.
        body.insert(u"system"_s,
                    QJsonArray{QJsonObject{{u"type"_s, u"text"_s},
                                           {u"text"_s, request.systemPrompt},
                                           {u"cache_control"_s,
                                            QJsonObject{{u"type"_s, u"ephemeral"_s}}}}});
    }
    if (!tools.isEmpty()) {
        body.insert(u"tools"_s, tools);
    }
    // Adaptive is the only thinking mode current models take, and asking for a
    // summary is what lets the island say what the assistant is working on
    // instead of showing a spinner.
    body.insert(u"thinking"_s,
                QJsonObject{{u"type"_s, u"adaptive"_s}, {u"display"_s, u"summarized"_s}});
    if (!request.effort.isEmpty()) {
        body.insert(u"output_config"_s, QJsonObject{{u"effort"_s, request.effort}});
    }

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

void AnthropicProvider::resetTurn()
{
    m_blocks.clear();
    m_raw = {};
    m_calls.clear();
    m_stopReason.clear();
}

QJsonObject AnthropicProvider::sealBlock(const Block &block) const
{
    QJsonObject object = block.start;
    if (block.type == u"tool_use"_s) {
        const auto parsed = QJsonDocument::fromJson(block.partialJson.isEmpty()
                                                        ? QByteArray("{}")
                                                        : block.partialJson.toUtf8());
        object.insert(u"input"_s, parsed.isObject() ? parsed.object() : QJsonObject());
        return object;
    }
    if (block.type == u"text"_s) {
        object.insert(u"text"_s, block.text);
        return object;
    }
    if (block.type == u"thinking"_s) {
        object.insert(u"thinking"_s, block.text);
        return object;
    }
    return object;
}

void AnthropicProvider::handleEvent(const QJsonObject &event)
{
    const QString type = event.value(u"type"_s).toString();

    if (type == u"error"_s) {
        const QJsonObject error = event.value(u"error"_s).toObject();
        Q_EMIT failed(error.value(u"message"_s).toString(tr("The assistant service reported an error.")));
        return;
    }

    if (type == u"content_block_start"_s) {
        const int index = event.value(u"index"_s).toInt();
        const QJsonObject start = event.value(u"content_block"_s).toObject();
        Block block;
        block.start = start;
        block.type = start.value(u"type"_s).toString();
        block.id = start.value(u"id"_s).toString();
        block.name = start.value(u"name"_s).toString();
        block.text = start.value(u"text"_s).toString();
        m_blocks.insert(index, block);
        return;
    }

    if (type == u"content_block_delta"_s) {
        const int index = event.value(u"index"_s).toInt();
        auto it = m_blocks.find(index);
        if (it == m_blocks.end()) {
            return;
        }
        const QJsonObject delta = event.value(u"delta"_s).toObject();
        const QString deltaType = delta.value(u"type"_s).toString();
        if (deltaType == u"text_delta"_s) {
            const QString piece = delta.value(u"text"_s).toString();
            it->text.append(piece);
            Q_EMIT textDelta(piece);
        } else if (deltaType == u"thinking_delta"_s) {
            const QString piece = delta.value(u"thinking"_s).toString();
            it->text.append(piece);
            Q_EMIT thoughtDelta(piece);
        } else if (deltaType == u"input_json_delta"_s) {
            it->partialJson.append(delta.value(u"partial_json"_s).toString());
        } else if (deltaType == u"signature_delta"_s) {
            // Belongs to the block's signature, which is carried in `start`
            // and replayed with it; nothing to accumulate here.
            it->start.insert(u"signature"_s, delta.value(u"signature"_s).toString());
        }
        return;
    }

    if (type == u"content_block_stop"_s) {
        const int index = event.value(u"index"_s).toInt();
        auto it = m_blocks.find(index);
        if (it == m_blocks.end()) {
            return;
        }
        const QJsonObject sealed = sealBlock(*it);
        m_raw.append(sealed);
        if (it->type == u"tool_use"_s) {
            AiToolCall call;
            call.id = it->id;
            call.name = it->name;
            call.input = sealed.value(u"input"_s).toObject().toVariantMap();
            m_calls.append(call);
        }
        m_blocks.erase(it);
        return;
    }

    if (type == u"message_delta"_s) {
        const QString stop = event.value(u"delta"_s).toObject().value(u"stop_reason"_s).toString();
        if (!stop.isEmpty()) {
            m_stopReason = stop;
        }
        return;
    }

    if (type == u"message_stop"_s) {
        Q_EMIT turnEnded(m_stopReason.isEmpty() ? u"end_turn"_s : m_stopReason, m_calls, m_raw);
        resetTurn();
    }
}
