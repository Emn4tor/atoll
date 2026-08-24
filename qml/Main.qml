// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQml
import Atoll

/**
 * One island per output the user asked for.
 *
 * `App.shell.targets` resolves "primary" and "all" against the outputs that
 * actually exist right now, so plugging in a monitor, unplugging one, or
 * moving the primary output adds and removes islands by itself.
 */
Item {
    Instantiator {
        id: islands

        model: App.shell.targets

        delegate: IslandWindow {
            required property string modelData
            screenName: modelData
        }

        onObjectAdded: (index, object) => {
            if (App.debugState) {
                console.warn("atoll: island added on", App.shell.targets[index])
            }
        }
    }

    /**
     * A second surface per output for the assistant's edge glow.
     *
     * It cannot share the island's surface: that one is a band along one screen
     * edge, and the glow needs all four. Creating it up front rather than on
     * demand is deliberate - a layer surface takes a moment to be mapped, and a
     * glow that arrives a beat after the panel it belongs to reads as a glitch.
     */
    Instantiator {
        active: Cfg.aiEnabled
        model: App.shell.targets

        delegate: AiGlowWindow {
            required property string modelData
            screenName: modelData
            // The light starts where the island is, which is what makes it
            // read as coming out of the pill rather than out of the bezel.
            originX: Cfg.position.endsWith("left")
                     ? 0.12
                     : (Cfg.position.endsWith("right") ? 0.88 : 0.5)
            originY: Cfg.atBottom ? 1.0 : 0.0
        }
    }
}
