import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// One tab per workspace: activity lamp, project name, mode light, close. "+" opens a project.
Item {
    id: tabs
    signal openRequested()
    implicitWidth: row.implicitWidth
    implicitHeight: parent ? parent.height : Theme.titlebarHeight

    Row {
        id: row
        height: parent.height
        spacing: 2
        Repeater {
            model: App.workspaces
            delegate: Item {
                id: tab
                required property int index
                required property var workspace
                required property string name
                required property int activity
                required property string mode
                required property bool mock
                required property string title
                readonly property bool current: App.workspaces.currentIndex === index
                height: row.height
                width: Math.min(220, content.implicitWidth + Theme.s3 * 2)
                Accessible.role: Accessible.PageTab
                Accessible.name: name
                Accessible.selected: current

                Rectangle {
                    anchors.fill: parent
                    anchors.topMargin: Theme.s1 + 2
                    anchors.bottomMargin: -1
                    radius: Theme.rMd
                    color: tab.current ? Theme.hull : tabHover.hovered ? Theme.surface1 : "transparent"
                    border.width: tab.current ? 1 : 0
                    border.color: Theme.hairline
                    // The current tab opens into the content below it.
                    Rectangle {
                        visible: tab.current
                        anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 1; rightMargin: 1 }
                        height: Theme.rMd + 1
                        color: Theme.hull
                    }
                }
                Row {
                    id: content
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.verticalCenterOffset: 2
                    x: Theme.s3
                    spacing: Theme.s2
                    GStatusLight {
                        anchors.verticalCenter: parent.verticalCenter
                        diameter: 7
                        tint: tab.activity === Workspace.Idle ? Theme.textDisabled : Theme.activityColor(tab.activity, tab.mode)
                        pulse: tab.activity === Workspace.Approval
                    }
                    GText {
                        anchors.verticalCenter: parent.verticalCenter
                        text: tab.name + (tab.mock ? qsTr(" (demo)") : "")
                        role: tab.current ? "strong" : "body"
                        color: tab.current ? Theme.textPrimary : Theme.textSecondary
                        lineHeight: 1.0
                        width: Math.min(implicitWidth, 140)
                    }
                    GBadge {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: tab.mode !== "default"
                        text: Theme.modeTitle(tab.mode)
                        tone: tab.mode === "plan" ? "plan" : "goal"
                        implicitHeight: 18
                    }
                    GIconButton {
                        anchors.verticalCenter: parent.verticalCenter
                        size: 20
                        iconName: "close"
                        tooltip: qsTr("Close workspace")
                        opacity: tabHover.hovered || tab.current ? 1 : 0
                        focusPolicy: Qt.NoFocus
                        onClicked: App.workspaces.close(tab.index)
                    }
                }
                HoverHandler { id: tabHover }
                TapHandler {
                    acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                    onTapped: (point, button) => {
                        if (button === Qt.MiddleButton)
                            App.workspaces.close(tab.index)
                        else
                            App.workspaces.currentIndex = tab.index
                    }
                }
            }
        }
        GIconButton {
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: 2
            size: Theme.controlSm
            iconName: "plus"
            tooltip: qsTr("Open a project")
            shortcutText: App.shortcuts.map["workspace.new"] || ""
            onClicked: tabs.openRequested()
        }
    }
}
