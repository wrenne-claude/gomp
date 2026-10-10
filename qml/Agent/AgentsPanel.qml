pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Subagents at a glance; the full lane view is the Subagents screen.
Item {
    id: panel
    property Workspace workspace: null
    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: Theme.s3
        model: panel.workspace ? panel.workspace.subagents : null
        spacing: Theme.s2
        clip: true
        T.ScrollBar.vertical: GScrollBar {}
        header: GButton {
            width: list.width
            visible: list.count > 0
            height: visible ? implicitHeight + Theme.s2 : 0
            text: qsTr("Open the subagent view")
            iconName: "nodes"
            variant: "ghost"
            size: "sm"
            onClicked: Ui.go("subagents")
        }
        delegate: GCard {
            id: card
            required property string agentId
            required property string agent
            required property string task
            required property string status
            required property string currentTool
            required property double tokens
            width: list.width
            height: 64
            interactive: true
            RowLayout {
                anchors.fill: parent
                anchors.margins: Theme.s3
                spacing: Theme.s3
                GStatusLight {
                    tint: card.status === "completed" ? Theme.success : card.status === "failed" || card.status === "aborted" ? Theme.danger : Theme.accent
                    pulse: card.status === "running"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    GText { text: card.agentId; role: "strong"; Layout.fillWidth: true }
                    GText { text: card.currentTool.length ? card.currentTool + "  " + card.task : card.task; role: "caption"; tone: "tertiary"; Layout.fillWidth: true }
                }
                GText { text: Fmt.tokens(card.tokens); role: "readout"; tone: "secondary" }
            }
            TapHandler { onTapped: { Ui.go("subagents"); Ui.openOverlay("subagent", { id: card.agentId }) } }
        }
        GEmptyState {
            anchors.centerIn: parent
            visible: list.count === 0
            iconName: "nodes"
            title: qsTr("No subagents in this turn")
            body: qsTr("When the agent delegates work to parallel agents, each one shows up here with its task and progress.")
        }
    }
}
