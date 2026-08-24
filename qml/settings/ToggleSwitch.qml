// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * A pill switch. `checked` is an input only - it never sets itself, so it can
 * stay bound to the config file it represents.
 */
Item {
    id: control

    property bool checked: false

    signal toggled(bool value)

    implicitWidth: 44
    implicitHeight: 26
    opacity: enabled ? 1 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? Skin.accent : "#2a2a33"
        border.width: 1
        border.color: control.checked ? Qt.lighter(Skin.accent, 1.1) : Skin.line

        Behavior on color {
            ColorAnimation {
                duration: 160
            }
        }
    }

    Rectangle {
        width: parent.height - 8
        height: width
        radius: width / 2
        y: 4
        x: control.checked ? parent.width - width - 4 : 4
        color: control.checked ? "#101014" : "#c9c9d2"

        Behavior on x {
            NumberAnimation {
                duration: 160
                easing.type: Easing.OutCubic
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: control.toggled(!control.checked)
    }
}
