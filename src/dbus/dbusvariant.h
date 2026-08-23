/*
 * SPDX-FileCopyrightText: 2026 The Atoll contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#pragma once

#include <QVariant>

// DBusMessageIter is an anonymous struct behind a typedef, so it cannot be
// forward declared; pull in the real header.
#include <dbus/dbus.h>

/** Recursive DBusMessage -> QVariant conversion for the raw libdbus monitor. */
namespace DBusVariant
{
QVariant fromIter(DBusMessageIter *iter);
QVariantList argumentsOf(DBusMessage *message);
}
