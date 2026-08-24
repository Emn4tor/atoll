// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The lyrics panel inside the dashboard.
 *
 * Synced lyrics scroll themselves: the line being sung is centred and lit, the
 * ones around it fade out with distance. Clicking a line seeks to it, which
 * turns the panel into a way to navigate a track by its words.
 */
Item {
    id: view

    readonly property var player: App.media.active
    readonly property string lyricState: App.lyrics.state
    readonly property bool synced: App.lyrics.synced
    readonly property bool busy: lyricState === "loading"
    readonly property bool empty: !synced && App.lyrics.plain.length === 0

    implicitHeight: 138

    Column {
        anchors.fill: parent
        spacing: 6

        Item {
            width: parent.width
            height: 16

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Lyrics")
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)
                font.weight: Font.DemiBold
                font.capitalization: Font.AllUppercase
                font.letterSpacing: 0.8
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: text.length > 0
                    text: {
                        switch (view.lyricState) {
                        case "loading":
                            return qsTr("searching")
                        case "missing":
                            return qsTr("nothing found")
                        case "error":
                            return qsTr("offline")
                        case "plain":
                            return qsTr("unsynced")
                        default:
                            return App.lyrics.origin
                        }
                    }
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(9)
                    opacity: 0.75
                }

                RoundButton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    icon: ["view-refresh"]
                    onClicked: App.lyrics.refresh()
                }
            }
        }

        Item {
            width: parent.width
            height: parent.height - 22

            // ---- synced ---------------------------------------------------
            ListView {
                id: lines
                anchors.fill: parent
                visible: view.synced
                clip: true
                model: App.lyrics.lines
                spacing: 2
                boundsBehavior: Flickable.StopAtBounds
                preferredHighlightBegin: height / 2 - 14
                preferredHighlightEnd: height / 2 + 14
                highlightRangeMode: ListView.ApplyRange
                currentIndex: App.lyrics.currentIndex

                // Snapping to the current line is what makes it readable while
                // the track runs; dragging hands control back for a moment.
                onCurrentIndexChanged: if (currentIndex >= 0 && !dragging && !moving) {
                    positionViewAtIndex(currentIndex, ListView.Center)
                }

                delegate: Text {
                    required property int index
                    required property var modelData

                    width: lines.width
                    text: modelData.text.length > 0 ? modelData.text : "· · ·"
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: index === App.lyrics.currentIndex ? Theme.accent : Theme.foreground
                    opacity: index === App.lyrics.currentIndex
                             ? 1
                             : Math.max(0.22, 0.6 - Math.abs(index - App.lyrics.currentIndex) * 0.12)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(index === App.lyrics.currentIndex ? 13 : 12)
                    font.weight: index === App.lyrics.currentIndex ? Font.DemiBold : Font.Normal

                    Behavior on opacity {
                        NumberAnimation {
                            duration: Theme.normal
                        }
                    }
                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.normal
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        enabled: (Cfg.lyrics.seekOnClick ?? true) && view.player && view.player.canSeek
                        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onClicked: view.player.setPositionRatio(
                                       App.lyrics.positionOf(parent.index) / Math.max(1, view.player.length))
                    }
                }
            }

            // ---- unsynced -------------------------------------------------
            Flickable {
                anchors.fill: parent
                visible: !view.synced && App.lyrics.plain.length > 0
                clip: true
                contentHeight: plain.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: plain
                    width: parent.width
                    text: App.lyrics.plain
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.foreground
                    opacity: 0.75
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(11)
                }
            }

            // ---- nothing to show ------------------------------------------
            Text {
                anchors.centerIn: parent
                visible: view.empty
                text: view.busy ? qsTr("Looking for lyrics…") : qsTr("No lyrics for this track")
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(11)

                SequentialAnimation on opacity {
                    running: view.busy
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.4; duration: 700; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutSine }
                }
            }
        }
    }
}
