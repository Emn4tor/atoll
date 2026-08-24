// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Shown when the session bus refused to let Atoll observe traffic. Without
 * that tap there are no notifications and no OSD, so say it plainly rather
 * than sitting there looking broken.
 */
Item {
    id: view

    implicitHeight: 62
    implicitWidth: Math.min(Cfg.maxWidth, row.implicitWidth + Math.max(36, Theme.edgeInset * 2))

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 12

        IconImage {
            anchors.verticalCenter: parent.verticalCenter
            width: 24
            height: 24
            names: ["dialog-warning"]
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Text {
                text: qsTr("Atoll cannot watch the session bus")
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(12)
                font.weight: Font.DemiBold
            }

            Text {
                text: App.busError
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)
                elide: Text.ElideRight
                maximumLineCount: 1
                width: 300
            }
        }
    }
}
