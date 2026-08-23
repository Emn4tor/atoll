// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Now playing, in the compact form: art, title, a spectrum, and transport
 * controls that fade in when the pointer is over the island.
 */
Item {
    id: view

    property bool interactive: false
    readonly property var player: App.media.active

    implicitHeight: 52
    implicitWidth: Math.min(Cfg.maxWidth, Math.max(340, layout.implicitWidth + 32))

    Row {
        id: layout
        anchors.centerIn: parent
        spacing: 12

        AlbumArt {
            id: cover
            anchors.verticalCenter: parent.verticalCenter
            width: 36
            height: 36
            source: view.player ? view.player.artUrl : ""
            fallbackIcon: view.player ? view.player.iconName : "media-optical-audio"
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: 178
            spacing: 1

            Marquee {
                width: parent.width
                text: view.player && view.player.title.length > 0
                      ? view.player.title
                      : (view.player ? view.player.identity : "")
                color: Theme.foreground
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(12)
                font.weight: Font.DemiBold
            }

            Marquee {
                width: parent.width
                visible: text.length > 0
                text: view.player ? view.player.artist : ""
                color: Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)
            }
        }

        // The spectrum yields its space to the controls on hover.
        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: 62
            height: 24

            Spectrum {
                id: spectrum
                anchors.centerIn: parent
                height: parent.height
                barWidth: 2
                opacity: view.interactive ? 0 : 1
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.fast
                    }
                }
            }

            Row {
                anchors.centerIn: parent
                spacing: 0
                opacity: view.interactive ? 1 : 0
                visible: opacity > 0

                Behavior on opacity {
                    NumberAnimation {
                        duration: Theme.fast
                    }
                }

                RoundButton {
                    width: 26
                    height: 26
                    icon: ["media-skip-backward"]
                    enabled: view.player ? view.player.canGoPrevious : false
                    onClicked: view.player && view.player.previous()
                }

                RoundButton {
                    width: 28
                    height: 28
                    icon: [view.player && view.player.playing ? "media-playback-pause" : "media-playback-start"]
                    enabled: view.player ? (view.player.canPlay || view.player.canPause) : false
                    onClicked: view.player && view.player.playPause()
                }

                RoundButton {
                    width: 26
                    height: 26
                    icon: ["media-skip-forward"]
                    enabled: view.player ? view.player.canGoNext : false
                    onClicked: view.player && view.player.next()
                }
            }
        }
    }

    // A hairline progress line along the bottom edge of the island.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        height: 2
        radius: 1
        color: Qt.rgba(1, 1, 1, 0.12)
        visible: view.player && view.player.length > 0

        Rectangle {
            height: parent.height
            radius: parent.radius
            color: Theme.accent
            width: parent.width * (view.player && view.player.length > 0
                                   ? Math.min(1, view.player.position / view.player.length)
                                   : 0)

            Behavior on width {
                NumberAnimation {
                    duration: 400
                    easing.type: Easing.Linear
                }
            }
        }
    }
}
