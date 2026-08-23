// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Effects

/**
 * Draws the island's bodies - the main pill and its satellite - and, when the
 * gooey effect is on, blurs and re-thresholds them so the two fuse into a
 * single blob as they approach each other instead of snapping together.
 *
 * The same component doubles as the drop-shadow pass; the shadow is computed
 * from the plain shapes and drawn underneath, where the opaque island covers
 * the source up anyway.
 */
Item {
    id: root

    property rect mainGeometry: Qt.rect(0, 0, 0, 0)
    property rect satelliteGeometry: Qt.rect(0, 0, 0, 0)
    property real mainRadius: -1
    property real satelliteRadius: -1
    property color fill: "#0b0b0e"
    property bool gooey: true
    /**
     * Radius of the blur that creates the merge halo, in pixels. It has to
     * stay well under the size of the smallest body: blur a 32 px circle by
     * 40 px and its centre stops being opaque, which shows up as a shape that
     * is mysteriously darker than its neighbour.
     */
    property real blurRadius: 14
    property bool shadowMode: false
    property real shadowOpacity: 0.45

    component Bodies: Item {
        Blob {
            geometry: root.mainGeometry
            cornerRadius: root.mainRadius
            color: root.shadowMode ? "black" : root.fill
        }
        Blob {
            geometry: root.satelliteGeometry
            cornerRadius: root.satelliteRadius
            color: root.shadowMode ? "black" : root.fill
        }
    }

    // Shadow pass: the shapes themselves are hidden behind the solid island.
    Bodies {
        anchors.fill: parent
        visible: root.shadowMode
        layer.enabled: root.shadowMode
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: 0.7
            shadowVerticalOffset: 6
            shadowColor: "black"
            shadowOpacity: root.shadowOpacity
        }
    }

    // Plain pass: no shader, shapes drawn as-is.
    Bodies {
        anchors.fill: parent
        visible: !root.shadowMode && !root.gooey
    }

    // Gooey pass: blur into a layer, then threshold that layer back to a hard
    // edge. Nesting the two layers is what lets the second effect read the
    // output of the first.
    Item {
        anchors.fill: parent
        visible: !root.shadowMode && root.gooey

        layer.enabled: true
        layer.effect: ShaderEffect {
            // A symmetric blur puts the original outline at exactly 0.5, so
            // cutting there reproduces the shapes at their true size; the
            // narrow softness band is only there to keep the edge smooth.
            property real cutoff: 0.5
            property real softness: 0.06
            property color tint: root.fill
            fragmentShader: "qrc:/qt/qml/Atoll/shaders/gooey.frag.qsb"
        }

        Item {
            anchors.fill: parent
            layer.enabled: true
            layer.effect: MultiEffect {
                blurEnabled: true
                blur: 1.0
                blurMax: Math.round(Math.max(4, root.blurRadius))
                blurMultiplier: 0
            }
            Bodies {
                anchors.fill: parent
            }
        }
    }
}
