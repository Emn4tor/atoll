/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QAtomicInt>
#include <QMetaType>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QVariantList>

/** One message seen on the session bus, flattened into Qt types. */
struct DBusMessageInfo {
    enum Kind { MethodCall, MethodReturn, Signal, Error, Unknown };

    Kind kind = Unknown;
    QString sender;
    QString destination;
    QString path;
    QString interface;
    QString member;
    quint32 serial = 0;
    quint32 replySerial = 0;
    QVariantList arguments;
};
Q_DECLARE_METATYPE(DBusMessageInfo)

/**
 * A read-only tap on the session bus.
 *
 * Atoll never owns `org.freedesktop.Notifications` - the user's real
 * notification daemon keeps doing its job - so the only way to learn about
 * notifications and Plasma's OSD requests is to ask the bus daemon to make us
 * a monitor. A monitor connection cannot send anything, which is why this runs
 * on its own private connection in its own thread and reports back by signal.
 */
class DBusMonitor : public QObject
{
    Q_OBJECT

public:
    explicit DBusMonitor(QObject *parent = nullptr);
    ~DBusMonitor() override;

    /**
     * @param matchRules bus match rules, without `eavesdrop=` (BecomeMonitor
     *        rejects it). An empty list means "everything", which we avoid.
     */
    bool start(const QStringList &matchRules);
    void stop();
    bool isRunning() const;

    /** Human-readable reason the tap is unavailable, if it is. */
    QString lastError() const
    {
        return m_lastError;
    }

Q_SIGNALS:
    void messageReceived(const DBusMessageInfo &message);
    void failed(const QString &reason);

private:
    class Worker : public QThread
    {
    public:
        Worker(DBusMonitor *owner, QStringList rules);
        void run() override;
        void requestStop()
        {
            m_stop.storeRelease(1);
        }

    private:
        DBusMonitor *m_owner;
        QStringList m_rules;
        QAtomicInt m_stop{0};
    };

    Worker *m_worker = nullptr;
    QString m_lastError;
};
