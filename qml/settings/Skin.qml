// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
pragma Singleton

import QtQuick
import Atoll

/**
 * The settings window's own palette.
 *
 * It deliberately does not follow `appearance.*`: those keys describe the
 * island, and a settings page that repaints itself while you drag its own
 * colour pickers is impossible to aim. Only the accent is shared, so the
 * window still feels like the island it configures.
 */
QtObject {
    readonly property color window: "#0f0f13"
    readonly property color sidebar: "#0a0a0d"
    readonly property color card: "#17171d"
    readonly property color cardHover: "#1e1e26"
    readonly property color field: "#101015"
    readonly property color text: "#f2f2f5"
    readonly property color muted: "#8f8f9c"
    readonly property color line: "#26262f"
    readonly property color accent: Theme.accent
    readonly property color critical: "#ff5f57"

    readonly property int radius: 14
    readonly property int pagePadding: 28
    readonly property int rowHeight: 44
}
