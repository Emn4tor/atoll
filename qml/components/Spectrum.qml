// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The spectrum strip next to the album art. Named Spectrum rather than
 * Visualizer so it does not collide with the C++ type of that name.
 */
Row {
    id: spectrum

    property var bars: App.visualizer.bars
    /** How many bars to draw at most; -1 draws everything the source gives. */
    property int limit: -1
    property color barColor: Theme.accent
    property real barWidth: 2
    property real minimumHeight: 2

    readonly property var values: limit > 0 ? bars.slice(0, limit) : bars

    spacing: 2

    Repeater {
        model: spectrum.values.length

        Rectangle {
            width: spectrum.barWidth
            radius: width / 2
            color: spectrum.barColor
            anchors.verticalCenter: parent.verticalCenter
            height: Math.max(spectrum.minimumHeight,
                             spectrum.height * Math.max(0, Math.min(1, spectrum.values[index] ?? 0)))
            opacity: 0.55 + 0.45 * Math.min(1, spectrum.values[index] ?? 0)

            Behavior on height {
                NumberAnimation {
                    duration: 70
                    easing.type: Easing.OutQuad
                }
            }
        }
    }
}
