// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

/** One body of the island: a rounded rectangle that can morph into a circle. */
Rectangle {
    id: blob

    property rect geometry: Qt.rect(0, 0, 0, 0)
    /** Negative radius means "fully rounded", which is the usual case. */
    property real cornerRadius: -1

    x: geometry.x
    y: geometry.y
    width: geometry.width
    height: geometry.height
    radius: cornerRadius >= 0 ? cornerRadius : height / 2
    visible: width > 0 && height > 0
    antialiasing: true
}
