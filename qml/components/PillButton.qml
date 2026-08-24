// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A labelled pill: what the island offers when a click needs a name on it. */
Item {
    id: button

    property alias icon: glyph.names
    property string label: ""
    property color tint: Theme.foreground
    property color fill: Qt.rgba(1, 1, 1, 0.08)
    property color highlight: Qt.rgba(1, 1, 1, 0.16)
    property bool accented: false

    signal clicked()

    implicitHeight: 30
    implicitWidth: row.implicitWidth + 24

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: button.accented
               ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, mouse.containsMouse ? 0.95 : 0.8)
               : (mouse.containsMouse ? button.highlight : button.fill)

        Behavior on color {
            ColorAnimation {
                duration: Theme.fast
            }
        }
    }

    Ripple {
        anchors.fill: parent
        pressed: mouse.pressed
        cornerRadius: height / 2
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: glyph.names.length > 0 && button.label.length > 0 ? 7 : 0

        IconImage {
            id: glyph
            anchors.verticalCenter: parent.verticalCenter
            visible: names.length > 0
            width: visible ? 15 : 0
            height: 15
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: button.label.length > 0
            text: button.label
            color: button.accented ? Theme.background : button.tint
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(11)
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
    }

    scale: mouse.pressed ? 0.96 : 1.0

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
