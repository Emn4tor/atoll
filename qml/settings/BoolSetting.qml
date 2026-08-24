// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A switch wired straight to a dotted config key. */
SettingRow {
    id: setting

    property string key: ""
    property bool defaultValue: false

    ToggleSwitch {
        checked: Cfg.get(setting.key, setting.defaultValue)
        onToggled: value => Cfg.set(setting.key, value)
    }
}
