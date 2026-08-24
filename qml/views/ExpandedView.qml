// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The dashboard the island unfolds into on click: a full transport with a
 * seek bar, the notification backlog, and the handful of toggles worth having
 * one click away.
 */
Item {
    id: view

    readonly property var player: App.media.active
    readonly property bool hasMedia: player !== null && (Cfg.modules.media ?? true)
    readonly property bool hasLyrics: hasMedia && Cfg.lyricsInExpanded
                                      && (App.lyrics.synced || App.lyrics.plain.length > 0
                                          || App.lyrics.state === "loading")

    signal collapseRequested()

    implicitWidth: Cfg.expandedWidth
    implicitHeight: column.implicitHeight + 28

    Column {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        anchors.topMargin: 14
        spacing: 14

        // ---- header -------------------------------------------------------
        Item {
            width: parent.width
            height: 40

            Column {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Text {
                    text: App.clock.time
                    color: Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(22)
                    font.weight: Font.Light
                }

                Text {
                    text: App.clock.date
                    color: Theme.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(11)
                }
            }

            Item {
                visible: App.battery.present && (Cfg.modules.battery ?? true)
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 34

                ProgressRing {
                    anchors.fill: parent
                    value: App.battery.percent / 100
                    fillColor: App.battery.percent <= 15 && !App.battery.charging
                               ? Theme.critical
                               : (App.battery.charging ? Theme.positive : Theme.accent)
                }

                Text {
                    anchors.centerIn: parent
                    text: App.battery.percent
                    color: Theme.foreground
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.size(10)
                    font.weight: Font.DemiBold
                }
            }
        }

        // ---- now playing --------------------------------------------------
        Rectangle {
            visible: view.hasMedia
            width: parent.width
            height: 108
            radius: 16
            color: Qt.rgba(1, 1, 1, 0.06)

            Row {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                AlbumArt {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 72
                    height: 72
                    cornerRadius: 14
                    source: view.player ? view.player.artUrl : ""
                    fallbackIcon: view.player ? view.player.iconName : "media-optical-audio"

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: view.player && view.player.raise()
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 96
                    spacing: 6

                    Marquee {
                        width: parent.width
                        text: view.player ? view.player.title : ""
                        color: Theme.foreground
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.size(13)
                        font.weight: Font.DemiBold
                    }

                    Marquee {
                        width: parent.width
                        text: view.player ? view.player.artist : ""
                        color: Theme.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.size(11)
                    }

                    // Seek bar: click anywhere to jump.
                    Item {
                        width: parent.width
                        height: 14

                        LevelBar {
                            id: seek
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width
                            height: 4
                            fillColor: Theme.accent
                            value: view.player && view.player.length > 0
                                   ? view.player.position / view.player.length
                                   : 0
                        }

                        MouseArea {
                            anchors.fill: parent
                            enabled: view.player ? view.player.canSeek : false
                            cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: mouse => view.player.setPositionRatio(mouse.x / width)
                        }
                    }

                    Row {
                        spacing: 2

                        RoundButton {
                            icon: ["media-skip-backward"]
                            enabled: view.player ? view.player.canGoPrevious : false
                            onClicked: view.player.previous()
                        }

                        RoundButton {
                            width: 34
                            height: 34
                            icon: [view.player && view.player.playing
                                   ? "media-playback-pause" : "media-playback-start"]
                            enabled: view.player ? (view.player.canPlay || view.player.canPause) : false
                            onClicked: view.player.playPause()
                        }

                        RoundButton {
                            icon: ["media-skip-forward"]
                            enabled: view.player ? view.player.canGoNext : false
                            onClicked: view.player.next()
                        }

                        RoundButton {
                            visible: App.media.count > 1
                            icon: ["media-playlist-repeat", "view-refresh"]
                            onClicked: App.media.cycle()
                        }
                    }
                }
            }
        }

        // ---- lyrics -------------------------------------------------------
        Rectangle {
            visible: view.hasLyrics
            width: parent.width
            height: visible ? lyrics.implicitHeight + 24 : 0
            radius: 16
            color: Qt.rgba(1, 1, 1, 0.06)

            LyricsView {
                id: lyrics
                anchors.fill: parent
                anchors.margins: 12
            }
        }

        // ---- notifications ------------------------------------------------
        Item {
            width: parent.width
            height: Math.min(168, Math.max(0, App.notifications.count * 58))
            visible: App.notifications.count > 0

            ListView {
                id: history
                anchors.fill: parent
                clip: true
                spacing: 6
                model: App.notifications
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    required property int index
                    required property var model

                    width: history.width
                    height: 52
                    radius: 12
                    color: Qt.rgba(1, 1, 1, hover.hovered ? 0.1 : 0.05)

                    Behavior on color {
                        ColorAnimation {
                            duration: Theme.fast
                        }
                    }

                    HoverHandler {
                        id: hover
                    }

                    Row {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        IconImage {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 22
                            height: 22
                            names: [model.appIcon, model.appName.toLowerCase(), "dialog-information"]
                        }

                        Column {
                            anchors.verticalCenter: parent.verticalCenter
                            width: parent.width - 70
                            spacing: 1

                            Text {
                                width: parent.width
                                text: model.summary
                                color: Theme.foreground
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.size(11)
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }

                            Text {
                                width: parent.width
                                text: model.body.replace(/<[^>]*>/g, "")
                                visible: text.length > 0
                                color: Theme.muted
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.size(10)
                                elide: Text.ElideRight
                                maximumLineCount: 1
                            }
                        }

                        RoundButton {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 24
                            height: 24
                            opacity: hover.hovered ? 1 : 0
                            icon: ["window-close"]
                            onClicked: App.notifications.close(model.uid)
                        }
                    }
                }
            }
        }

        // ---- toggles ------------------------------------------------------
        Row {
            width: parent.width
            spacing: 8

            QuickToggle {
                icon: [App.notifications.doNotDisturb ? "notifications-disabled" : "notifications"]
                label: qsTr("Silence")
                checked: App.notifications.doNotDisturb
                onToggled: App.notifications.doNotDisturb = !App.notifications.doNotDisturb
            }

            QuickToggle {
                icon: ["edit-clear-history", "edit-delete"]
                label: qsTr("Clear")
                enabled: App.notifications.count > 0
                opacity: App.notifications.count > 0 ? 1 : 0.4
                onToggled: App.notifications.clear()
            }

            QuickToggle {
                icon: ["audio-volume-muted"]
                label: qsTr("Mute")
                onToggled: App.toggleMute()
            }

            QuickToggle {
                visible: Cfg.modules.lyrics ?? true
                icon: ["view-media-lyrics", "view-media-track"]
                label: qsTr("Lyrics")
                checked: Cfg.lyrics.enabled ?? true
                onToggled: App.config.setValue("lyrics.enabled", !(Cfg.lyrics.enabled ?? true))
            }

            QuickToggle {
                icon: ["configure", "settings-configure"]
                label: qsTr("Settings")
                onToggled: {
                    App.openSettings()
                    view.collapseRequested()
                }
            }
        }
    }
}
