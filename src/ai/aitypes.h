/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariantMap>

/**
 * How much trust an action needs before Atoll will carry it out.
 *
 * The assistant never names its own tier. It asks for a tool, and the broker
 * decides what that request actually costs - which is the whole point: a model
 * that only needs to read a file cannot talk its way into a root shell, no
 * matter how it words the request.
 */
enum class AiRisk {
    /** Reads something that is already visible to the user. Runs unattended. */
    Safe,
    /** Changes the session: files in $HOME, settings, launching things. */
    User,
    /** Leaves the user's account: package manager, systemd, anything as root. */
    Admin,
    /** Refused outright, whatever the user says. */
    Forbidden,
};

/** One call the model wants to make. */
struct AiToolCall {
    QString id;
    QString name;
    QVariantMap input;
};

/** What came back from it, ready to be handed to the model. */
struct AiToolResult {
    QString id;
    QString content;
    bool isError = false;
    /** Set when the tool produced a picture rather than only words. */
    QByteArray image;
    QString imageMediaType;
};

/** A tool call after the broker has looked at it. */
struct AiVerdict {
    AiRisk risk = AiRisk::User;
    /** One line, shown on the island: "Install firefox". */
    QString summary;
    /** The literal thing that will happen, for the user to read before saying yes. */
    QString detail;
    /** Why it was refused, when risk is Forbidden. */
    QString refusal;
    /** What a granted permission is remembered under, when the user allows it. */
    QString grantKey;
};

/** Enum classes have no ordering of their own; risk very much does. */
inline AiRisk maxRisk(AiRisk a, AiRisk b)
{
    return static_cast<int>(a) >= static_cast<int>(b) ? a : b;
}

inline QString aiRiskName(AiRisk risk)
{
    switch (risk) {
    case AiRisk::Safe:
        return QStringLiteral("safe");
    case AiRisk::User:
        return QStringLiteral("user");
    case AiRisk::Admin:
        return QStringLiteral("admin");
    case AiRisk::Forbidden:
        return QStringLiteral("forbidden");
    }
    return QStringLiteral("user");
}
