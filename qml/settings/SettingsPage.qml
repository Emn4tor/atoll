// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Controls.Basic
import Atoll

/** A scrollable page of setting groups, with its own heading. */
Flickable {
    id: page

    default property alias content: column.data
    property string title: ""
    property string description: ""

    contentWidth: width
    contentHeight: column.implicitHeight + Skin.pagePadding * 2
    boundsBehavior: Flickable.StopAtBounds
    clip: true

    ScrollBar.vertical: ScrollBar {
        policy: page.contentHeight > page.height ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
    }

    Column {
        id: column
        x: Skin.pagePadding
        y: Skin.pagePadding
        width: page.width - Skin.pagePadding * 2 - 8
        spacing: 18

        Column {
            width: parent.width
            spacing: 4
            visible: page.title.length > 0

            Text {
                text: page.title
                color: Skin.text
                font.pixelSize: 22
                font.weight: Font.DemiBold
            }

            Text {
                width: parent.width
                text: page.description
                visible: text.length > 0
                color: Skin.muted
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }
    }
}
