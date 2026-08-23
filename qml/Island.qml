// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The island itself: a body whose size is dictated by whichever view currently
 * has the floor, plus a satellite that buds off it when something is playing.
 *
 * There is no background here. The bodies are drawn behind by the stage's goo
 * layers, so that two shapes can visually fuse; this item only carries content,
 * geometry and interaction.
 */
Item {
    id: island

    // ---- what the stage needs to draw ------------------------------------
    readonly property rect mainGeometry: Qt.rect(x, y, width, height)
    readonly property rect satelliteGeometry: Qt.rect(x + width + satelliteGap,
                                                      y + (height - satelliteSize) / 2,
                                                      satelliteSize,
                                                      satelliteSize)
    readonly property real cornerRadius: {
        const configured = Cfg.island.cornerRadius ?? 0
        if (configured > 0) {
            return Math.min(configured, height / 2)
        }
        // The dashboard wants softened corners, not a lozenge.
        return mode === "expanded" ? 26 : height / 2
    }

    // ---- state -----------------------------------------------------------
    property bool expanded: false
    property bool notificationSticky: false
    property var currentNotification: ({})

    readonly property bool hasMedia: App.media.active !== null && (Cfg.modules.media ?? true)
    readonly property bool mediaPlaying: hasMedia && App.media.active.playing
    readonly property bool hovered: hoverHandler.hovered

    /**
     * Priority order, highest first. An OSD outranks a notification because it
     * is a direct response to something the user just did.
     */
    readonly property string mode: {
        if (expanded) {
            return "expanded"
        }
        if (!App.busTapActive && busWarning.running) {
            return "warning"
        }
        if (osdTimer.running) {
            return "osd"
        }
        if (notificationTimer.running || notificationSticky) {
            return "notification"
        }
        if (mediaTimer.running) {
            return "media"
        }
        if (hovered && hasMedia && (Cfg.behavior.hoverPeek ?? true)) {
            return "media"
        }
        return "idle"
    }

    readonly property bool satelliteVisible: mode === "idle" && mediaPlaying && (Cfg.modules.media ?? true)
    property real satelliteSize: satelliteVisible ? Cfg.collapsedHeight : 0
    // A negative gap keeps the satellite tucked inside the body until it is
    // ready to emerge, which is what makes the separation read as budding off.
    property real satelliteGap: satelliteVisible ? 9 : -Cfg.collapsedHeight

    width: Math.max(0, Math.min(Cfg.maxWidth, stack.contentWidth))
    height: Math.max(0, stack.contentHeight)

    Behavior on width {
        SpringAnimation {
            spring: Cfg.spring
            damping: Cfg.damping
            mass: 1.0
            epsilon: 0.25
        }
    }
    Behavior on height {
        SpringAnimation {
            spring: Cfg.spring
            damping: Cfg.damping
            mass: 1.0
            epsilon: 0.25
        }
    }
    Behavior on satelliteSize {
        SpringAnimation {
            spring: Cfg.spring
            damping: 0.5
            epsilon: 0.2
        }
    }
    Behavior on satelliteGap {
        SpringAnimation {
            spring: Cfg.spring
            damping: 0.5
            epsilon: 0.2
        }
    }

    Component.onCompleted: if (App.debugState) {
        console.warn("atoll: island ready; osd=" + App.osd + " notif=" + App.notifications
                     + " media=" + App.media + " ipc=" + App.ipc + " busTap=" + App.busTapActive)
    }

    onModeChanged: if (App.debugState) {
        console.warn("atoll: mode ->", mode, "size", Math.round(width) + "x" + Math.round(height))
    }

    // ---- event plumbing --------------------------------------------------
    Timer {
        id: osdTimer
        interval: Cfg.osdTimeout
    }
    Timer {
        id: notificationTimer
        interval: Cfg.notificationTimeout
    }
    Timer {
        id: mediaTimer
        interval: Cfg.mediaPeekDuration
    }
    Timer {
        id: busWarning
        interval: 9000
    }
    Timer {
        id: leaveTimer
        interval: 420
        onTriggered: {
            if (island.expanded && !island.hovered && (Cfg.behavior.collapseOnLeave ?? true)) {
                island.expanded = false
            }
        }
    }

    onHoveredChanged: {
        if (hovered) {
            leaveTimer.stop()
            // Touching the island acknowledges whatever it was showing.
            notificationSticky = false
        } else if (expanded) {
            leaveTimer.restart()
        }
    }

    Connections {
        target: App.osd

        function onTriggered() {
            if (island.expanded) {
                return // The dashboard already shows the same information.
            }
            osdTimer.restart()
        }

        function onDismissed() {
            osdTimer.stop()
        }
    }

    Connections {
        target: App.notifications

        function onArrived(notification) {
            if (App.debugState) {
                console.warn("atoll: notification from", notification.appName, "-", notification.summary)
            }
            if (island.expanded || (notification.transient ?? false)) {
                return
            }
            island.currentNotification = notification
            island.notificationSticky = (notification.urgency ?? 1) >= 2
                    && (Cfg.notifications.criticalStaysOpen ?? true)
            notificationTimer.restart()
        }
    }

    Connections {
        target: App.media

        function onTrackChanged() {
            if (!(Cfg.media.showOnPlay ?? true) || island.expanded || !island.mediaPlaying) {
                return
            }
            mediaTimer.restart()
        }
    }

    Connections {
        target: App.ipc

        function onExpandRequested() {
            if (App.debugState) {
                console.warn("atoll: ipc expand")
            }
            island.expanded = true
        }
        function onCollapseRequested() {
            island.expanded = false
        }
        function onToggleRequested() {
            island.expanded = !island.expanded
        }
    }

    Connections {
        target: App

        function onBusTapChanged() {
            if (!App.busTapActive) {
                busWarning.restart()
            }
        }
    }

    // The accent follows the current cover art unless the user pinned a colour.
    Binding {
        target: Theme
        property: "dynamicAccent"
        value: App.media.active && App.media.active.artUrl.length > 0 && App.media.active.accent.a > 0
               ? App.media.active.accent
               : Theme.accentFallback
        restoreMode: Binding.RestoreNone
    }

    // ---- content ---------------------------------------------------------
    Item {
        id: stack

        // The island tracks whichever loader is currently in front.
        property bool frontIsA: true
        property Item frontItem: frontIsA ? loaderA.item : loaderB.item
        property real contentWidth: frontItem ? Math.max(frontItem.implicitWidth, 0) : Cfg.collapsedWidth
        property real contentHeight: frontItem ? Math.max(frontItem.implicitHeight, 0) : Cfg.collapsedHeight

        property Component target: {
            switch (island.mode) {
            case "expanded":
                return expandedComponent
            case "warning":
                return warningComponent
            case "osd":
                return osdComponent
            case "notification":
                return notificationComponent
            case "media":
                return mediaComponent
            default:
                return idleComponent
            }
        }

        anchors.fill: parent
        clip: true

        onTargetChanged: {
            // Ping-pong the two loaders so the outgoing view can fade out
            // while the incoming one fades in.
            if (frontIsA) {
                loaderB.sourceComponent = target
            } else {
                loaderA.sourceComponent = target
            }
            frontIsA = !frontIsA
        }

        Component.onCompleted: loaderA.sourceComponent = target

        Loader {
            id: loaderA
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            opacity: stack.frontIsA ? 1 : 0
            visible: opacity > 0
            scale: stack.frontIsA ? 1 : 0.94

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.fast
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }
        }

        Loader {
            id: loaderB
            anchors.centerIn: parent
            width: parent.width
            height: parent.height
            opacity: stack.frontIsA ? 0 : 1
            visible: opacity > 0
            scale: stack.frontIsA ? 0.94 : 1

            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.fast
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }
        }
    }

    // Optional hairline, drawn on top of the goo so it traces the real shape.
    Rectangle {
        anchors.fill: parent
        visible: Theme.border
        radius: island.cornerRadius
        color: "transparent"
        border.width: 1
        border.color: Theme.borderColor
        antialiasing: true
    }

    // ---- satellite -------------------------------------------------------
    Item {
        x: island.width + island.satelliteGap
        y: (island.height - island.satelliteSize) / 2
        width: island.satelliteSize
        height: island.satelliteSize
        visible: island.satelliteSize > 4
        opacity: island.satelliteVisible ? 1 : 0

        Behavior on opacity {
            NumberAnimation {
                duration: Theme.fast
            }
        }

        Spectrum {
            anchors.centerIn: parent
            height: parent.height * 0.42
            barWidth: 2
            spacing: 2
            bars: App.visualizer.bars.slice(0, 4)
        }
    }

    // ---- interaction -----------------------------------------------------
    HoverHandler {
        id: hoverHandler
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: {
            if (island.mode === "notification") {
                island.notificationSticky = false
                notificationTimer.stop()
                return
            }
            if ((Cfg.behavior.clickAction ?? "expand") === "expand") {
                island.expanded = !island.expanded
            }
        }
    }

    TapHandler {
        acceptedButtons: Qt.MiddleButton
        onTapped: {
            if ((Cfg.behavior.middleClickAction ?? "playPause") === "playPause" && island.hasMedia) {
                App.media.active.playPause()
            }
        }
    }

    WheelHandler {
        enabled: Cfg.behavior.scrollAdjustsVolume ?? true
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            const step = Cfg.behavior.volumeStep ?? 5
            App.adjustVolume(event.angleDelta.y > 0 ? step : -step)
        }
    }

    // ---- view components -------------------------------------------------
    Component {
        id: idleComponent
        IdleView {}
    }

    Component {
        id: osdComponent
        OsdView {}
    }

    Component {
        id: mediaComponent
        MediaView {
            interactive: island.hovered
        }
    }

    Component {
        id: notificationComponent
        NotificationView {
            notification: island.currentNotification
            onDismissRequested: {
                island.notificationSticky = false
                notificationTimer.stop()
            }
        }
    }

    Component {
        id: expandedComponent
        ExpandedView {
            onCollapseRequested: island.expanded = false
        }
    }

    Component {
        id: warningComponent
        CallToActionView {}
    }
}
