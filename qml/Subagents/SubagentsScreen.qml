pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Delegated work: the agent that spawned it on the left, one lane per subagent with live status,
// the tools it has used and what it is doing now. Selecting a lane opens its output.
Item {
    id: screen
    readonly property Workspace ws: App.workspaces.current
    readonly property SubagentModel agents: ws ? ws.subagents : null
    property string selectedId: ""
    readonly property bool drawerDocked: width >= 1300
    readonly property int hubWidth: 168

    Connections {
        target: Ui
        function onOpenOverlay(name, props) { if (name === "subagent") screen.selectedId = props.id }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.rightMargin: screen.selectedId.length && screen.drawerDocked ? drawer.width : 0
        spacing: 0

        // ---- Summary ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.s5
            Layout.bottomMargin: Theme.s3
            spacing: Theme.s5
            ColumnLayout {
                spacing: 2
                GText { text: qsTr("Subagents"); role: "title" }
                GText {
                    text: screen.ws ? qsTr("Parallel work delegated by the agent in %1").arg(screen.ws.name) : qsTr("No workspace open")
                    tone: "tertiary"
                    role: "caption"
                }
            }
            Item { Layout.fillWidth: true }
            component Count: ColumnLayout {
                property string label
                property string value
                property color tint: Theme.textPrimary
                spacing: 0
                GText { text: parent.value; role: "readoutLarge"; color: parent.tint; Layout.alignment: Qt.AlignRight }
                GText { text: parent.label; role: "caption"; tone: "tertiary"; Layout.alignment: Qt.AlignRight }
            }
            Count { label: qsTr("running"); value: screen.agents ? String(screen.agents.running) : "0"; tint: Theme.accent }
            Count { label: qsTr("finished"); value: screen.agents ? String(screen.agents.finished) : "0"; tint: Theme.success }
            Count { label: qsTr("failed"); value: screen.agents ? String(screen.agents.failed) : "0"; tint: screen.agents && screen.agents.failed > 0 ? Theme.danger : Theme.textTertiary }
            Count { label: qsTr("cost"); value: screen.agents ? Fmt.cost(screen.agents.totalCost) : "—" }
        }
        GDivider { Layout.fillWidth: true }

        Flickable {
            id: flick
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentHeight: lanes.height + Theme.s6
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: GScrollBar {}

            Item {
                id: lanes
                x: Theme.s5
                y: Theme.s5
                width: flick.width - Theme.s5 * 2
                height: column.height
                visible: screen.agents && screen.agents.count > 0

                // Hub: the agent that delegated the work.
                GCard {
                    id: hub
                    width: screen.hubWidth
                    height: 72
                    y: Math.max(0, column.height / 2 - height / 2)
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s3
                        spacing: 2
                        RowLayout {
                            spacing: Theme.s2
                            Corona { workspace: screen.ws; size: 18 }
                            GText { text: qsTr("Main agent"); role: "strong" }
                        }
                        GText {
                            text: screen.ws ? screen.ws.modelName : ""
                            role: "caption"
                            tone: "tertiary"
                            Layout.fillWidth: true
                        }
                    }
                }

                // Connectors from the hub to each lane.
                Repeater {
                    model: screen.agents
                    delegate: Shape {
                        id: connector
                        required property int index
                        required property var model // `status` would shadow Shape.status
                        readonly property string agentStatus: model.status
                        readonly property Item lane: laneRepeater.count > index ? laneRepeater.itemAt(index) : null
                        anchors.fill: parent
                        preferredRendererType: Shape.CurveRenderer
                        visible: lane !== null
                        ShapePath {
                            strokeColor: connector.agentStatus === "running" || connector.agentStatus === "pending" ? Theme.alpha(Theme.accent, 0.7)
                                       : connector.agentStatus === "completed" ? Theme.alpha(Theme.success, 0.5) : Theme.alpha(Theme.danger, 0.5)
                            strokeWidth: 1.5
                            fillColor: "transparent"
                            strokeStyle: connector.agentStatus === "pending" ? ShapePath.DashLine : ShapePath.SolidLine
                            startX: hub.width
                            startY: hub.y + hub.height / 2
                            PathCubic {
                                x: column.x
                                y: connector.lane ? connector.lane.y + connector.lane.height / 2 : 0
                                control1X: hub.width + (column.x - hub.width) * 0.55
                                control1Y: hub.y + hub.height / 2
                                control2X: hub.width + (column.x - hub.width) * 0.45
                                control2Y: connector.lane ? connector.lane.y + connector.lane.height / 2 : 0
                            }
                        }
                    }
                }

                Column {
                    id: column
                    x: screen.hubWidth + Theme.s7
                    width: parent.width - x
                    spacing: Theme.s3
                    Repeater {
                        id: laneRepeater
                        model: screen.agents
                        delegate: AgentLane {
                            width: column.width
                            selected: screen.selectedId === agentId
                            onClicked: screen.selectedId = agentId
                        }
                    }
                }
            }

            GEmptyState {
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.max(Theme.s7, flick.height / 2 - height / 2)
                visible: !screen.agents || screen.agents.count === 0
                iconName: "nodes"
                title: qsTr("No subagents running")
                body: qsTr("When the agent splits work across parallel agents (the task tool), each one appears here as a lane with its progress and output.")
                GButton {
                    visible: screen.ws !== null
                    text: qsTr("Back to the conversation")
                    variant: "ghost"
                    onClicked: Ui.go("agent")
                }
            }
        }
    }

    SubagentDrawer {
        id: drawer
        width: Math.min(460, screen.width - Theme.s6)
        height: parent.height
        x: screen.selectedId.length ? screen.width - width : screen.width
        visible: x < screen.width
        docked: screen.drawerDocked
        workspace: screen.ws
        agentId: screen.selectedId
        onCloseRequested: screen.selectedId = ""
        Behavior on x { NumberAnimation { duration: Theme.dSlow; easing.type: Theme.easeOut } }
    }
}
