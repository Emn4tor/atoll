// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A circular icon button sized for transport controls. */
Item {
    id: button

    property alias icon: glyph.names
    property real iconScale: 0.52
    property color tint: Theme.foreground

    signal clicked()

    implicitWidth: 30
    implicitHeight: 30
    opacity: enabled ? (mouse.containsMouse ? 1.0 : 0.86) : 0.32

    Behavior on opacity {
        NumberAnimation {
            duration: Theme.fast
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: mouse.containsMouse ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

        Behavior on color {
            ColorAnimation {
                duration: Theme.fast
            }
        }
    }

    Ripple {
        anchors.fill: parent
        pressed: mouse.pressed
        cornerRadius: width / 2
    }

    IconImage {
        id: glyph
        anchors.centerIn: parent
        width: Math.round(parent.width * button.iconScale)
        height: width
    }

    scale: mouse.pressed ? 0.9 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: Theme.fast
            easing.type: Easing.OutBack
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }
}
