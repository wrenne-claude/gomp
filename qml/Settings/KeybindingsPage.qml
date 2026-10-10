import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Rebind gomp's shortcuts. Recording captures the next key combination; clashes are named.
Item {
    id: page
    readonly property string title: qsTr("Keyboard")
    readonly property string subtitle: qsTr("Select a shortcut, then press the new keys. Escape cancels; Backspace clears.")
    property string recording: ""
    property string message: ""

    ListView {
        id: list
        anchors.fill: parent
        leftMargin: Theme.s6
        rightMargin: Theme.s6
        model: App.shortcuts
        clip: true
        T.ScrollBar.vertical: GScrollBar {}
        section.property: "category"
        section.delegate: Item {
            required property string section
            width: Math.min(list.width - Theme.s6 * 2, 900)
            height: 40
            GText { anchors.bottom: parent.bottom; anchors.bottomMargin: 4; text: section; role: "caption"; tone: "tertiary" }
        }
        header: RowLayout {
            width: Math.min(list.width - Theme.s6 * 2, 900)
            height: 40
            GText {
                Layout.fillWidth: true
                text: page.message
                tone: page.message.length ? "caution" : "tertiary"
            }
            GButton { text: qsTr("Reset all"); variant: "ghost"; size: "sm"; iconName: "undo"; onClicked: App.shortcuts.resetAll() }
        }
        delegate: Item {
            id: row
            required property string actionId
            required property string title
            required property string sequence
            required property string defaultSequence
            required property bool isDefault
            required property string conflict
            readonly property bool rec: page.recording === actionId
            width: Math.min(list.width - Theme.s6 * 2, 900)
            height: 48
            Rectangle {
                anchors.fill: parent
                radius: Theme.rMd
                color: row.rec ? Theme.accentSoft : rh.hovered ? Theme.alpha(Theme.surface2, 0.6) : "transparent"
                HoverHandler { id: rh }
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s3
                anchors.rightMargin: Theme.s2
                spacing: Theme.s3
                GText { text: row.title; Layout.fillWidth: true }
                GBadge { visible: row.conflict.length > 0; text: qsTr("Same as %1").arg(row.conflict); tone: "danger"; iconName: "alert" }
                Item {
                    implicitWidth: Math.max(140, keys.implicitWidth + Theme.s4)
                    implicitHeight: Theme.controlSm
                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.rMd
                        color: row.rec ? Theme.surface1 : "transparent"
                        border.width: row.rec ? 2 : 0
                        border.color: Theme.accent
                    }
                    GKbd { id: keys; anchors.right: parent.right; anchors.rightMargin: Theme.s2; anchors.verticalCenter: parent.verticalCenter; sequence: row.sequence; visible: !row.rec && row.sequence.length > 0 }
                    GText { anchors.centerIn: parent; visible: row.rec; text: qsTr("Press keys…"); role: "caption"; tone: "accent" }
                    GText { anchors.centerIn: parent; visible: !row.rec && row.sequence.length === 0; text: qsTr("None"); role: "caption"; tone: "tertiary" }
                    TapHandler { onTapped: { page.recording = row.actionId; page.message = ""; recorder.forceActiveFocus() } }
                }
                GIconButton {
                    size: Theme.controlSm
                    iconName: "undo"
                    tooltip: qsTr("Reset to %1").arg(row.defaultSequence)
                    opacity: row.isDefault ? 0 : 1
                    enabled: !row.isDefault
                    onClicked: App.shortcuts.reset(row.actionId)
                }
            }
        }
    }

    // Captures the next key combination while recording.
    Item {
        id: recorder
        focus: page.recording.length > 0
        Keys.onPressed: event => {
            if (!page.recording.length) return
            event.accepted = true
            const modifierKeys = [Qt.Key_Control, Qt.Key_Shift, Qt.Key_Alt, Qt.Key_Meta, Qt.Key_AltGr]
            if (modifierKeys.indexOf(event.key) >= 0) return
            if (event.key === Qt.Key_Escape && event.modifiers === Qt.NoModifier) { page.recording = ""; return }
            let seq = ""
            if (event.key !== Qt.Key_Backspace || event.modifiers !== Qt.NoModifier) {
                const parts = []
                if (event.modifiers & Qt.ControlModifier) parts.push("Ctrl")
                if (event.modifiers & Qt.AltModifier) parts.push("Alt")
                if (event.modifiers & Qt.ShiftModifier) parts.push("Shift")
                if (event.modifiers & Qt.MetaModifier) parts.push("Meta")
                parts.push(page.keyName(event))
                seq = parts.join("+")
            }
            const clash = App.shortcuts.conflictFor(page.recording, seq)
            const err = App.shortcuts.setSequence(page.recording, seq)
            page.message = err.length ? err : clash.length ? qsTr("%1 also uses %2. Change one of them.").arg(clash).arg(seq) : ""
            page.recording = ""
        }
    }
    function keyName(event) {
        const named = { [Qt.Key_Return]: "Return", [Qt.Key_Enter]: "Return", [Qt.Key_Tab]: "Tab", [Qt.Key_Space]: "Space",
            [Qt.Key_Backspace]: "Backspace", [Qt.Key_Delete]: "Del", [Qt.Key_PageUp]: "PgUp", [Qt.Key_PageDown]: "PgDown",
            [Qt.Key_Home]: "Home", [Qt.Key_End]: "End", [Qt.Key_Left]: "Left", [Qt.Key_Right]: "Right", [Qt.Key_Up]: "Up",
            [Qt.Key_Down]: "Down", [Qt.Key_Escape]: "Esc", [Qt.Key_Comma]: ",", [Qt.Key_Period]: ".", [Qt.Key_Slash]: "/" }
        if (named[event.key]) return named[event.key]
        if (event.key >= Qt.Key_F1 && event.key <= Qt.Key_F35) return "F" + (event.key - Qt.Key_F1 + 1)
        return String.fromCharCode(event.key).toUpperCase()
    }
}
