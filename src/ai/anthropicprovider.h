/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aiprovider.h"

#include <QHash>

/** Claude, over the Messages API. */
class AnthropicProvider : public AiProvider
{
    Q_OBJECT

public:
    explicit AnthropicProvider(QNetworkAccessManager *network, QObject *parent = nullptr);

    QString id() const override
    {
        return QStringLiteral("anthropic");
    }
    QString defaultModel() const override
    {
        return QStringLiteral("claude-opus-5");
    }

protected:
    QNetworkRequest buildRequest(const AiRequest &request) const override;
    QByteArray buildBody(const AiRequest &request) const override;
    void handleEvent(const QJsonObject &event) override;
    void resetTurn() override;

private:
    /** One content block as it is being streamed in. */
    struct Block {
        QString type;
        QString id;
        QString name;
        QString text;
        /** tool_use inputs arrive as JSON in pieces. */
        QString partialJson;
        /** Everything the block started with, for replaying it verbatim. */
        QJsonObject start;
    };

    QJsonObject sealBlock(const Block &block) const;

    QHash<int, Block> m_blocks;
    QJsonArray m_raw;
    QList<AiToolCall> m_calls;
    QString m_stopReason;
};
