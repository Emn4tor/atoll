// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

/** Press feedback: a short highlight bloom, clipped to the parent's shape. */
Item {
    id: ripple

    property bool pressed: false
    property color color: Qt.rgba(1, 1, 1, 0.18)
    property real cornerRadius: height / 2

    Rectangle {
        id: glow
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        radius: ripple.cornerRadius
        color: ripple.color
        opacity: 0
        scale: 0.85
    }

    states: State {
        name: "pressed"
        when: ripple.pressed
        PropertyChanges {
            glow.opacity: 1
            glow.scale: 1
        }
    }

    transitions: [
        Transition {
            to: "pressed"
            NumberAnimation {
                properties: "opacity,scale"
                duration: 90
                easing.type: Easing.OutQuad
            }
        },
        Transition {
            from: "pressed"
            NumberAnimation {
                properties: "opacity,scale"
                duration: 260
                easing.type: Easing.OutCubic
            }
        }
    ]
}
