// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** An ordinary button, for the few actions a settings page needs. */
Rectangle {
    id: button

    property string text: ""
    property bool destructive: false

    signal clicked()

    implicitWidth: Math.max(96, caption.implicitWidth + 28)
    implicitHeight: 32
    radius: 9
    color: hover.hovered ? Skin.cardHover : "#22222a"
    border.width: 1
    border.color: button.destructive && hover.hovered ? Skin.critical : Skin.line

    Behavior on color {
        ColorAnimation {
            duration: 140
        }
    }

    Text {
        id: caption
        anchors.centerIn: parent
        text: button.text
        color: button.destructive ? Skin.critical : Skin.text
        font.pixelSize: 12
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: button.clicked()
    }
}
