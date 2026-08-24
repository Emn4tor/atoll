// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/**
 * The assistant's settings: which service answers, what it is allowed to touch,
 * and how loudly it announces itself.
 *
 * The permission section is the part worth reading twice. It is written as
 * three plain choices rather than a matrix of checkboxes, because the question
 * it is really asking - how much of my computer does this thing get - is one
 * question, and splitting it into eight would only make it easier to answer
 * wrongly.
 */
SettingsPage {
    id: page

    title: qsTr("Assistant")
    description: qsTr("Hold the island to ask a question. It can answer, and - with your "
                      + "permission, one step at a time - it can act on this machine.")

    readonly property string provider: Cfg.get("ai.provider", "anthropic")

    SettingsGroup {
        title: qsTr("Service")

        BoolSetting {
            label: qsTr("Assistant")
            description: qsTr("Turns the long press, the edge glow and everything below it off.")
            key: "modules.ai"
            defaultValue: true
        }

        ChoiceSetting {
            label: qsTr("Provider")
            description: qsTr("Both need an account with the company that runs them. Nothing is sent anywhere until you ask a question.")
            key: "ai.provider"
            defaultValue: "anthropic"
            options: [
                { value: "anthropic", label: "Claude" },
                { value: "gemini", label: "Gemini" }
            ]
        }

        SettingRow {
            label: page.provider === "gemini" ? qsTr("Gemini API key") : qsTr("Claude API key")
            description: page.provider === "gemini"
                         ? qsTr("From aistudio.google.com. Atoll also reads GEMINI_API_KEY from your environment.")
                         : qsTr("From console.anthropic.com. Atoll also reads ANTHROPIC_API_KEY from your environment.")
            wide: true

            SecretField {
                id: secret
                width: 420

                // The service is asked rather than the config, because a key
                // can live in three different places and only it knows which.
                property int revision: 0
                stored: revision >= 0 && App.ai.hasKeyFor(page.provider)
                backend: revision >= 0 ? App.ai.keyBackendFor(page.provider) : "none"

                onCommitted: value => {
                    App.ai.setKeyFor(page.provider, value)
                    revision++
                }
                onCleared: {
                    App.ai.setKeyFor(page.provider, "")
                    revision++
                }
            }
        }

        SettingRow {
            label: qsTr("Check the connection")
            description: App.ai.keyTestResult.length > 0
                         ? App.ai.keyTestResult
                         : qsTr("Asks the service one throwaway question to prove the key works.")

            PushButton {
                text: qsTr("Test")
                enabled: App.ai.hasKeyFor(page.provider)
                opacity: enabled ? 1 : 0.4
                onClicked: App.ai.testKey()
            }
        }

        TextSetting {
            label: qsTr("Model")
            description: qsTr("Empty means the best general model the provider offers.")
            key: "ai.model"
            defaultValue: ""
            placeholder: page.provider === "gemini" ? "gemini-2.5-pro" : "claude-opus-5"
        }

        BoolSetting {
            label: qsTr("Let it search the web")
            description: qsTr("Uses the provider's own search, so no third service is involved.")
            key: "ai.webSearch"
            defaultValue: true
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("What it may do")

        ChoiceSetting {
            label: qsTr("Permissions")
            description: qsTr("Reading is always allowed. Anything as administrator always asks, whatever this is set to.")
            key: "ai.permissions.mode"
            defaultValue: "guarded"
            options: [
                { value: "readonly", label: qsTr("Look only") },
                { value: "guarded", label: qsTr("Ask first") },
                { value: "trusted", label: qsTr("Trust it") }
            ]
        }

        BoolSetting {
            label: qsTr("May ask for administrator rights")
            description: qsTr("Needed to install software or update the system. Your desktop asks you to confirm; Atoll never sees your password or touches your security key.")
            key: "ai.permissions.allowRoot"
            defaultValue: true
        }

        BoolSetting {
            label: qsTr("May look at your screen")
            description: qsTr("Adds a button to the question box, and lets the assistant ask for a screenshot. Your desktop asks before any picture is taken.")
            key: "ai.allowScreenshots"
            defaultValue: true
        }

        NumberSetting {
            label: qsTr("Give up on a command after")
            description: qsTr("Seconds. A system upgrade needs more than a directory listing does.")
            key: "ai.commandTimeout"
            defaultValue: 180
            from: 10
            to: 3600
            step: 10
        }

        TextSetting {
            label: qsTr("Standing instructions")
            description: qsTr("Added to every conversation. \"I use zsh\", \"explain things simply\", that sort of thing.")
            key: "ai.systemPrompt"
            defaultValue: ""
            placeholder: qsTr("Anything it should always know")
            last: true
        }
    }

    SettingsGroup {
        title: qsTr("Appearance")

        BoolSetting {
            label: qsTr("Light up the screen edges")
            description: qsTr("While the assistant has your attention. It never covers anything: the desktop underneath stays usable.")
            key: "ai.glow"
            defaultValue: true
        }

        SliderSetting {
            label: qsTr("Glow strength")
            key: "ai.glowIntensity"
            defaultValue: 0.9
            from: 0.15
            to: 1.0
            step: 0.05
            enabled: Cfg.get("ai.glow", true)
            opacity: enabled ? 1 : 0.45
        }

        NumberSetting {
            label: qsTr("Glow width")
            description: qsTr("Pixels from the edge inwards.")
            key: "ai.glowThickness"
            defaultValue: 130
            from: 30
            to: 400
            step: 10
            enabled: Cfg.get("ai.glow", true)
            opacity: enabled ? 1 : 0.45
        }

        BoolSetting {
            label: qsTr("Show the face")
            description: qsTr("The little character that blinks while it thinks. Turning it off leaves the text.")
            key: "ai.avatar"
            defaultValue: true
        }

        NumberSetting {
            label: qsTr("Hold the island for")
            description: qsTr("Milliseconds before a press counts as opening the assistant.")
            key: "ai.longPressMs"
            defaultValue: 450
            from: 150
            to: 1500
            step: 50
        }

        NumberSetting {
            label: qsTr("Panel width")
            description: qsTr("How wide the island grows for a conversation.")
            key: "ai.panelWidth"
            defaultValue: 560
            from: 360
            to: 1000
            step: 20
            last: true
        }
    }
}
