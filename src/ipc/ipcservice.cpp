/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ipcservice.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QUuid>

using namespace Qt::StringLiterals;

IpcService::IpcService(QObject *parent)
    : QObject(parent)
{
}

bool IpcService::registerOnBus()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(u"org.atoll.Atoll"_s)) {
        return false;
    }
    return bus.registerObject(u"/Atoll"_s,
                              this,
                              QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
}

bool IpcService::registerSettingsOnBus()
{
    auto bus = QDBusConnection::sessionBus();
    if (!bus.registerService(u"org.atoll.AtollSettings"_s)) {
        return false;
    }
    return bus.registerObject(u"/Settings"_s,
                              this,
                              QDBusConnection::ExportScriptableSlots | QDBusConnection::ExportScriptableSignals);
}

void IpcService::expand()
{
    Q_EMIT expandRequested();
}

void IpcService::collapse()
{
    Q_EMIT collapseRequested();
}

void IpcService::toggle()
{
    Q_EMIT toggleRequested();
}

void IpcService::showText(const QString &icon, const QString &text)
{
    Q_EMIT textRequested(icon, text);
}

void IpcService::showProgress(const QString &icon, int percent, const QString &text)
{
    Q_EMIT progressRequested(icon, percent, text);
}

void IpcService::share(const QStringList &paths)
{
    Q_EMIT shareRequested(paths);
}

void IpcService::assistant()
{
    Q_EMIT assistantRequested();
}

void IpcService::ask(const QString &prompt)
{
    Q_EMIT askRequested(prompt);
}

QString IpcService::reviewToolCall(const QString &payload)
{
    if (!calledFromDBus()) {
        return {};
    }
    // The answer depends on a person, so the method call is parked and the
    // reply sent from allow() or deny() whenever that person gets to it.
    setDelayedReply(true);

    const QString token = u"review-%1"_s.arg(++m_reviewCounter);
    m_reviews.insert(token, OpenReview{connection(), message()});
    Q_EMIT toolReviewRequested(payload, token);
    return {};
}

void IpcService::answerToolReview(const QString &token, const QString &verdictJson)
{
    const auto review = m_reviews.take(token);
    if (review.request.type() == QDBusMessage::InvalidMessage) {
        return;
    }
    QDBusConnection connection = review.connection;
    connection.send(review.request.createReply(verdictJson));
}

void IpcService::reloadConfig()
{
    Q_EMIT reloadRequested();
}

void IpcService::settings()
{
    Q_EMIT settingsRequested();
}

void IpcService::raise()
{
    Q_EMIT raiseRequested();
}

QString IpcService::version() const
{
    return QStringLiteral(ATOLL_VERSION);
}

void IpcService::quit()
{
    QCoreApplication::quit();
}
