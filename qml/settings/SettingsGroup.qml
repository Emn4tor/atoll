// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A titled card holding a run of setting rows. */
Column {
    id: group

    property string title: ""
    default property alias rows: body.data

    width: parent ? parent.width : 0
    spacing: 8

    Text {
        text: group.title
        visible: text.length > 0
        color: Skin.muted
        font.pixelSize: 11
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.9
    }

    Rectangle {
        width: parent.width
        height: body.implicitHeight + 8
        radius: Skin.radius
        color: Skin.card
        border.width: 1
        border.color: Skin.line

        Column {
            id: body
            x: 0
            y: 4
            width: parent.width
        }
    }
}
