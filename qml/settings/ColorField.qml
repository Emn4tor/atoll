// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A hex colour box with a swatch, plus the presets worth one click. */
Row {
    id: control

    property string value: "#000000"
    property var presets: []

    signal committed(string value)

    spacing: 8

    Repeater {
        model: control.presets

        delegate: Rectangle {
            required property var modelData

            anchors.verticalCenter: parent.verticalCenter
            width: 24
            height: 24
            radius: 7
            color: modelData
            border.width: control.value.toLowerCase() === String(modelData).toLowerCase() ? 2 : 1
            border.color: control.value.toLowerCase() === String(modelData).toLowerCase()
                          ? Skin.accent : Skin.line

            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                onTapped: control.committed(String(modelData))
            }
        }
    }

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        radius: 7
        color: control.value.startsWith("#") ? control.value : "transparent"
        border.width: 1
        border.color: Skin.line
    }

    InputField {
        anchors.verticalCenter: parent.verticalCenter
        width: 130
        value: control.value
        placeholder: "#rrggbb"
        onCommitted: text => control.committed(String(text).trim())
    }
}
