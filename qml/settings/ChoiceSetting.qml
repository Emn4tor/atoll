// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** One of a handful of values, laid out as pills. */
SettingRow {
    id: setting

    property string key: ""
    property var options: []
    property var defaultValue: ""

    wide: options.length > 3

    Segmented {
        options: setting.options
        current: Cfg.get(setting.key, setting.defaultValue)
        onPicked: value => Cfg.set(setting.key, value)
    }
}
