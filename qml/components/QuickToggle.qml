// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A labelled on/off pill for the expanded dashboard. */
Item {
    id: toggle

    property alias icon: glyph.names
    property string label: ""
    property bool checked: false

    signal toggled()

    implicitWidth: 74
    implicitHeight: 58

    Rectangle {
        anchors.fill: parent
        radius: 14
        color: toggle.checked ? Theme.accent : Qt.rgba(1, 1, 1, mouse.containsMouse ? 0.12 : 0.07)

        Behavior on color {
            ColorAnimation {
                duration: Theme.normal
            }
        }
    }

    Ripple {
        anchors.fill: parent
        pressed: mouse.pressed
        cornerRadius: 14
    }

    Column {
        anchors.centerIn: parent
        spacing: 4

        IconImage {
            id: glyph
            width: 20
            height: 20
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Text {
            text: toggle.label
            anchors.horizontalCenter: parent.horizontalCenter
            color: toggle.checked ? "#101014" : Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(10)
            font.weight: Font.Medium
        }
    }

    scale: mouse.pressed ? 0.95 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: Theme.fast
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: toggle.toggled()
    }
}
