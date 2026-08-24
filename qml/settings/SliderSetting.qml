// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A continuous value. Writes are debounced by the config itself. */
SettingRow {
    id: setting

    property string key: ""
    property real defaultValue: 0
    property alias from: slider.from
    property alias to: slider.to
    property alias step: slider.step
    property alias decimals: slider.decimals
    property alias suffix: slider.suffix

    SliderControl {
        id: slider
        width: 240
        value: Cfg.get(setting.key, setting.defaultValue)
        onMoved: value => Cfg.set(setting.key, value)
    }
}
