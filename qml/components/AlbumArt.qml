// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import QtQuick.Effects
import Atoll

/** Rounded album art with a graceful stand-in when a player exposes none. */
Item {
    id: art

    property string source: ""
    property string fallbackIcon: "media-optical-audio"
    property real cornerRadius: width * 0.22

    implicitWidth: 34
    implicitHeight: 34

    Rectangle {
        id: placeholder
        anchors.fill: parent
        radius: art.cornerRadius
        color: Qt.rgba(1, 1, 1, 0.08)
        visible: cover.status !== Image.Ready

        IconImage {
            anchors.centerIn: parent
            width: parent.width * 0.6
            height: width
            names: [art.fallbackIcon]
            opacity: 0.7
        }
    }

    Image {
        id: cover
        anchors.fill: parent
        source: art.source
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: false
        smooth: true
        visible: false
    }

    MultiEffect {
        anchors.fill: parent
        source: cover
        visible: cover.status === Image.Ready
        maskEnabled: true
        maskSource: mask
    }

    Item {
        id: mask
        anchors.fill: parent
        layer.enabled: true
        visible: false

        Rectangle {
            anchors.fill: parent
            radius: art.cornerRadius
            color: "white"
        }
    }
}
