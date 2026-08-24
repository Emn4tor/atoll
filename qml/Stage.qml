// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The transparent canvas the island lives on.
 *
 * The layer surface is deliberately much larger than the island so the island
 * can morph without the compositor resizing anything. What keeps the rest of
 * the screen usable is the input region, which is kept clipped to the island's
 * actual bodies and nothing else.
 *
 * The surface is anchored to the top or the bottom edge of the screen by the
 * shell; where the island sits *inside* it is decided here.
 */
Item {
    id: stage

    property var targetWindow: null
    property string screenName: ""

    readonly property bool atBottom: Cfg.atBottom
    readonly property string alignment: Cfg.position.endsWith("left")
                                        ? "left"
                                        : (Cfg.position.endsWith("right") ? "right" : "center")

    /** How far the satellite currently sticks out past the main body. */
    readonly property real satelliteExtent: Math.max(0, island.satelliteGap + island.satelliteSize)
    readonly property real totalWidth: island.width + satelliteExtent

    readonly property real islandX: {
        switch (alignment) {
        case "left":
            return Cfg.sideMargin
        case "right":
            return Math.max(0, stage.width - totalWidth - Cfg.sideMargin)
        default:
            return Math.round((stage.width - totalWidth) / 2)
        }
    }

    GooLayer {
        anchors.fill: parent
        visible: Theme.shadow
        shadowMode: true
        shadowOpacity: Theme.shadowOpacity
        mainGeometry: island.mainGeometry
        satelliteGeometry: island.satelliteGeometry
        mainRadius: island.cornerRadius
        mainTopRadius: island.topRadius
        mainBottomRadius: island.bottomRadius
        satelliteRadius: -1
    }

    GooLayer {
        anchors.fill: parent
        fill: Theme.background
        gooey: Cfg.gooey
        // More strength means a wider halo, so bodies fuse from further apart.
        blurRadius: Cfg.collapsedHeight * (0.22 + 0.5 * Cfg.gooeyStrength)
        mainGeometry: island.mainGeometry
        satelliteGeometry: island.satelliteGeometry
        mainRadius: island.cornerRadius
        mainTopRadius: island.topRadius
        mainBottomRadius: island.bottomRadius
        satelliteRadius: -1
    }

    Island {
        id: island
        targetWindow: stage.targetWindow
        x: stage.islandX
        y: stage.atBottom ? Math.max(0, stage.height - height) : 0
    }

    // The dashboard can be taller than the configured canvas, especially with
    // a lyrics panel and a stack of notifications in it. Ask for the room.
    readonly property int neededHeight: Math.ceil(island.height) + 48
    onNeededHeightChanged: if (targetWindow) {
        App.shell.ensureSurfaceHeight(targetWindow, neededHeight)
    }

    // ---- input region ----------------------------------------------------
    //
    // Applied on a timer rather than on every geometry change: each update is
    // a surface commit, and the island moves 60 times a second while morphing.
    QtObject {
        id: mask

        property string applied: ""

        function regions() {
            const rects = []
            if (island.width > 1 && island.height > 1) {
                rects.push(Qt.rect(island.x, island.y, island.width, island.height))
            }
            if (island.satelliteSize > 1) {
                const s = island.satelliteGeometry
                rects.push(Qt.rect(island.x + s.x, island.y + s.y, s.width, s.height))
            }
            if (rects.length === 0 && (Cfg.behavior.hoverPeek ?? true)) {
                // A hidden island still needs somewhere to be woken up from.
                const centre = stage.islandX + island.width / 2
                rects.push(Qt.rect(centre - 30, stage.atBottom ? stage.height - 3 : 0, 60, 3))
            }
            return rects
        }

        function apply() {
            if (!stage.targetWindow) {
                return
            }
            const rects = regions()
            const key = rects.map(r => [Math.round(r.x), Math.round(r.y),
                                        Math.round(r.width), Math.round(r.height)].join(":")).join("|")
            if (key === applied) {
                return
            }
            applied = key
            App.shell.setInputRegion(stage.targetWindow, rects)
        }
    }

    Timer {
        interval: 40
        running: true
        repeat: true
        triggeredOnStart: true
        onTriggered: mask.apply()
    }
}
