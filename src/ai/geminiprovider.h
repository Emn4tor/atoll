/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aiprovider.h"

/** Gemini, over Google's generative language API. */
class GeminiProvider : public AiProvider
{
    Q_OBJECT

public:
    explicit GeminiProvider(QNetworkAccessManager *network, QObject *parent = nullptr);

    QString id() const override
    {
        return QStringLiteral("gemini");
    }
    QString defaultModel() const override
    {
        return QStringLiteral("gemini-2.5-pro");
    }

protected:
    QNetworkRequest buildRequest(const AiRequest &request) const override;
    QByteArray buildBody(const AiRequest &request) const override;
    void handleEvent(const QJsonObject &event) override;
    void resetTurn() override;

private:
    /** Rewrite one of Atoll's JSON Schema objects as Google expects it. */
    static QJsonValue toGoogleSchema(const QJsonValue &value);

    QJsonArray m_raw;
    QList<AiToolCall> m_calls;
    QString m_finishReason;
    int m_callCounter = 0;
    /** Where the model's own text is being collected, for the final turn. */
    QString m_text;
};
