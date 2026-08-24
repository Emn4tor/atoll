// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A row of pills, one of which is the current value. */
Row {
    id: control

    /** [{ value: "top", label: "Top" }] */
    property var options: []
    property var current: ""

    signal picked(var value)

    spacing: 4

    Repeater {
        model: control.options

        delegate: Rectangle {
            required property var modelData

            readonly property bool active: modelData.value === control.current

            width: Math.max(56, caption.implicitWidth + 22)
            height: 30
            radius: 9
            color: active ? Skin.accent : (hover.hovered ? Skin.cardHover : "#22222a")
            border.width: 1
            border.color: active ? Qt.lighter(Skin.accent, 1.1) : Skin.line

            Behavior on color {
                ColorAnimation {
                    duration: 140
                }
            }

            Text {
                id: caption
                anchors.centerIn: parent
                text: modelData.label
                color: parent.active ? "#101014" : Skin.text
                font.pixelSize: 12
                font.weight: parent.active ? Font.DemiBold : Font.Normal
            }

            HoverHandler {
                id: hover
                cursorShape: Qt.PointingHandCursor
            }

            TapHandler {
                onTapped: control.picked(modelData.value)
            }
        }
    }
}
