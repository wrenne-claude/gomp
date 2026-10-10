pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Plan, context and agent details beside the conversation.
Item {
    id: inspector
    property Workspace workspace: null
    property bool docked: true
    readonly property int pendingBadge: workspace && workspace.planPending ? 1 : 0

    // Draw attention to a plan that needs review.
    Connections {
        target: inspector.workspace
        function onPlanChanged() { if (inspector.workspace.planPending) { Ui.inspectorTab = "plan"; Ui.inspectorOpen = true } }
    }

    Rectangle {
        anchors.fill: parent
        visible: inspector.docked
        color: Theme.surface0
        Rectangle { width: 1; height: parent.height; color: Theme.hairline }
    }
    GFloating {
        anchors.fill: parent
        anchors.margins: Theme.s2
        visible: !inspector.docked
        level: 3
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: inspector.docked ? 0 : Theme.s2
        anchors.leftMargin: inspector.docked ? 1 : Theme.s2
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.s4
            Layout.rightMargin: Theme.s2
            Layout.preferredHeight: Theme.compact ? 48 : 56
            GTabs {
                Layout.fillWidth: true
                value: Ui.inspectorTab
                tabs: [
                    { value: "plan", label: qsTr("Plan"), count: inspector.workspace && inspector.workspace.planPending ? 1 : (inspector.workspace ? inspector.workspace.todos.total - inspector.workspace.todos.completed : 0) },
                    { value: "context", label: qsTr("Context") },
                    { value: "agents", label: qsTr("Agents"), count: inspector.workspace ? inspector.workspace.subagents.running : 0 }
                ]
                onActivated: v => Ui.inspectorTab = v
            }
            GIconButton {
                visible: !inspector.docked
                iconName: "close"
                tooltip: qsTr("Close inspector")
                onClicked: Ui.inspectorOpen = false
            }
        }
        GDivider { Layout.fillWidth: true }

        PlanPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Ui.inspectorTab === "plan"
            workspace: inspector.workspace
        }
        ContextPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Ui.inspectorTab === "context"
            workspace: inspector.workspace
        }
        AgentsPanel {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Ui.inspectorTab === "agents"
            workspace: inspector.workspace
        }
    }
}
