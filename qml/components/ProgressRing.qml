// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A circular gauge, used for battery level in the expanded dashboard. */
Item {
    id: ring

    property real value: 0
    property real thickness: 3
    property color trackColor: Qt.rgba(1, 1, 1, 0.15)
    property color fillColor: Theme.accent

    implicitWidth: 34
    implicitHeight: 34

    onValueChanged: canvas.requestPaint()
    onFillColorChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            const radius = Math.min(width, height) / 2 - ring.thickness / 2
            const cx = width / 2
            const cy = height / 2

            ctx.lineWidth = ring.thickness
            ctx.lineCap = "round"

            ctx.beginPath()
            ctx.strokeStyle = ring.trackColor
            ctx.arc(cx, cy, radius, 0, Math.PI * 2)
            ctx.stroke()

            const clamped = Math.max(0, Math.min(1, ring.value))
            if (clamped <= 0) {
                return
            }
            ctx.beginPath()
            ctx.strokeStyle = ring.fillColor
            ctx.arc(cx, cy, radius, -Math.PI / 2, -Math.PI / 2 + Math.PI * 2 * clamped)
            ctx.stroke()
        }
    }
}
