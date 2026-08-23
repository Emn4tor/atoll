// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

/**
 * Text that scrolls only when it does not fit, pausing at both ends so a long
 * track title stays readable instead of sliding past.
 */
Item {
    id: marquee

    property alias text: label.text
    property alias font: label.font
    property alias color: label.color
    property int pauseDuration: 1600
    property real speed: 26 // pixels per second

    implicitWidth: label.implicitWidth
    implicitHeight: label.implicitHeight
    clip: true

    readonly property real overflow: Math.max(0, label.implicitWidth - width)
    readonly property bool scrolling: overflow > 1

    Text {
        id: label
        width: Math.max(implicitWidth, marquee.width)
        renderType: Text.NativeRendering
        elide: Text.ElideNone
        maximumLineCount: 1

        onTextChanged: {
            scroll.stop()
            x = 0
            if (marquee.scrolling) {
                scroll.restart()
            }
        }
    }

    onOverflowChanged: {
        scroll.stop()
        label.x = 0
        if (scrolling) {
            scroll.restart()
        }
    }

    SequentialAnimation {
        id: scroll
        loops: Animation.Infinite
        running: false

        PauseAnimation {
            duration: marquee.pauseDuration
        }
        NumberAnimation {
            target: label
            property: "x"
            to: -marquee.overflow
            duration: Math.max(400, (marquee.overflow / marquee.speed) * 1000)
            easing.type: Easing.InOutQuad
        }
        PauseAnimation {
            duration: marquee.pauseDuration
        }
        NumberAnimation {
            target: label
            property: "x"
            to: 0
            duration: Math.max(300, (marquee.overflow / (marquee.speed * 1.6)) * 1000)
            easing.type: Easing.InOutQuad
        }
    }
}
