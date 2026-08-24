/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include "ai/aitypes.h"

#include <QObject>
#include <QSet>
#include <QString>

class Config;

/**
 * Decides what a tool call is allowed to cost.
 *
 * The assistant asks for a *tool*, never for a privilege. This class looks at
 * what the call would actually do and returns the tier it belongs in, so a
 * model cannot obtain root by claiming it needs root - if the work fits inside
 * the user's own account, that is where it runs.
 *
 * The classification is deliberately pessimistic: anything the rules do not
 * recognise is treated as a change to the session rather than as harmless, and
 * a small set of operations - wiping a disk, rewriting the password database,
 * piping a download straight into a shell - is refused no matter who asks.
 */
class PermissionBroker : public QObject
{
    Q_OBJECT

public:
    explicit PermissionBroker(Config *config, QObject *parent = nullptr);

    /** What this call would cost, and how to describe it to the user. */
    AiVerdict classify(const AiToolCall &call) const;

    /**
     * Whether the verdict can proceed without stopping to ask. True for read
     * only work, for anything the user has already allowed this session, and -
     * if they configured it that way - for changes inside their own account.
     */
    bool isPreApproved(const AiVerdict &verdict) const;

    /** Remember an allowance for the rest of this session. */
    void grantForSession(const QString &grantKey);
    void revokeAll();

    /** Whether the tier is reachable at all with the current settings. */
    bool tierEnabled(AiRisk risk) const;

    /** Human wording for the island. */
    static QString tierTitle(AiRisk risk);

private:
    AiVerdict classifyCommand(const QString &command) const;
    AiVerdict classifyPath(const QString &path, bool forWriting) const;

    Config *m_config = nullptr;
    QSet<QString> m_sessionGrants;
};
