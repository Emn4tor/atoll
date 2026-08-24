// SPDX-FileCopyrightText: 2026 The Atoll contributors
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick
import Atoll

/** A single-line text box that commits on Enter or on losing the focus. */
Rectangle {
    id: field

    property string value: ""
    property string placeholder: ""
    property var validator: null
    property alias inputMethodHints: input.inputMethodHints
    property alias horizontalAlignment: input.horizontalAlignment

    signal committed(string value)

    implicitWidth: 220
    implicitHeight: 32
    radius: 9
    color: Skin.field
    border.width: 1
    border.color: input.activeFocus ? Skin.accent : Skin.line

    onValueChanged: if (!input.activeFocus) {
        input.text = value
    }

    Component.onCompleted: input.text = value

    TextInput {
        id: input
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        verticalAlignment: TextInput.AlignVCenter
        clip: true
        color: Skin.text
        selectionColor: Skin.accent
        selectedTextColor: "#101014"
        font.pixelSize: 12
        validator: field.validator
        selectByMouse: true

        onEditingFinished: field.committed(text)
        Keys.onEscapePressed: {
            text = field.value
            focus = false
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 11
        anchors.verticalCenter: parent.verticalCenter
        text: field.placeholder
        visible: input.text.length === 0 && !input.activeFocus
        color: Skin.muted
        font.pixelSize: 12
        opacity: 0.7
    }
}
