import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import Gomp
import Gomp.Core

// Session branches as lanes: the active path on the first lane in the accent colour, earlier
// alternatives on further lanes. Rows are user and assistant turns in the order they happened.
Column {
    id: timeline
    property var model: null
    property Workspace workspace: null
    readonly property int laneGap: 18
    readonly property int rowHeight: 44
    readonly property int textX: 20 + Math.max(1, model ? model.laneCount : 1) * laneGap
    spacing: 0

    Repeater {
        model: timeline.model
        delegate: Item {
            id: row
            required property int index
            required property string entryId
            required property string kind
            required property string text
            required property double timestamp
            required property int lane
            required property int parentRow
            required property int parentLane
            required property bool active
            required property bool fork
            required property var through
            required property bool continues
            width: timeline.width
            height: timeline.rowHeight
            readonly property real nodeX: 10 + lane * timeline.laneGap
            readonly property color laneColor: active ? Theme.accent : Theme.blueprint

            // Lanes passing through this row.
            Repeater {
                model: row.through
                Rectangle {
                    required property int modelData
                    x: 10 + modelData * timeline.laneGap - 1
                    width: 2
                    height: row.height
                    color: modelData === 0 ? Theme.alpha(Theme.accent, 0.6) : Theme.alpha(Theme.blueprint, 0.5)
                }
            }
            // Incoming line from the parent.
            Rectangle {
                visible: row.parentRow >= 0 && row.parentLane === row.lane
                x: row.nodeX - 1
                width: 2
                height: row.height / 2
                color: Theme.alpha(row.laneColor, 0.6)
            }
            Shape {
                visible: row.parentRow >= 0 && row.parentLane !== row.lane
                anchors.fill: parent
                preferredRendererType: Shape.CurveRenderer
                ShapePath {
                    strokeColor: Theme.alpha(row.laneColor, 0.6)
                    strokeWidth: 2
                    fillColor: "transparent"
                    startX: 10 + row.parentLane * timeline.laneGap
                    startY: 0
                    PathCubic {
                        x: row.nodeX; y: row.height / 2
                        control1X: 10 + row.parentLane * timeline.laneGap; control1Y: row.height * 0.35
                        control2X: row.nodeX; control2Y: row.height * 0.15
                    }
                }
            }
            // Outgoing line to the next node on this lane.
            Rectangle {
                visible: row.continues
                x: row.nodeX - 1
                y: row.height / 2
                width: 2
                height: row.height / 2
                color: Theme.alpha(row.laneColor, 0.6)
            }
            // The node.
            Rectangle {
                x: row.nodeX - width / 2
                y: row.height / 2 - height / 2
                width: row.kind === "user" ? 10 : 8
                height: width
                radius: row.kind === "compaction" ? 2 : width / 2
                color: row.kind === "user" ? row.laneColor : Theme.hull
                border.width: 2
                border.color: row.kind === "compaction" ? Theme.textTertiary : row.laneColor
            }

            RowLayout {
                x: timeline.textX
                width: parent.width - x
                height: parent.height
                spacing: Theme.s2
                GIcon {
                    name: row.kind === "user" ? "user" : row.kind === "compaction" ? "compress" : row.kind === "branch" ? "branch" : "corona"
                    size: 14
                    color: row.active ? Theme.textSecondary : Theme.textTertiary
                }
                GText {
                    Layout.fillWidth: true
                    text: row.text
                    role: row.kind === "user" ? "body" : "caption"
                    color: row.active ? (row.kind === "user" ? Theme.textPrimary : Theme.textSecondary) : Theme.textTertiary
                }
                GText { text: Fmt.relativeTime(row.timestamp); role: "caption"; tone: "tertiary" }
                GIconButton {
                    visible: timeline.workspace !== null && row.kind === "user"
                    opacity: rowHover.hovered ? 1 : 0
                    size: Theme.controlSm
                    iconName: "branch"
                    tooltip: qsTr("Continue from this message on a new branch")
                    onClicked: { timeline.workspace.branchAt(row.entryId); Ui.go("agent") }
                }
            }
            HoverHandler { id: rowHover }
        }
    }
}
