// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A list of strings, edited as a comma separated line. */
SettingRow {
    id: setting

    property string key: ""
    property alias placeholder: field.placeholder

    wide: true

    InputField {
        id: field
        width: parent.width
        value: (Cfg.get(setting.key, []) || []).join(", ")
        onCommitted: value => Cfg.set(setting.key,
                                      String(value).split(",")
                                          .map(entry => entry.trim())
                                          .filter(entry => entry.length > 0))
    }
}
