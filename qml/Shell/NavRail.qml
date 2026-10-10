pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Primary navigation. Screens, then help at the bottom. Badges carry live counts only.
Rectangle {
    id: rail
    implicitWidth: Theme.railWidth
    color: Theme.surface0
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.hairline }

    readonly property var ws: App.workspaces.current

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.s3
        anchors.bottomMargin: Theme.s3
        spacing: Theme.s1 + 2

        Repeater {
            model: [
                { screen: "agent", icon: "corona", label: qsTr("Agent"), shortcut: App.shortcuts.map["nav.agent"] },
                { screen: "sessions", icon: "history", label: qsTr("Sessions"), shortcut: App.shortcuts.map["nav.sessions"] },
                { screen: "subagents", icon: "nodes", label: qsTr("Subagents"), shortcut: App.shortcuts.map["nav.subagents"] },
                { screen: "settings", icon: "sliders", label: qsTr("Settings"), shortcut: App.shortcuts.map["nav.settings"] }
            ]
            delegate: Item {
                id: navItem
                required property var modelData
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 40
                implicitHeight: 40
                readonly property bool selected: Ui.screen === navItem.modelData.screen
                GIconButton {
                    anchors.fill: parent
                    size: 40
                    iconName: navItem.modelData.icon
                    tooltip: navItem.modelData.label
                    shortcutText: navItem.modelData.shortcut || ""
                    checked: navItem.selected
                    iconColor: navItem.selected ? Theme.accent : hovered ? Theme.textPrimary : Theme.textTertiary
                    onClicked: Ui.go(navItem.modelData.screen)
                }
                // Selection marker on the rail edge, sliding between items.
                Rectangle {
                    x: -Theme.s2 - 2
                    width: 3
                    height: navItem.selected ? 20 : 0
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 1.5
                    color: Theme.accent
                    Behavior on height { NumberAnimation { duration: Theme.dBase; easing.type: Theme.easeOut } }
                }
                GBadge {
                    readonly property int running: rail.ws && navItem.modelData.screen === "subagents" ? rail.ws.subagents.running : 0
                    readonly property int staged: navItem.modelData.screen === "settings" ? App.settings.stagedCount : 0
                    visible: running > 0 || staged > 0
                    text: running > 0 ? String(running) : String(staged)
                    tone: running > 0 ? "accent" : "caution"
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.rightMargin: -4
                    anchors.topMargin: -2
                    implicitHeight: 16
                }
            }
        }
        Item { Layout.fillHeight: true }
        GIconButton {
            Layout.alignment: Qt.AlignHCenter
            size: 40
            iconName: "keyboard"
            tooltip: qsTr("Keyboard shortcuts")
            shortcutText: App.shortcuts.map["help.shortcuts"] || ""
            iconColor: hovered ? Theme.textPrimary : Theme.textTertiary
            onClicked: Ui.shortcutsOpen = true
        }
    }
}
