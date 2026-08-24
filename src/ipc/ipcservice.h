/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QDBusContext>
#include <QDBusMessage>
#include <QHash>
#include <QObject>
#include <QStringList>
#include <QtQml/qqmlregistration.h>

/**
 * The scriptable side of Atoll: `org.atoll.Atoll` on the session bus.
 *
 * Everything the island can be told to do from the outside lives here, which
 * makes the island usable as a general purpose heads-up display for scripts,
 * hotkeys and other desktop tooling - not just for the events it watches.
 */
class IpcService : public QObject, protected QDBusContext
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.atoll.Atoll")
    QML_ELEMENT
    QML_UNCREATABLE("Accessed through App.ipc")

public:
    explicit IpcService(QObject *parent = nullptr);

    /** Claim the bus name. Returns false if another instance already has it. */
    bool registerOnBus();

    /**
     * Claim the settings window's own bus name. The settings page runs as a
     * second process - it wants a normal window, and layer-shell is a
     * process-wide decision - so it needs a name of its own to be raised
     * through instead of being started twice.
     */
    bool registerSettingsOnBus();

public Q_SLOTS:
    Q_SCRIPTABLE void expand();
    Q_SCRIPTABLE void collapse();
    Q_SCRIPTABLE void toggle();
    Q_SCRIPTABLE void showText(const QString &icon, const QString &text);
    Q_SCRIPTABLE void showProgress(const QString &icon, int percent, const QString &text);
    /** Offer these files to nearby devices, as if they had been dropped. */
    Q_SCRIPTABLE void share(const QStringList &paths);
    /** Open the assistant with an empty box, as a long press does. */
    Q_SCRIPTABLE void assistant();
    /** Put a question to the assistant, as if it had been typed on the island. */
    Q_SCRIPTABLE void ask(const QString &prompt);
    Q_SCRIPTABLE void reloadConfig();
    Q_SCRIPTABLE void settings();
    Q_SCRIPTABLE void raise();
    Q_SCRIPTABLE QString version() const;
    Q_SCRIPTABLE void quit();

    /**
     * Ask the island whether a tool call may go ahead.
     *
     * `payload` is the request as the command-line client describes it, and
     * the reply is `{"decision": "allow"|"deny", "reason": "..."}`. The caller
     * is left waiting, sometimes for as long as it takes a person to notice
     * the question, so the reply is sent later rather than returned here.
     */
    Q_SCRIPTABLE QString reviewToolCall(const QString &payload);

public:
    /** Answer a review that reviewToolCall left open. */
    void answerToolReview(const QString &token, const QString &verdictJson);
    /** Whether anything is still waiting for an answer. */
    bool hasOpenReviews() const
    {
        return !m_reviews.isEmpty();
    }

Q_SIGNALS:
    void expandRequested();
    void collapseRequested();
    void toggleRequested();
    void textRequested(const QString &icon, const QString &text);
    void progressRequested(const QString &icon, int percent, const QString &text);
    void shareRequested(const QStringList &paths);
    void assistantRequested();
    void askRequested(const QString &prompt);
    void reloadRequested();
    void settingsRequested();
    void raiseRequested();
    /** A tool call is waiting for a verdict; answer it with `token`. */
    void toolReviewRequested(const QString &payload, const QString &token);

private:
    /** One caller left hanging on reviewToolCall, and how to reach it. */
    struct OpenReview {
        QDBusConnection connection;
        QDBusMessage request;
    };

    QHash<QString, OpenReview> m_reviews;
    int m_reviewCounter = 0;
};
