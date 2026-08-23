// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick

/**
 * A themed icon. `names` may list several candidates, which the C++ provider
 * walks in order - handy because apps are inconsistent about the icon they
 * advertise in a notification hint.
 */
Image {
    id: icon

    property var names: []
    property color tint: "transparent"

    readonly property string joined: Array.isArray(names) ? names.filter(n => !!n).join(",") : String(names ?? "")

    source: joined.length > 0 ? "image://icon/" + joined : ""
    sourceSize.width: width
    sourceSize.height: height
    fillMode: Image.PreserveAspectFit
    smooth: true
    asynchronous: true
    visible: status === Image.Ready
}
