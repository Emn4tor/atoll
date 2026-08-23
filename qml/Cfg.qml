// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton

import QtQuick
import Atoll

/**
 * Typed view onto ~/.config/atoll/atoll.json.
 *
 * Everything hangs off one notifying property, so editing the file re-evaluates
 * every binding in the island without a restart.
 */
QtObject {
    id: cfg

    readonly property var data: App.config.data

    readonly property var island: data.island ?? ({})
    readonly property var appearance: data.appearance ?? ({})
    readonly property var effects: data.effects ?? ({})
    readonly property var modules: data.modules ?? ({})
    readonly property var osd: data.osd ?? ({})
    readonly property var notifications: data.notifications ?? ({})
    readonly property var media: data.media ?? ({})
    readonly property var behavior: data.behavior ?? ({})
    readonly property var clock: data.clock ?? ({})

    readonly property int collapsedWidth: island.collapsedWidth ?? 168
    readonly property int collapsedHeight: island.collapsedHeight ?? 32
    readonly property int expandedWidth: island.expandedWidth ?? 460
    readonly property int maxWidth: island.maxWidth ?? 620
    readonly property string idleMode: island.idleMode ?? "notch"

    readonly property bool gooey: effects.gooey ?? true
    readonly property real gooeyStrength: effects.gooeyStrength ?? 0.62
    readonly property real spring: effects.spring ?? 4.2
    readonly property real damping: effects.damping ?? 0.36
    readonly property real animationScale: effects.animationScale ?? 1.0

    readonly property int osdTimeout: osd.timeout ?? 1700
    readonly property int notificationTimeout: notifications.timeout ?? 5000
    readonly property int mediaPeekDuration: media.peekDuration ?? 4200

    function ms(base) {
        return Math.max(1, Math.round(base * animationScale))
    }
}
