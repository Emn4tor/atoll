// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import Atoll

/**
 * The layer-shell surface. It is transparent, click-through everywhere except
 * the island, and never takes focus away from whatever the user is doing.
 */
Window {
    id: root

    width: App.shell.surfaceWidth
    height: App.shell.surfaceHeight
    color: App.debugSurface ? "#66ff0000" : "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus
    visible: false
    title: "Atoll"

    Component.onCompleted: {
        App.shell.configure(root)
        visible = true
        if (App.debugState) {
            console.warn("atoll: window ready " + root + " app=" + App)
        }
    }

    Stage {
        anchors.fill: parent
        targetWindow: root
    }
}
