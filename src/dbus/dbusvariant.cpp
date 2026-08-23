/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "dbusvariant.h"

#include <dbus/dbus.h>

#include <QVariantList>
#include <QVariantMap>

namespace DBusVariant
{

QVariant fromIter(DBusMessageIter *iter)
{
    const int type = dbus_message_iter_get_arg_type(iter);

    switch (type) {
    case DBUS_TYPE_BYTE: {
        unsigned char v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(uint(v));
    }
    case DBUS_TYPE_BOOLEAN: {
        dbus_bool_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(bool(v));
    }
    case DBUS_TYPE_INT16: {
        dbus_int16_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(int(v));
    }
    case DBUS_TYPE_UINT16: {
        dbus_uint16_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(uint(v));
    }
    case DBUS_TYPE_INT32: {
        dbus_int32_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(int(v));
    }
    case DBUS_TYPE_UINT32: {
        dbus_uint32_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(uint(v));
    }
    case DBUS_TYPE_INT64: {
        dbus_int64_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(qint64(v));
    }
    case DBUS_TYPE_UINT64: {
        dbus_uint64_t v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(quint64(v));
    }
    case DBUS_TYPE_DOUBLE: {
        double v = 0;
        dbus_message_iter_get_basic(iter, &v);
        return QVariant::fromValue(v);
    }
    case DBUS_TYPE_STRING:
    case DBUS_TYPE_OBJECT_PATH:
    case DBUS_TYPE_SIGNATURE: {
        const char *v = nullptr;
        dbus_message_iter_get_basic(iter, &v);
        return QString::fromUtf8(v ? v : "");
    }
    case DBUS_TYPE_VARIANT: {
        DBusMessageIter sub;
        dbus_message_iter_recurse(iter, &sub);
        return fromIter(&sub);
    }
    case DBUS_TYPE_ARRAY: {
        const int element = dbus_message_iter_get_element_type(iter);

        // ay is almost always a pixel buffer or a blob; keep it compact.
        if (element == DBUS_TYPE_BYTE) {
            DBusMessageIter sub;
            dbus_message_iter_recurse(iter, &sub);
            QByteArray bytes;
            while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_BYTE) {
                unsigned char b = 0;
                dbus_message_iter_get_basic(&sub, &b);
                bytes.append(char(b));
                dbus_message_iter_next(&sub);
            }
            return bytes;
        }

        if (element == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter sub;
            dbus_message_iter_recurse(iter, &sub);
            QVariantMap map;
            while (dbus_message_iter_get_arg_type(&sub) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter entry;
                dbus_message_iter_recurse(&sub, &entry);
                const QVariant key = fromIter(&entry);
                dbus_message_iter_next(&entry);
                map.insert(key.toString(), fromIter(&entry));
                dbus_message_iter_next(&sub);
            }
            return map;
        }

        DBusMessageIter sub;
        dbus_message_iter_recurse(iter, &sub);
        QVariantList list;
        while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
            list.append(fromIter(&sub));
            dbus_message_iter_next(&sub);
        }
        return list;
    }
    case DBUS_TYPE_STRUCT:
    case DBUS_TYPE_DICT_ENTRY: {
        DBusMessageIter sub;
        dbus_message_iter_recurse(iter, &sub);
        QVariantList list;
        while (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_INVALID) {
            list.append(fromIter(&sub));
            dbus_message_iter_next(&sub);
        }
        return list;
    }
    default:
        return {};
    }
}

QVariantList argumentsOf(DBusMessage *message)
{
    QVariantList args;
    DBusMessageIter iter;
    if (!dbus_message_iter_init(message, &iter)) {
        return args;
    }
    while (dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_INVALID) {
        args.append(fromIter(&iter));
        dbus_message_iter_next(&iter);
    }
    return args;
}

}
