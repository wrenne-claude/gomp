import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// One subagent: status, identity, its task, a track of the tools it used (the current one live),
// and its budget readouts.
T.AbstractButton {
    id: lane
    required property string agentId
    required property string agent
    required property string task
    required property string status
    required property string modelRole
    required property string currentTool
    required property var recentTools
    required property int toolCount
    required property double tokens
    required property double cost
    required property double durationMs
    property bool selected: false
    readonly property bool running: status === "running" || status === "pending"
    readonly property color statusColor: status === "completed" ? Theme.success : status === "failed" || status === "aborted" ? Theme.danger : Theme.accent

    implicitHeight: 104
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: qsTr("%1, %2").arg(agentId).arg(status)

    background: GCard {
        interactive: true
        selected: lane.selected
        GFocusRing { target: lane; radius_: Theme.rLg }
        // A thin status edge: the lane's state at a glance from across the room.
        Rectangle {
            x: 0
            width: 3
            height: parent.height - Theme.s5
            anchors.verticalCenter: parent.verticalCenter
            radius: 1.5
            color: lane.statusColor
            opacity: lane.running ? 1 : 0.6
        }
    }
    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s4
        anchors.rightMargin: Theme.s4
        anchors.topMargin: Theme.s3
        anchors.bottomMargin: Theme.s3
        spacing: Theme.s2

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            GStatusLight { tint: lane.statusColor; pulse: lane.running }
            GText { text: lane.agentId; role: "strong" }
            GBadge { text: lane.agent }
            GBadge { text: lane.modelRole; visible: lane.modelRole.length > 0; tone: "info" }
            Item { Layout.fillWidth: true }
            component Readout: Row {
                property string icon
                property string value
                spacing: 4
                GIcon { name: parent.icon; size: 14; color: Theme.textTertiary; anchors.verticalCenter: parent.verticalCenter }
                GText { text: parent.value; role: "readout"; tone: "secondary"; anchors.verticalCenter: parent.verticalCenter }
            }
            Readout { icon: "wrench"; value: String(lane.toolCount) }
            Readout { icon: "gauge"; value: Fmt.tokens(lane.tokens) }
            Readout { icon: "coin"; value: Fmt.cost(lane.cost) }
            Readout { icon: "clock"; value: Fmt.duration(lane.durationMs) }
        }
        GText {
            Layout.fillWidth: true
            text: lane.task
            tone: "secondary"
        }
        // Tool track: each tool used is a tick; the running one glows at the end.
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s1 + 2
            Repeater {
                model: lane.recentTools
                Rectangle {
                    required property string modelData
                    implicitHeight: 22
                    implicitWidth: toolLabel.implicitWidth + Theme.s3
                    radius: Theme.rSm
                    color: Theme.surface2
                    GText { id: toolLabel; anchors.centerIn: parent; text: parent.modelData; font: Theme.fCodeSmall; tone: "tertiary" }
                }
            }
            Rectangle {
                visible: lane.running && lane.currentTool.length > 0
                implicitHeight: 22
                implicitWidth: current.implicitWidth + Theme.s3 + 12
                radius: Theme.rSm
                color: Theme.accentSoft
                border.width: 1
                border.color: Theme.alpha(Theme.accent, 0.5)
                Row {
                    anchors.centerIn: parent
                    spacing: 5
                    GStatusLight { diameter: 6; tint: Theme.accent; pulse: true; anchors.verticalCenter: parent.verticalCenter }
                    GText { id: current; text: lane.currentTool; font: Theme.fCodeSmall; color: Theme.accent; anchors.verticalCenter: parent.verticalCenter }
                }
            }
            GText {
                visible: !lane.running
                text: lane.status === "completed" ? qsTr("Finished") : lane.status === "aborted" ? qsTr("Cancelled") : qsTr("Failed")
                role: "caption"
                color: lane.statusColor
            }
            Item { Layout.fillWidth: true }
        }
    }
}
