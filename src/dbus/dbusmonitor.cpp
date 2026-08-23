/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "dbusmonitor.h"
#include "dbusvariant.h"

#include <dbus/dbus.h>

#include <QDebug>

using namespace Qt::StringLiterals;

DBusMonitor::DBusMonitor(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DBusMessageInfo>("DBusMessageInfo");
}

DBusMonitor::~DBusMonitor()
{
    stop();
}

bool DBusMonitor::isRunning() const
{
    return m_worker && m_worker->isRunning();
}

bool DBusMonitor::start(const QStringList &matchRules)
{
    if (m_worker) {
        return true;
    }
    if (matchRules.isEmpty()) {
        m_lastError = u"no match rules given"_s;
        return false;
    }

    m_worker = new Worker(this, matchRules);
    connect(m_worker, &QThread::finished, this, [this] {
        m_worker->deleteLater();
        m_worker = nullptr;
    });
    m_worker->start();
    return true;
}

void DBusMonitor::stop()
{
    if (!m_worker) {
        return;
    }
    Worker *worker = m_worker;
    m_worker = nullptr;
    worker->requestStop();
    if (!worker->wait(2000)) {
        worker->terminate();
        worker->wait(500);
    }
    delete worker;
}

DBusMonitor::Worker::Worker(DBusMonitor *owner, QStringList rules)
    : QThread(owner)
    , m_owner(owner)
    , m_rules(std::move(rules))
{
}

void DBusMonitor::Worker::run()
{
    DBusError error;
    dbus_error_init(&error);

    DBusConnection *connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
    if (!connection) {
        const QString reason = QString::fromUtf8(error.message ? error.message : "cannot reach the session bus");
        dbus_error_free(&error);
        QMetaObject::invokeMethod(m_owner, [owner = m_owner, reason] {
            Q_EMIT owner->failed(reason);
        }, Qt::QueuedConnection);
        return;
    }
    dbus_connection_set_exit_on_disconnect(connection, FALSE);

    DBusMessage *call = dbus_message_new_method_call(DBUS_SERVICE_DBUS,
                                                     DBUS_PATH_DBUS,
                                                     "org.freedesktop.DBus.Monitoring",
                                                     "BecomeMonitor");
    DBusMessageIter iter;
    DBusMessageIter array;
    dbus_message_iter_init_append(call, &iter);
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_STRING_AS_STRING, &array);
    QList<QByteArray> ruleBytes;
    ruleBytes.reserve(m_rules.size());
    for (const QString &rule : std::as_const(m_rules)) {
        ruleBytes.append(rule.toUtf8());
        const char *raw = ruleBytes.last().constData();
        dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &raw);
    }
    dbus_message_iter_close_container(&iter, &array);
    const dbus_uint32_t flags = 0;
    dbus_message_iter_append_basic(&iter, DBUS_TYPE_UINT32, &flags);

    DBusMessage *reply = dbus_connection_send_with_reply_and_block(connection, call, 3000, &error);
    dbus_message_unref(call);

    if (!reply) {
        const QString reason = QString::fromUtf8(error.message ? error.message : "BecomeMonitor was refused");
        dbus_error_free(&error);
        dbus_connection_close(connection);
        dbus_connection_unref(connection);
        QMetaObject::invokeMethod(m_owner, [owner = m_owner, reason] {
            Q_EMIT owner->failed(reason);
        }, Qt::QueuedConnection);
        return;
    }
    dbus_message_unref(reply);

    while (m_stop.loadAcquire() == 0) {
        // A monitor connection never dispatches to handlers, so pop by hand.
        if (!dbus_connection_read_write(connection, 200)) {
            break;
        }
        while (DBusMessage *message = dbus_connection_pop_message(connection)) {
            DBusMessageInfo info;
            switch (dbus_message_get_type(message)) {
            case DBUS_MESSAGE_TYPE_METHOD_CALL:
                info.kind = DBusMessageInfo::MethodCall;
                break;
            case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                info.kind = DBusMessageInfo::MethodReturn;
                break;
            case DBUS_MESSAGE_TYPE_SIGNAL:
                info.kind = DBusMessageInfo::Signal;
                break;
            case DBUS_MESSAGE_TYPE_ERROR:
                info.kind = DBusMessageInfo::Error;
                break;
            default:
                info.kind = DBusMessageInfo::Unknown;
                break;
            }

            // Replies are only interesting when they carry a bare uint32: that
            // is a Notify() answer handing out a notification id. Skipping the
            // rest keeps a bus-wide method_return match rule cheap.
            if (info.kind == DBusMessageInfo::MethodReturn) {
                const char *signature = dbus_message_get_signature(message);
                if (!signature || qstrcmp(signature, "u") != 0) {
                    dbus_message_unref(message);
                    continue;
                }
            }

            const auto str = [](const char *value) {
                return value ? QString::fromUtf8(value) : QString();
            };
            info.sender = str(dbus_message_get_sender(message));
            info.destination = str(dbus_message_get_destination(message));
            info.path = str(dbus_message_get_path(message));
            info.interface = str(dbus_message_get_interface(message));
            info.member = str(dbus_message_get_member(message));
            info.serial = dbus_message_get_serial(message);
            info.replySerial = dbus_message_get_reply_serial(message);
            info.arguments = DBusVariant::argumentsOf(message);

            dbus_message_unref(message);

            QMetaObject::invokeMethod(m_owner, [owner = m_owner, info] {
                Q_EMIT owner->messageReceived(info);
            }, Qt::QueuedConnection);
        }
    }

    dbus_connection_close(connection);
    dbus_connection_unref(connection);
}
