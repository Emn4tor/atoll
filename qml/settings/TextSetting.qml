// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A free-form string, such as a font name or a date format. */
SettingRow {
    id: setting

    property string key: ""
    property string defaultValue: ""
    property alias placeholder: field.placeholder

    InputField {
        id: field
        width: 240
        value: String(Cfg.get(setting.key, setting.defaultValue))
        onCommitted: value => Cfg.set(setting.key, String(value))
    }
}
