// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import Atoll

/**
 * A second layer surface per output, covering the whole screen, holding
 * nothing but the assistant's edge glow.
 *
 * It has to be its own window: the island's surface is a band along one edge,
 * and the glow needs all four. It takes no pointer input at all, so the
 * desktop underneath keeps working exactly as it did.
 */
Window {
    id: root

    property string screenName: ""
    /** Where the island sits on this output, so the light starts from it. */
    property real originX: 0.5
    property real originY: 0.0

    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
           | Qt.WindowTransparentForInput
    visible: false
    title: "Atoll assistant"

    Component.onCompleted: {
        App.shell.configureOverlay(root, root.screenName)
        visible = true
    }

    EdgeGlow {
        anchors.fill: parent
        lit: App.ai.glowing && Cfg.get("ai.glow", true)
        intensity: Cfg.get("ai.glowIntensity", 0.9)
        thickness: Cfg.get("ai.glowThickness", 130)
        originX: root.originX
        originY: root.originY
    }
}
