import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Cheat sheet of every gomp shortcut, grouped; rebinding lives in Settings > Keyboard.
GSheet {
    id: sheet
    title: qsTr("Keyboard shortcuts")
    subtitle: qsTr("Every action is also in the command palette.")
    iconName: "keyboard"
    preferredWidth: 760
    Connections {
        target: Ui
        function onShortcutsOpenChanged() { if (Ui.shortcutsOpen) sheet.open(); else sheet.close() }
    }
    onClosed: Ui.shortcutsOpen = false

    GridLayout {
        Layout.fillWidth: true
        columns: width > 640 ? 2 : 1
        columnSpacing: Theme.s6
        rowSpacing: Theme.s4
        Repeater {
            model: ["General", "Navigation", "Workspaces", "Agent", "Session"]
            ColumnLayout {
                id: group
                required property string modelData
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                spacing: Theme.s1
                GText { text: qsTr(group.modelData); role: "caption"; tone: "tertiary"; Layout.bottomMargin: 2 }
                Repeater {
                    model: { App.shortcuts.map; return App.shortcuts.entries(group.modelData) }
                    RowLayout {
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 26
                        GText { text: parent.modelData.title; Layout.fillWidth: true; lineHeight: 1.0 }
                        GKbd { sequence: parent.modelData.sequence }
                    }
                }
            }
        }
    }
    footer: [
        Item { Layout.fillWidth: true },
        GButton {
            text: qsTr("Change shortcuts")
            variant: "ghost"
            onClicked: { sheet.close(); Ui.openSettings("keybindings") }
        },
        GButton { text: qsTr("Done"); variant: "primary"; onClicked: sheet.close() }
    ]
}
