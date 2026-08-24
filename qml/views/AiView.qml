// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The assistant, inside the island.
 *
 * One view rather than several, because every phase is the same conversation
 * seen a moment later: the avatar keeps its place, the panel keeps its width,
 * and only what sits next to the face changes. Switching whole views here
 * would make the island jump on every step the assistant takes, and it takes
 * a lot of them.
 */
Item {
    id: view

    readonly property var ai: App.ai
    readonly property string phase: ai.state

    signal dismissRequested()

    readonly property int panelWidth: Math.min(Cfg.maxAiWidth, Cfg.get("ai.panelWidth", 560))

    implicitWidth: panelWidth
    implicitHeight: Math.max(Cfg.collapsedHeight, content.implicitHeight + 26)

    function focusInput() {
        if (composer.visible) {
            prompt.forceActiveFocus()
        }
    }

    Connections {
        target: App.ai

        function onFocusRequested() {
            Qt.callLater(view.focusInput)
        }
    }

    // The view is built *because* the assistant was engaged, so the signal that
    // asks for the keyboard has already gone out by the time anything here can
    // hear it. Claiming the focus on arrival is what actually puts the cursor
    // in the box after a long press.
    Component.onCompleted: Qt.callLater(view.focusInput)

    // ---- the face --------------------------------------------------------
    readonly property string mood: {
        switch (phase) {
        case "setup":
            return "asleep"
        case "composing":
            return prompt.activeFocus && prompt.text.length > 0 ? "listening" : "idle"
        case "thinking":
            return "thinking"
        case "answering":
            return "idle"
        case "working":
            return "working"
        case "permission":
            return "alert"
        case "done":
            return "done"
        case "failed":
            return "alert"
        default:
            return "idle"
        }
    }

    Column {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 10

        // ---- head row: face, and whatever the assistant is doing ----------
        Item {
            width: parent.width
            height: Math.max(bot.height, headText.implicitHeight, composer.implicitHeight)

            BloubBot {
                id: bot
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                mood: view.mood
                bodyColor: view.phase === "failed" || view.phase === "permission"
                           ? Theme.critical
                           : Theme.accent
                size: 26
                visible: Cfg.get("ai.avatar", true)
            }

            // The prompt box, which is the only thing here that takes input.
            Item {
                id: composer
                anchors.left: bot.visible ? bot.right : parent.left
                anchors.leftMargin: bot.visible ? 10 : 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                implicitHeight: 34
                height: implicitHeight
                visible: view.phase === "composing"

                Rectangle {
                    anchors.fill: parent
                    radius: height / 2
                    color: Qt.rgba(1, 1, 1, prompt.activeFocus ? 0.10 : 0.06)
                    border.width: 1
                    border.color: prompt.activeFocus
                                  ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.55)
                                  : Theme.borderColor

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.fast
                        }
                    }
                }

                TextInput {
                    id: prompt
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: actions.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.foreground
                    selectionColor: Theme.accent
                    selectedTextColor: Theme.background
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(13)
                    selectByMouse: true
                    clip: true

                    onAccepted: {
                        view.ai.ask(text)
                        text = ""
                    }
                    Keys.onEscapePressed: view.dismissRequested()
                    Keys.onUpPressed: if (text.length === 0 && view.ai.question.length > 0) {
                        text = view.ai.question
                        cursorPosition = text.length
                    }
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 15
                    anchors.verticalCenter: parent.verticalCenter
                    visible: prompt.text.length === 0
                    text: qsTr("Ask %1").arg(view.ai.providerLabel)
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(13)
                }

                Row {
                    id: actions
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    RoundButton {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 26
                        height: 26
                        visible: view.ai.screenAvailable && Cfg.get("ai.allowScreenshots", true)
                        icon: ["video-display", "camera-photo", "image-x-generic"]
                        tint: view.ai.shareScreen ? Theme.accent : Theme.foreground
                        opacity: view.ai.shareScreen ? 1 : 0.6
                        onClicked: view.ai.shareScreen = !view.ai.shareScreen
                    }

                    RoundButton {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 26
                        height: 26
                        enabled: prompt.text.trim().length > 0
                        icon: ["go-up", "document-send", "mail-send"]
                        tint: Theme.accent
                        onClicked: {
                            view.ai.ask(prompt.text)
                            prompt.text = ""
                        }
                    }
                }
            }

            // Everything that is not the prompt box says one line here.
            Column {
                id: headText
                anchors.left: bot.visible ? bot.right : parent.left
                anchors.leftMargin: bot.visible ? 10 : 0
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                visible: !composer.visible
                spacing: 2

                Text {
                    width: parent.width
                    text: {
                        switch (view.phase) {
                        case "setup":
                            return qsTr("Set up your assistant")
                        case "thinking":
                            return qsTr("Thinking…")
                        case "working":
                            return view.ai.activity.length > 0 ? view.ai.activity : qsTr("Working…")
                        case "permission":
                            return view.ai.pendingSummary
                        case "failed":
                            return qsTr("That did not work")
                        default:
                            return view.ai.question
                        }
                    }
                    color: view.phase === "failed" ? Theme.critical : Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(13)
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }

                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: {
                        switch (view.phase) {
                        case "setup":
                            return qsTr("Add a key for Claude or Gemini and the island can act on this machine.")
                        case "permission":
                            return view.ai.pendingTier
                        case "thinking":
                            return view.ai.thought.length > 0
                                   ? view.ai.thought.slice(-110)
                                   : qsTr("Asking %1").arg(view.ai.providerLabel)
                        default:
                            return ""
                        }
                    }
                    color: view.phase === "permission" ? Theme.accent : Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(10)
                    elide: Text.ElideRight
                    maximumLineCount: 1
                }
            }
        }

        // ---- what the assistant is about to do ----------------------------
        Rectangle {
            width: parent.width
            visible: view.phase === "permission" && view.ai.pendingDetail.length > 0
            height: visible ? detail.implicitHeight + 18 : 0
            radius: 9
            color: Qt.rgba(1, 1, 1, 0.05)
            border.width: 1
            border.color: Theme.borderColor

            Text {
                id: detail
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 11
                text: view.ai.pendingDetail
                color: Theme.foreground
                font.family: "monospace"
                font.pixelSize: Theme.size(10)
                wrapMode: Text.Wrap
                maximumLineCount: 4
                elide: Text.ElideRight
            }
        }

        // ---- the answer ---------------------------------------------------
        Flickable {
            width: parent.width
            visible: answer.text.length > 0
                     && (view.phase === "answering" || view.phase === "done"
                         || view.phase === "working" || view.phase === "thinking")
            height: visible ? Math.min(answer.implicitHeight, 190) : 0
            contentHeight: answer.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            // Follow the text as it streams, but stop following the moment the
            // user scrolls back to read something.
            onContentHeightChanged: if (!moving && !flicking) {
                contentY = Math.max(0, contentHeight - height)
            }

            Text {
                id: answer
                width: parent.width
                text: view.ai.answer
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(12)
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                lineHeight: 1.25
            }
        }

        Text {
            width: parent.width
            visible: view.phase === "failed" && view.ai.error.length > 0
            text: view.ai.error
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(11)
            wrapMode: Text.Wrap
            maximumLineCount: 3
            elide: Text.ElideRight
        }

        // ---- what it has done so far --------------------------------------
        Column {
            width: parent.width
            visible: view.ai.steps.length > 0
                     && (view.phase === "working" || view.phase === "permission"
                         || view.phase === "done")
            spacing: 3

            Repeater {
                // Only the tail: the island is not a log viewer, and the last
                // few lines are what tells the user where things stand.
                model: view.ai.steps.slice(-4)

                delegate: Row {
                    required property var modelData

                    width: parent.width
                    spacing: 7

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 5
                        height: 5
                        radius: 2.5
                        color: {
                            switch (modelData.status) {
                            case "done":
                                return Theme.positive
                            case "failed":
                            case "denied":
                                return Theme.critical
                            case "elevated":
                                return "#f0a020"
                            default:
                                return Theme.accent
                            }
                        }

                        SequentialAnimation on opacity {
                            running: modelData.status === "running" || modelData.status === "elevated"
                            loops: Animation.Infinite
                            NumberAnimation { to: 0.3; duration: 600; easing.type: Easing.InOutSine }
                            NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutSine }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 12
                        text: modelData.text
                        color: modelData.status === "denied" ? Theme.muted : Theme.foreground
                        opacity: modelData.status === "done" ? 0.65 : 0.9
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.size(10)
                        elide: Text.ElideRight
                        maximumLineCount: 1
                    }
                }
            }
        }

        // ---- buttons -------------------------------------------------------
        Row {
            width: parent.width
            visible: buttons.count > 0
            spacing: 7

            Repeater {
                id: buttons
                model: view.buttonModel

                delegate: PillButton {
                    required property var modelData

                    accented: modelData.accented ?? false
                    tint: (modelData.destructive ?? false) ? Theme.critical : Theme.foreground
                    icon: modelData.icon ?? []
                    label: modelData.label
                    onClicked: modelData.action()
                }
            }
        }

        Text {
            width: parent.width
            visible: view.phase === "permission" && view.ai.pendingElevated
            text: qsTr("Your system will ask you to confirm. Atoll never sees your password.")
            color: Theme.muted
            font.family: Theme.fontFamily
            font.pixelSize: Theme.size(9)
            wrapMode: Text.Wrap
        }
    }

    /**
     * The buttons for the current phase. Kept as data so the row above stays
     * one Repeater instead of a stack of conditionals.
     */
    readonly property var buttonModel: {
        switch (phase) {
        case "setup":
            return [
                { label: qsTr("Open settings"), accented: true, icon: ["configure", "settings-configure"],
                  action: () => { App.openSettings("ai"); view.dismissRequested() } },
                { label: qsTr("Not now"), action: () => view.dismissRequested() }
            ]
        case "thinking":
        case "answering":
        case "working":
            return [
                { label: qsTr("Continue in background"), icon: ["go-down", "window-minimize"],
                  action: () => { view.ai.continueInBackground(); view.dismissRequested() } },
                { label: qsTr("Stop"), destructive: true, action: () => view.ai.cancel() }
            ]
        case "permission":
            if (view.ai.pendingElevated) {
                return [
                    { label: qsTr("Allow"), accented: true, icon: ["security-high", "dialog-password"],
                      action: () => view.ai.allow(false) },
                    { label: qsTr("Don't allow"), destructive: true, action: () => view.ai.deny() }
                ]
            }
            return [
                { label: qsTr("Allow once"), accented: true, action: () => view.ai.allow(false) },
                { label: qsTr("Allow this session"), action: () => view.ai.allow(true) },
                { label: qsTr("No"), destructive: true, action: () => view.ai.deny() }
            ]
        case "done":
            return [
                { label: qsTr("Ask something else"), accented: true,
                  action: () => { view.ai.engage(); Qt.callLater(view.focusInput) } },
                { label: qsTr("New conversation"), action: () => view.ai.startOver() },
                { label: qsTr("Close"), action: () => view.dismissRequested() }
            ]
        case "failed":
            return [
                { label: qsTr("Try again"), accented: true,
                  action: () => { view.ai.engage(); Qt.callLater(view.focusInput) } },
                { label: qsTr("Close"), action: () => view.dismissRequested() }
            ]
        default:
            return []
        }
    }
}
