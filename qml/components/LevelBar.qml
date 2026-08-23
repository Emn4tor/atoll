// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A rounded progress track; the fill animates so OSD steps feel continuous. */
Item {
    id: bar

    property real value: 0 // 0..1
    property color trackColor: Qt.rgba(1, 1, 1, 0.16)
    property color fillColor: Theme.foreground

    implicitHeight: 6
    implicitWidth: 120

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: bar.trackColor
    }

    Rectangle {
        width: Math.max(height, parent.width * Math.max(0, Math.min(1, bar.value)))
        height: parent.height
        radius: height / 2
        color: bar.fillColor

        Behavior on width {
            NumberAnimation {
                duration: Theme.fast
                easing.type: Easing.OutCubic
            }
        }
    }

}
