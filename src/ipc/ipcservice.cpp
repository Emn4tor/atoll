/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "ipcservice.h"

#include <QCoreApplication>
#include <QDBusConnection>

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

void IpcService::reloadConfig()
{
    Q_EMIT reloadRequested();
}

QString IpcService::version() const
{
    return QStringLiteral(ATOLL_VERSION);
}

void IpcService::quit()
{
    QCoreApplication::quit();
}
