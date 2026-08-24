/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "permissionhook.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

using namespace Qt::StringLiterals;

namespace
{
/**
 * Long enough for somebody to come back to their desk, and short enough that a
 * forgotten question does not pin a process there for the rest of the session.
 * It has to stay under the timeout the client itself was given for the hook,
 * or the client gives up first and the answer arrives with nobody to hear it.
 */
constexpr int AnswerTimeoutMs = 9 * 60 * 1000;

/** The shape the client reads back: a decision and a sentence about it. */
void reply(const QString &decision, const QString &reason)
{
    const QJsonObject specific{{u"hookEventName"_s, u"PreToolUse"_s},
                               {u"permissionDecision"_s, decision},
                               {u"permissionDecisionReason"_s, reason}};
    const QJsonObject output{{u"hookSpecificOutput"_s, specific}};

    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(output).toJson(QJsonDocument::Compact)) << Qt::endl;
}
}

int runPermissionHook()
{
    QFile input;
    if (!input.open(stdin, QIODevice::ReadOnly)) {
        reply(u"deny"_s, u"Atoll could not read the request."_s);
        return 0;
    }
    const QByteArray payload = input.readAll();

    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        reply(u"deny"_s, u"Atoll is not reachable, so nobody can approve this."_s);
        return 0;
    }

    auto message = QDBusMessage::createMethodCall(u"org.atoll.Atoll"_s,
                                                  u"/Atoll"_s,
                                                  u"org.atoll.Atoll"_s,
                                                  u"reviewToolCall"_s);
    message.setArguments({QString::fromUtf8(payload)});

    const QDBusMessage answer = bus.call(message, QDBus::Block, AnswerTimeoutMs);
    if (answer.type() == QDBusMessage::ErrorMessage || answer.arguments().isEmpty()) {
        reply(u"deny"_s,
              u"Atoll could not ask the user about this (%1)."_s.arg(answer.errorMessage()));
        return 0;
    }

    const QJsonObject verdict =
        QJsonDocument::fromJson(answer.arguments().constFirst().toString().toUtf8()).object();
    const QString decision = verdict.value(u"decision"_s).toString();
    const QString reason = verdict.value(u"reason"_s).toString();

    reply(decision == u"allow"_s ? u"allow"_s : u"deny"_s,
          reason.isEmpty() ? u"Decided on the island."_s : reason);
    return 0;
}
