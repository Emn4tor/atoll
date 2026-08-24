// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Window
import Atoll

/**
 * The layer-shell surface on one output. It is transparent, click-through
 * everywhere except the island, and never takes focus away from whatever the
 * user is doing.
 *
 * Its geometry belongs to C++: the surface has to be sized and anchored before
 * it is mapped, and it is re-anchored from there whenever the config or the
 * output layout changes.
 */
Window {
    id: root

    property string screenName: ""

    color: App.debugSurface ? "#66ff0000" : "transparent"
    // Refusing focus outright would also refuse the assistant's question box.
    // On layer-shell the compositor already withholds the keyboard until the
    // surface asks for it, so the flag is only needed on the fallback path -
    // and because `layerShellAvailable` never changes, the flags are decided
    // once, before the surface is mapped, rather than swapped underneath it.
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
           | (App.shell.layerShellAvailable ? 0 : Qt.WindowDoesNotAcceptFocus)
    visible: false
    title: "Atoll"

    Component.onCompleted: {
        App.shell.configure(root, root.screenName)
        visible = true
        // Straight after showing: the surface has its layer-shell role by now
        // and has not been rendered into yet, which is what the lock screen
        // permission needs.
        App.shell.allowOnLockScreen(root)
        if (App.debugState) {
            console.warn("atoll: window ready on " + root.screenName + " " + root.width + "x" + root.height)
        }
    }

    Component.onDestruction: App.shell.release(root)

    Stage {
        anchors.fill: parent
        targetWindow: root
        screenName: root.screenName
    }
}
