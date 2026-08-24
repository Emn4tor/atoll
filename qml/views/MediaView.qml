// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * Now playing, in the compact form: art, title, a spectrum, and transport
 * controls that fade in when the pointer is over the island.
 *
 * The second line is the one that changes: normally the artist (and album, if
 * asked for), but the line being sung right now whenever synced lyrics exist
 * for the track.
 */
Item {
    id: view

    property bool interactive: false
    readonly property var player: App.media.active

    readonly property string lyric: Cfg.lyricsInIsland ? App.lyrics.currentLine : ""
    readonly property string subtitle: {
        if (!player) {
            return ""
        }
        if (lyric.length > 0) {
            return lyric
        }
        if ((Cfg.media.showAlbum ?? true) && player.album.length > 0 && player.artist.length > 0) {
            return player.artist + " - " + player.album
        }
        return player.artist
    }

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
            visible: Cfg.media.showArt ?? true
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
                id: second
                width: parent.width
                visible: text.length > 0
                text: view.subtitle
                color: view.lyric.length > 0 ? Theme.accent : Theme.muted
                font.family: Theme.fontFamily
                font.pixelSize: Theme.size(10)

                // A lyric line replacing the artist should read as a change of
                // line, not as a caption being overwritten.
                onTextChanged: if (view.lyric.length > 0) {
                    lineIn.restart()
                }

                NumberAnimation {
                    id: lineIn
                    target: second
                    property: "opacity"
                    from: 0.25
                    to: 1
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }
        }

        // The spectrum yields its space to the controls on hover. The slot is
        // only ever as wide as whichever of the two is showing, so neither the
        // bars nor the buttons spill past the island's rounded edge.
        Item {
            anchors.verticalCenter: parent.verticalCenter
            width: view.interactive ? 84 : 26
            height: 24

            Behavior on width {
                NumberAnimation {
                    duration: Theme.normal
                    easing.type: Easing.OutCubic
                }
            }

            Spectrum {
                id: spectrum
                anchors.centerIn: parent
                height: parent.height
                barWidth: 2
                // A handful of bars reads as "sound is happening"; the full
                // band count belongs to a window, not to a notch.
                limit: 5
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
}
