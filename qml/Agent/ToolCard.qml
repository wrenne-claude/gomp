import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// One tool call: what ran, on what, how it went and how long it took. Expands to arguments,
// output and the diff. A call waiting for approval opens itself, shows the proposed change and
// asks; that is the only state that lights the card in sodium.
Item {
    id: card
    required property string toolName
    required property string toolTitle
    required property string toolDetail
    required property string toolStatus
    required property string toolArgs
    required property string toolOutput
    required property var diffLines
    required property int diffAdded
    required property int diffRemoved
    required property string diffPath
    required property string approvalId
    required property double durationMs
    required property int subagentCount
    property Workspace workspace: null

    readonly property bool awaiting: toolStatus === "awaiting"
    readonly property bool running: toolStatus === "running"
    readonly property bool failed: toolStatus === "error" || toolStatus === "denied"
    readonly property bool hasDiff: diffLines.length > 0
    property bool userExpanded: false
    readonly property bool expanded: userExpanded || awaiting
    property string tab: hasDiff ? "diff" : "output"

    implicitHeight: frame.height

    function iconFor(name) {
        switch (name) {
        case "bash": case "shell": return "terminal"
        case "read": return "file-text"
        case "write": return "file-plus"
        case "edit": case "ast_edit": return "pencil"
        case "grep": case "glob": case "find": case "ast_grep": return "search"
        case "web_search": case "fetch": case "browser": return "globe"
        case "task": return "nodes"
        case "todo": case "todo_write": return "list"
        default: return "wrench"
        }
    }
    readonly property color statusColor: awaiting ? Theme.sodium : failed ? Theme.danger : running ? Theme.accent : Theme.success

    Rectangle {
        id: frame
        width: parent.width
        height: col.implicitHeight
        radius: Theme.rMd + 2
        color: card.awaiting ? Theme.mix(Theme.surface1, Theme.sodium, Theme.dark ? 0.05 : 0.04) : Theme.surface1
        border.width: 1
        border.color: card.awaiting ? Theme.alpha(Theme.sodium, 0.6) : Theme.hairline
        Behavior on border.color { ColorAnimation { duration: Theme.dBase } }

        ColumnLayout {
            id: col
            width: parent.width
            spacing: 0

            // Header row
            Item {
                id: head
                Layout.fillWidth: true
                implicitHeight: 40
                activeFocusOnTab: true
                Accessible.role: Accessible.Button
                Accessible.name: qsTr("%1 %2, %3").arg(card.toolName).arg(card.toolTitle).arg(card.toolStatus)
                Keys.onReturnPressed: card.userExpanded = !card.userExpanded
                Keys.onSpacePressed: card.userExpanded = !card.userExpanded
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s3
                    anchors.rightMargin: Theme.s2
                    spacing: Theme.s2 + 2
                    Item {
                        implicitWidth: Theme.iconMd
                        implicitHeight: Theme.iconMd
                        GIcon { anchors.centerIn: parent; name: card.iconFor(card.toolName); size: Theme.iconSm + 2; color: card.awaiting ? Theme.sodium : Theme.textSecondary }
                    }
                    GText { text: card.toolName; role: "strong"; color: Theme.textPrimary }
                    GText {
                        Layout.fillWidth: true
                        text: card.toolTitle
                        font: Theme.fCodeSmall
                        color: Theme.textSecondary
                        elide: Text.ElideMiddle
                    }
                    GBadge { visible: card.subagentCount > 0; iconName: "nodes"; text: Fmt.count(card.subagentCount, qsTr("agent"), qsTr("agents")); tone: "accent" }
                    GText {
                        visible: card.hasDiff
                        text: "+" + card.diffAdded
                        role: "readout"
                        color: Theme.diffAddText
                    }
                    GText {
                        visible: card.hasDiff && card.diffRemoved > 0
                        text: "−" + card.diffRemoved
                        role: "readout"
                        color: Theme.diffRemoveText
                    }
                    GText {
                        visible: card.durationMs > 0 && !card.running
                        text: Fmt.duration(card.durationMs)
                        role: "readout"
                        tone: "tertiary"
                    }
                    GStatusLight {
                        tint: card.statusColor
                        pulse: card.running || card.awaiting
                        diameter: 7
                    }
                    GIcon {
                        name: "chevron-down"
                        size: Theme.iconSm
                        color: Theme.textTertiary
                        rotation: card.expanded ? 180 : 0
                        Behavior on rotation { NumberAnimation { duration: Theme.dBase; easing.type: Theme.easeOut } }
                    }
                }
                HoverHandler { cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: card.userExpanded = !card.userExpanded }
                GFocusRing { target: head; visible: head.activeFocus; radius_: Theme.rMd }
            }
            GText {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s3 + Theme.iconMd + Theme.s2 + 2
                Layout.rightMargin: Theme.s3
                Layout.topMargin: -Theme.s2
                Layout.bottomMargin: Theme.s2
                visible: card.toolDetail.length > 0
                text: card.toolDetail
                role: "caption"
                tone: "tertiary"
            }

            // Approval bar
            Rectangle {
                Layout.fillWidth: true
                visible: card.awaiting
                implicitHeight: 48
                color: Theme.alpha(Theme.sodium, Theme.dark ? 0.08 : 0.07)
                Rectangle { width: parent.width; height: 1; color: Theme.alpha(Theme.sodium, 0.3) }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s3
                    anchors.rightMargin: Theme.s3
                    spacing: Theme.s2
                    GIcon { name: "shield"; size: Theme.iconSm + 2; color: Theme.sodium }
                    GText {
                        Layout.fillWidth: true
                        text: card.hasDiff ? qsTr("Apply this change to %1?").arg(card.diffPath || qsTr("the file"))
                                           : qsTr("Allow %1 to run?").arg(card.toolName)
                        role: "strong"
                    }
                    GButton {
                        text: card.hasDiff ? qsTr("Reject") : qsTr("Deny")
                        variant: "ghost"
                        size: "sm"
                        onClicked: card.workspace.approve(card.approvalId, false)
                    }
                    GButton {
                        text: card.hasDiff ? qsTr("Apply") : qsTr("Allow")
                        variant: "primary"
                        size: "sm"
                        iconName: "check"
                        onClicked: card.workspace.approve(card.approvalId, true)
                        GToolTip { visible: parent.hovered; text: qsTr("Approve"); shortcut: App.shortcuts.map["approval.approve"] || "" }
                    }
                }
            }

            // Expanded body: tabs over diff, output and arguments.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Theme.s3
                Layout.topMargin: Theme.s2
                visible: card.expanded
                spacing: Theme.s2
                GTabs {
                    Layout.fillWidth: true
                    value: card.tab
                    tabs: (card.hasDiff ? [{ value: "diff", label: qsTr("Change") }] : [])
                          .concat(card.toolOutput.length ? [{ value: "output", label: card.running ? qsTr("Output so far") : qsTr("Output") }] : [])
                          .concat(card.toolArgs.length > 4 ? [{ value: "args", label: qsTr("Arguments") }] : [])
                    onActivated: v => card.tab = v
                }
                DiffView {
                    Layout.fillWidth: true
                    visible: card.tab === "diff" && card.hasDiff
                    lines: card.diffLines
                    path: card.diffPath
                    added: card.diffAdded
                    removed: card.diffRemoved
                }
                CodeBlock {
                    Layout.fillWidth: true
                    visible: card.tab === "output" && card.toolOutput.length > 0
                    code: card.toolOutput
                    language: card.toolName === "bash" ? "console" : ""
                    maxHeight: 320
                }
                CodeBlock {
                    Layout.fillWidth: true
                    visible: card.tab === "args"
                    code: card.toolArgs
                    language: "json"
                    maxHeight: 280
                }
                GText {
                    visible: card.running && card.toolOutput.length === 0 && !card.hasDiff
                    text: qsTr("No output yet.")
                    tone: "tertiary"
                    role: "caption"
                }
            }
        }
    }
}
