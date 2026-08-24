// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

/**
 * One body of the island: a rounded rectangle that can morph into a circle, a
 * lozenge, or a notch - a shape whose corners differ per side, so it can grow
 * out of the screen edge instead of floating below it.
 */
Rectangle {
    id: blob

    property rect geometry: Qt.rect(0, 0, 0, 0)
    /** Negative radius means "fully rounded", which is the usual case. */
    property real cornerRadius: -1
    /** Per-corner overrides; negative keeps `cornerRadius`. */
    property real topRadius: -1
    property real bottomRadius: -1

    readonly property real baseRadius: cornerRadius >= 0 ? cornerRadius : height / 2

    x: geometry.x
    y: geometry.y
    width: geometry.width
    height: geometry.height
    radius: baseRadius
    topLeftRadius: topRadius >= 0 ? topRadius : baseRadius
    topRightRadius: topRadius >= 0 ? topRadius : baseRadius
    bottomLeftRadius: bottomRadius >= 0 ? bottomRadius : baseRadius
    bottomRightRadius: bottomRadius >= 0 ? bottomRadius : baseRadius
    visible: width > 0 && height > 0
    antialiasing: true
}
