/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QObject>
#include <QString>

/**
 * Where the assistant's API keys live.
 *
 * In order of preference: an environment variable, so a machine that is
 * already set up needs no configuration at all; then the desktop's own secret
 * service, which on Plasma is KWallet and means the key is encrypted at rest
 * and unlocked with the login; and only if neither is available, a file in the
 * user's data directory with no group or world access.
 *
 * The last one is a fallback rather than a design: it is reported as such in
 * the settings window so nobody is misled about where their key ended up.
 */
class CredentialStore : public QObject
{
    Q_OBJECT

public:
    explicit CredentialStore(QObject *parent = nullptr);

    /** `provider` is "anthropic" or "gemini". */
    QString key(const QString &provider) const;
    bool hasKey(const QString &provider) const;
    /** An empty value forgets the key. */
    void setKey(const QString &provider, const QString &value);

    /** "environment", "wallet", "file" or "none" - what the settings window shows. */
    QString backendFor(const QString &provider) const;

    /** Whether a secret service is reachable at all. */
    static bool walletAvailable();

private:
    static QString environmentVariable(const QString &provider);
    static QString filePath();
    static QString fromWallet(const QString &provider);
    static bool toWallet(const QString &provider, const QString &value);
    static QString fromFile(const QString &provider);
    static void toFile(const QString &provider, const QString &value);
};
