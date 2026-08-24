// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The light that runs out from the island and traces the edges of the screen
 * while the assistant has the floor.
 *
 * It is deliberately only a border: filling or dimming the screen would make
 * the desktop underneath look disabled, and it is not - the user can carry on
 * working while a long job runs. What the glow says is "something of yours is
 * listening", and a lit outline says that without taking the screen away.
 */
Item {
    id: glow

    /** Where the island is, in 0..1 of this surface. */
    property real originX: 0.5
    property real originY: 0.0
    property bool lit: false
    property real intensity: 0.9
    /** Thickness of the band in pixels, converted to the shader's units. */
    property real thickness: 130

    property color colorA: Theme.accent
    property color colorB: "#8f5bff"
    property color colorC: "#4ad9c8"

    /** Runs from 0 to past 1 as the light spreads out from the island. */
    property real progress: 0
    property real clock: 0

    visible: opacity > 0.004
    opacity: lit ? 1 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: glow.lit ? Cfg.ms(220) : Cfg.ms(520)
            easing.type: Easing.OutCubic
        }
    }

    onLitChanged: {
        if (lit) {
            clock = 0
            progress = 0
            spread.restart()
        }
    }

    NumberAnimation {
        id: spread
        target: glow
        property: "progress"
        from: 0
        // Past the far corner, so the wave finishes clearing the screen rather
        // than stalling just short of it.
        to: 1.9
        duration: Cfg.ms(900)
        easing.type: Easing.OutCubic
    }

    FrameAnimation {
        running: glow.visible
        onTriggered: glow.clock += frameTime
    }

    ShaderEffect {
        anchors.fill: parent

        property real time: glow.clock
        property real intensity: glow.intensity
        property real thickness: parent.height > 0 ? glow.thickness / parent.height : 0.12
        property real progress: glow.progress
        property real aspect: parent.height > 0 ? parent.width / parent.height : 1.7
        property vector2d origin: Qt.vector2d(glow.originX, glow.originY)
        property vector4d colorA: Qt.vector4d(glow.colorA.r, glow.colorA.g, glow.colorA.b, 1)
        property vector4d colorB: Qt.vector4d(glow.colorB.r, glow.colorB.g, glow.colorB.b, 1)
        property vector4d colorC: Qt.vector4d(glow.colorC.r, glow.colorC.g, glow.colorC.b, 1)

        blending: true
        fragmentShader: "qrc:/qt/qml/Atoll/shaders/edgeglow.frag.qsb"
    }
}
