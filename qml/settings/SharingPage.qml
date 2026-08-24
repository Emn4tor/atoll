// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Sharing")
    description: qsTr("Drag files onto the island to send them to a phone or another computer nearby. Atoll speaks LocalSend, so anything running LocalSend can send and receive.")

    SettingsGroup {
        title: qsTr("This device")

        BoolSetting {
            label: qsTr("Sharing")
            description: qsTr("Turns both the drop target and the discovery of nearby devices off.")
            key: "modules.sharing"
            defaultValue: true
        }

        TextSetting {
            label: qsTr("Name")
            description: qsTr("How this machine introduces itself. Empty means its hostname.")
            key: "sharing.alias"
            defaultValue: ""
            placeholder: Qt.application.name
            enabled: Cfg.get("modules.sharing", true)
            opacity: enabled ? 1 : 0.45
        }

        NumberSetting {
            label: qsTr("Port")
            description: qsTr("53317 is what everyone else uses. Atoll moves to the next free port if something already holds this one.")
            key: "sharing.port"
            defaultValue: 53317
            from: 1024
            to: 65535
            step: 1
            enabled: Cfg.get("modules.sharing", true)
            opacity: enabled ? 1 : 0.45
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Receiving")

        BoolSetting {
            label: qsTr("Accept incoming files")
            description: qsTr("Other devices can offer files; the island asks before anything is written.")
            key: "sharing.receive"
            defaultValue: true
            enabled: Cfg.get("modules.sharing", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Take them without asking")
            description: qsTr("Files land in the folder below the moment they are offered. Convenient on a network you trust, and only there.")
            key: "sharing.autoAccept"
            defaultValue: false
            enabled: Cfg.get("modules.sharing", true) && Cfg.get("sharing.receive", true)
            opacity: enabled ? 1 : 0.45
        }

        TextSetting {
            label: qsTr("Save to")
            description: qsTr("Empty means your download folder.")
            key: "sharing.saveDirectory"
            defaultValue: ""
            placeholder: "~/Downloads"
            enabled: Cfg.get("modules.sharing", true) && Cfg.get("sharing.receive", true)
            opacity: enabled ? 1 : 0.45
            last: true
        }
    }
}
