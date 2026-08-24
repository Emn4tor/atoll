/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "geminiprovider.h"

#include <QJsonDocument>
#include <QRegularExpression>
#include <QNetworkRequest>
#include <QUrlQuery>

using namespace Qt::StringLiterals;

namespace
{
QJsonObject textPart(const QString &text)
{
    return QJsonObject{{u"text"_s, text}};
}

QJsonObject imagePart(const QByteArray &data, const QString &mediaType)
{
    return QJsonObject{{u"inlineData"_s,
                        QJsonObject{{u"mimeType"_s, mediaType},
                                    {u"data"_s, QString::fromLatin1(data.toBase64())}}}};
}
}

GeminiProvider::GeminiProvider(QNetworkAccessManager *network, QObject *parent)
    : AiProvider(network, parent)
{
}

QNetworkRequest GeminiProvider::buildRequest(const AiRequest &request) const
{
    const QString model = request.model.isEmpty() ? defaultModel() : request.model;
    const QString base = request.baseUrl.isEmpty()
        ? u"https://generativelanguage.googleapis.com"_s
        : QString(request.baseUrl).remove(QRegularExpression(u"/+$"_s));
    QUrl url(u"%1/v1beta/models/%2:streamGenerateContent"_s.arg(base, model));
    // Without alt=sse the endpoint streams a JSON array instead of events, and
    // there is then nothing to parse until the whole answer has arrived.
    QUrlQuery query;
    query.addQueryItem(u"alt"_s, u"sse"_s);
    url.setQuery(query);

    QNetworkRequest network(url);
    network.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_s);
    network.setRawHeader("x-goog-api-key", m_apiKey.toUtf8());
    network.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return network;
}

QJsonValue GeminiProvider::toGoogleSchema(const QJsonValue &value)
{
    if (value.isArray()) {
        QJsonArray out;
        for (const QJsonValue &entry : value.toArray()) {
            out.append(toGoogleSchema(entry));
        }
        return out;
    }
    if (!value.isObject()) {
        return value;
    }

    QJsonObject in = value.toObject();
    QJsonObject out;
    for (auto it = in.constBegin(); it != in.constEnd(); ++it) {
        if (it.key() == u"type"_s && it.value().isString()) {
            // Google's schema dialect spells the primitive types in capitals.
            out.insert(u"type"_s, it.value().toString().toUpper());
            continue;
        }
        // Keys that mean nothing here are dropped rather than sent and refused.
        if (it.key() == u"additionalProperties"_s || it.key() == u"$schema"_s) {
            continue;
        }
        out.insert(it.key(), toGoogleSchema(it.value()));
    }
    return out;
}

QByteArray GeminiProvider::buildBody(const AiRequest &request) const
{
    QJsonArray contents;

    for (const AiTurn &turn : request.history) {
        if (turn.role == u"assistant"_s) {
            if (!turn.rawContent.isEmpty() && turn.rawProvider == id()) {
                contents.append(QJsonObject{{u"role"_s, u"model"_s}, {u"parts"_s, turn.rawContent}});
                continue;
            }
            QJsonArray parts;
            if (!turn.text.isEmpty()) {
                parts.append(textPart(turn.text));
            }
            for (const AiToolCall &call : turn.toolCalls) {
                parts.append(QJsonObject{{u"functionCall"_s,
                                          QJsonObject{{u"name"_s, call.name},
                                                      {u"args"_s, QJsonObject::fromVariantMap(call.input)}}}});
            }
            if (parts.isEmpty()) {
                continue;
            }
            contents.append(QJsonObject{{u"role"_s, u"model"_s}, {u"parts"_s, parts}});
            continue;
        }

        QJsonArray parts;
        for (const AiToolResult &result : turn.toolResults) {
            // Google matches results to calls by tool name, not by an id, so
            // the name has to survive the round trip; the service keeps it in
            // the result id after the colon.
            const QString name = result.id.section(u':', 1);
            QJsonObject response{{u"output"_s, result.content}};
            if (result.isError) {
                response.insert(u"error"_s, true);
            }
            parts.append(QJsonObject{
                {u"functionResponse"_s,
                 QJsonObject{{u"name"_s, name.isEmpty() ? result.id : name}, {u"response"_s, response}}}});
            // A picture cannot ride inside a function response, so it follows
            // as an ordinary part of the same message.
            if (!result.image.isEmpty()) {
                parts.append(imagePart(result.image, result.imageMediaType));
            }
        }
        if (!turn.image.isEmpty()) {
            parts.append(imagePart(turn.image, turn.imageMediaType));
        }
        if (!turn.text.isEmpty()) {
            parts.append(textPart(turn.text));
        }
        if (parts.isEmpty()) {
            continue;
        }
        contents.append(QJsonObject{{u"role"_s, u"user"_s}, {u"parts"_s, parts}});
    }

    QJsonArray declarations;
    for (const QJsonValue &entry : request.tools) {
        const QJsonObject definition = entry.toObject();
        QJsonObject declaration{{u"name"_s, definition.value(u"name"_s)},
                                {u"description"_s, definition.value(u"description"_s)}};
        const QJsonObject schema = definition.value(u"input_schema"_s).toObject();
        // A tool that takes nothing must not carry an empty parameter object;
        // it is rejected as a malformed schema.
        if (!schema.value(u"properties"_s).toObject().isEmpty()) {
            declaration.insert(u"parameters"_s, toGoogleSchema(schema));
        }
        declarations.append(declaration);
    }

    QJsonArray tools;
    if (!declarations.isEmpty()) {
        tools.append(QJsonObject{{u"functionDeclarations"_s, declarations}});
    }
    if (request.webSearch) {
        tools.append(QJsonObject{{u"googleSearch"_s, QJsonObject{}}});
    }

    QJsonObject body{{u"contents"_s, contents},
                     {u"generationConfig"_s,
                      QJsonObject{{u"maxOutputTokens"_s, request.maxTokens}}}};
    if (!request.systemPrompt.isEmpty()) {
        body.insert(u"systemInstruction"_s,
                    QJsonObject{{u"parts"_s, QJsonArray{textPart(request.systemPrompt)}}});
    }
    if (!tools.isEmpty()) {
        body.insert(u"tools"_s, tools);
    }

    return QJsonDocument(body).toJson(QJsonDocument::Compact);
}

void GeminiProvider::resetTurn()
{
    m_raw = {};
    m_calls.clear();
    m_finishReason.clear();
    m_callCounter = 0;
    m_text.clear();
}

void GeminiProvider::handleEvent(const QJsonObject &event)
{
    if (event.contains(u"error"_s)) {
        Q_EMIT failed(event.value(u"error"_s)
                          .toObject()
                          .value(u"message"_s)
                          .toString(tr("The assistant service reported an error.")));
        return;
    }

    const QJsonArray candidates = event.value(u"candidates"_s).toArray();
    if (candidates.isEmpty()) {
        return;
    }
    const QJsonObject candidate = candidates.first().toObject();

    for (const QJsonValue &entry : candidate.value(u"content"_s).toObject().value(u"parts"_s).toArray()) {
        const QJsonObject part = entry.toObject();

        if (part.contains(u"functionCall"_s)) {
            const QJsonObject function = part.value(u"functionCall"_s).toObject();
            AiToolCall call;
            // The id is Atoll's own bookkeeping; the name after the colon is
            // what has to go back to Google when the result is returned.
            call.name = function.value(u"name"_s).toString();
            call.id = u"gem%1:%2"_s.arg(++m_callCounter).arg(call.name);
            call.input = function.value(u"args"_s).toObject().toVariantMap();
            m_calls.append(call);
            m_raw.append(part);
            continue;
        }

        const QString text = part.value(u"text"_s).toString();
        if (text.isEmpty()) {
            continue;
        }
        // Gemini marks its reasoning with a flag on the part rather than with
        // a block type of its own.
        if (part.value(u"thought"_s).toBool()) {
            Q_EMIT thoughtDelta(text);
            continue;
        }
        m_text.append(text);
        m_raw.append(part);
        Q_EMIT textDelta(text);
    }

    const QString finish = candidate.value(u"finishReason"_s).toString();
    if (finish.isEmpty()) {
        return;
    }
    m_finishReason = finish;

    QString stopReason = u"end_turn"_s;
    if (!m_calls.isEmpty()) {
        stopReason = u"tool_use"_s;
    } else if (finish == u"MAX_TOKENS"_s) {
        stopReason = u"max_tokens"_s;
    } else if (finish == u"SAFETY"_s || finish == u"BLOCKLIST"_s || finish == u"PROHIBITED_CONTENT"_s) {
        stopReason = u"refusal"_s;
    }

    Q_EMIT turnEnded(stopReason, m_calls, m_raw);
    resetTurn();
}
