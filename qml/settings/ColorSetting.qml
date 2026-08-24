// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A colour, as hex, with a swatch and a few presets. */
SettingRow {
    id: setting

    property string key: ""
    property string defaultValue: "#000000"
    property alias presets: field.presets

    ColorField {
        id: field
        value: String(Cfg.get(setting.key, setting.defaultValue))
        onCommitted: value => Cfg.set(setting.key, value)
    }
}
