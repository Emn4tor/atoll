// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The resting state. In "notch" mode it is a bare pill that reads as part of
 * the screen bezel; in "clock" mode it shows the time. Either way it grows a
 * small status cluster on the right when there is something to say.
 */
Item {
    id: view

    readonly property bool showClock: Cfg.idleMode === "clock"
    readonly property bool hasUnread: App.notifications.count > 0 && !App.notifications.doNotDisturb
    readonly property bool playing: App.media.active !== null && App.media.active.playing

    implicitHeight: Cfg.collapsedHeight
    implicitWidth: Cfg.idleMode === "hidden"
                   ? 0
                   : Math.max(Cfg.collapsedWidth, content.implicitWidth + 28)

    Row {
        id: content
        anchors.centerIn: parent
        spacing: 10

        Text {
            visible: view.showClock
            anchors.verticalCenter: parent.verticalCenter
            text: App.clock.time
            color: Theme.foreground
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(13)
            font.weight: Font.DemiBold
        }

        Rectangle {
            visible: view.hasUnread
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            color: Theme.accent

            SequentialAnimation on opacity {
                running: view.hasUnread
                loops: Animation.Infinite
                NumberAnimation { to: 0.35; duration: 900; easing.type: Easing.InOutSine }
                NumberAnimation { to: 1.0; duration: 900; easing.type: Easing.InOutSine }
            }
        }
    }
}
