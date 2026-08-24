// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** The little square next to a number field. */
Rectangle {
    id: button

    property string glyph: "+"

    signal clicked()

    width: 30
    height: 32
    radius: 9
    color: hover.hovered ? Skin.cardHover : "#22222a"
    border.width: 1
    border.color: Skin.line

    Text {
        anchors.centerIn: parent
        text: button.glyph
        color: Skin.text
        font.pixelSize: 15
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }

    TapHandler {
        onTapped: button.clicked()
    }
}
