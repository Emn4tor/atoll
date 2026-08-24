// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A number with steppers. */
SettingRow {
    id: setting

    property string key: ""
    property real defaultValue: 0
    property alias from: field.from
    property alias to: field.to
    property alias step: field.step
    property alias decimals: field.decimals
    property alias suffix: field.suffix

    NumberField {
        id: field
        value: Cfg.get(setting.key, setting.defaultValue)
        onCommitted: value => Cfg.set(setting.key, value)
    }
}
