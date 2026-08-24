// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Behaviour")
    description: qsTr("What the pointer does to the island.")

    SettingsGroup {
        title: qsTr("Pointer")

        ChoiceSetting {
            label: qsTr("Click")
            key: "behavior.clickAction"
            defaultValue: "expand"
            options: [
                { value: "expand", label: qsTr("Expand") },
                { value: "none", label: qsTr("Nothing") }
            ]
        }

        ChoiceSetting {
            label: qsTr("Middle click")
            key: "behavior.middleClickAction"
            defaultValue: "playPause"
            options: [
                { value: "playPause", label: qsTr("Play / pause") },
                { value: "none", label: qsTr("Nothing") }
            ]
        }

        ChoiceSetting {
            label: qsTr("Right click")
            key: "behavior.rightClickAction"
            defaultValue: "settings"
            options: [
                { value: "settings", label: qsTr("Open settings") },
                { value: "collapse", label: qsTr("Collapse") },
                { value: "none", label: qsTr("Nothing") }
            ]
        }

        BoolSetting {
            label: qsTr("Scroll changes the volume")
            key: "behavior.scrollAdjustsVolume"
            defaultValue: true
        }

        NumberSetting {
            label: qsTr("Volume step")
            key: "behavior.volumeStep"
            defaultValue: 5
            from: 1
            to: 25
            suffix: "%"
            enabled: Cfg.get("behavior.scrollAdjustsVolume", true)
            opacity: enabled ? 1 : 0.45
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Hover")

        BoolSetting {
            label: qsTr("Peek on hover")
            description: qsTr("Pointing at the island shows what is playing without clicking.")
            key: "behavior.hoverPeek"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("Expand on hover")
            description: qsTr("Unfolds the whole dashboard when the pointer rests on the island.")
            key: "behavior.expandOnHover"
            defaultValue: false
        }

        NumberSetting {
            label: qsTr("Hover delay")
            key: "behavior.hoverDelay"
            defaultValue: 180
            from: 0
            to: 2000
            step: 10
            suffix: " ms"
        }

        BoolSetting {
            label: qsTr("Collapse when the pointer leaves")
            key: "behavior.collapseOnLeave"
            defaultValue: true
            last: true
        }
    }
}
