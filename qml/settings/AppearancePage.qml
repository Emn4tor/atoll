// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

SettingsPage {
    title: qsTr("Appearance")
    description: qsTr("Colours, type and the physics of the morph. Everything here applies live.")

    SettingsGroup {
        title: qsTr("Colours")

        ChoiceSetting {
            label: qsTr("Accent")
            description: qsTr("\"From the cover\" follows the dominant colour of the album art that is playing.")
            key: "appearance.accent"
            defaultValue: "auto"
            options: [
                { value: "auto", label: qsTr("From the cover") },
                { value: "#5aa2ff", label: qsTr("Blue") },
                { value: "#32d74b", label: qsTr("Green") },
                { value: "#ff9f0a", label: qsTr("Amber") },
                { value: "#ff5f57", label: qsTr("Red") },
                { value: "#bf5af2", label: qsTr("Purple") }
            ]
        }

        ColorSetting {
            label: qsTr("Accent fallback")
            description: qsTr("Used when nothing is playing, or when a cover has no usable colour.")
            key: "appearance.accentFallback"
            defaultValue: "#5aa2ff"
            presets: ["#5aa2ff", "#32d74b", "#ff9f0a", "#bf5af2"]
        }

        ColorSetting {
            label: qsTr("Background")
            key: "appearance.background"
            defaultValue: "#0b0b0e"
            presets: ["#0b0b0e", "#141419", "#000000"]
        }

        SliderSetting {
            label: qsTr("Background opacity")
            key: "appearance.backgroundOpacity"
            defaultValue: 0.97
            from: 0.2
            to: 1
            step: 0.01
            decimals: 2
        }

        ColorSetting {
            label: qsTr("Foreground")
            key: "appearance.foreground"
            defaultValue: "#f4f4f7"
            presets: ["#f4f4f7", "#ffffff"]
        }

        ColorSetting {
            label: qsTr("Secondary text")
            key: "appearance.muted"
            defaultValue: "#9a9aa6"
            presets: ["#9a9aa6", "#7c7c88"]
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Edges")

        BoolSetting {
            label: qsTr("Hairline border")
            key: "appearance.border"
            defaultValue: true
        }

        ColorSetting {
            label: qsTr("Border colour")
            description: qsTr("Alpha first, as #aarrggbb.")
            key: "appearance.borderColor"
            defaultValue: "#1affffff"
            enabled: Cfg.get("appearance.border", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Drop shadow")
            key: "appearance.shadow"
            defaultValue: true
        }

        SliderSetting {
            label: qsTr("Shadow strength")
            key: "appearance.shadowOpacity"
            defaultValue: 0.45
            from: 0
            to: 1
            step: 0.01
            decimals: 2
            enabled: Cfg.get("appearance.shadow", true)
            opacity: enabled ? 1 : 0.45
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Type")

        TextSetting {
            label: qsTr("Font family")
            description: qsTr("Empty follows the system font.")
            key: "appearance.fontFamily"
            placeholder: qsTr("System font")
        }

        SliderSetting {
            label: qsTr("Text size")
            key: "appearance.fontScale"
            defaultValue: 1.0
            from: 0.7
            to: 1.6
            step: 0.05
            decimals: 2
            suffix: "×"
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Motion")

        BoolSetting {
            label: qsTr("Gooey merge")
            description: qsTr("The metaball effect that fuses the island with its satellite while music plays.")
            key: "effects.gooey"
            defaultValue: true
        }

        SliderSetting {
            label: qsTr("Merge strength")
            key: "effects.gooeyStrength"
            defaultValue: 0.62
            from: 0
            to: 1
            step: 0.01
            decimals: 2
            enabled: Cfg.get("effects.gooey", true)
            opacity: enabled ? 1 : 0.45
        }

        SliderSetting {
            label: qsTr("Spring")
            description: qsTr("How eagerly the island snaps to its new shape.")
            key: "effects.spring"
            defaultValue: 4.2
            from: 1
            to: 12
            step: 0.1
            decimals: 1
        }

        SliderSetting {
            label: qsTr("Damping")
            description: qsTr("Lower values overshoot more.")
            key: "effects.damping"
            defaultValue: 0.36
            from: 0.05
            to: 1
            step: 0.01
            decimals: 2
        }

        SliderSetting {
            label: qsTr("Animation speed")
            description: qsTr("A multiplier on every duration. Higher is slower.")
            key: "effects.animationScale"
            defaultValue: 1.0
            from: 0.2
            to: 3
            step: 0.05
            decimals: 2
            suffix: "×"
            last: true
        }
    }
}
