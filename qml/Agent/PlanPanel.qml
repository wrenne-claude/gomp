pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Plan review and progress. A proposal shows as an editable checklist with the full plan below;
// approving runs it, and the same list then tracks each step live. Edits made before approval are
// sent back so the agent rewrites the plan to match.
Item {
    id: panel
    property Workspace workspace: null
    readonly property bool pending: workspace !== null && workspace.planPending
    readonly property TodoModel todos: workspace ? workspace.todos : null
    readonly property var plan: workspace ? workspace.plan : ({})
    readonly property var options: plan.options || []
    property bool edited: false
    property bool showDocument: false
    Connections {
        target: panel.todos
        function onEdited() { if (panel.pending) panel.edited = true }
    }
    onPendingChanged: { edited = false; feedback.text = "" }

    function hasOption(id) { return options.some(o => o.id === id) }
    function approve(option) {
        workspace.answerPlan(option, edited ? "# " + (plan.title || "Plan") + "\n\n" + todos.toMarkdown() : "")
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: col.implicitHeight + Theme.s5
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: GScrollBar {}

        ColumnLayout {
            id: col
            x: Theme.s4
            y: Theme.s4
            width: flick.width - Theme.s4 * 2
            spacing: Theme.s3

            // ---- Proposal header ----
            ColumnLayout {
                Layout.fillWidth: true
                visible: panel.pending
                spacing: Theme.s1 + 2
                RowLayout {
                    spacing: Theme.s2
                    GStatusLight { tint: Theme.blueprint; pulse: true }
                    GText { text: qsTr("Plan ready for review"); role: "caption"; color: Theme.blueprint }
                }
                GText {
                    Layout.fillWidth: true
                    text: panel.plan.title || ""
                    role: "heading"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
                GText {
                    Layout.fillWidth: true
                    text: qsTr("Edit, reorder or remove steps before approving. Nothing has changed in your files yet.")
                    role: "caption"
                    tone: "tertiary"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
            }

            // ---- Progress header ----
            ColumnLayout {
                Layout.fillWidth: true
                visible: !panel.pending && panel.todos && panel.todos.total > 0
                spacing: Theme.s2
                RowLayout {
                    Layout.fillWidth: true
                    GText {
                        text: panel.todos ? qsTr("%1 of %2 steps done").arg(panel.todos.completed).arg(panel.todos.total) : ""
                        role: "strong"
                        Layout.fillWidth: true
                    }
                    GText {
                        text: panel.todos && panel.todos.total > 0 ? Fmt.percent(panel.todos.completed / panel.todos.total) : ""
                        role: "readout"
                        tone: "secondary"
                    }
                }
                GMeter {
                    Layout.fillWidth: true
                    value: panel.todos && panel.todos.total > 0 ? panel.todos.completed / panel.todos.total : 0
                    cautionAt: 2
                    tint: Theme.success
                }
                GText {
                    Layout.fillWidth: true
                    visible: panel.todos && panel.todos.activeTask.length > 0
                    text: panel.todos ? qsTr("Now: %1").arg(panel.todos.activeTask) : ""
                    role: "caption"
                    tone: "secondary"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
            }

            // ---- Checklist ----
            Repeater {
                model: panel.todos
                delegate: TodoRow {
                    Layout.fillWidth: true
                    editable: panel.pending
                    todos: panel.todos
                }
            }
            GButton {
                visible: panel.pending
                text: qsTr("Add a step")
                iconName: "plus"
                variant: "ghost"
                size: "sm"
                onClicked: panel.todos.insertAfter(panel.todos.count - 1, qsTr("New step"))
            }

            // ---- Full plan document ----
            ColumnLayout {
                Layout.fillWidth: true
                visible: panel.pending && (panel.plan.markdown || "").length > 0
                spacing: Theme.s2
                GButton {
                    text: panel.showDocument ? qsTr("Hide the full plan") : qsTr("Read the full plan")
                    iconName: panel.showDocument ? "chevron-up" : "file-text"
                    variant: "ghost"
                    size: "sm"
                    onClicked: panel.showDocument = !panel.showDocument
                }
                GCard {
                    Layout.fillWidth: true
                    visible: panel.showDocument
                    implicitHeight: planDoc.implicitHeight + Theme.s3 * 2
                    MarkdownView {
                        id: planDoc
                        x: Theme.s3
                        y: Theme.s3
                        width: parent.width - Theme.s3 * 2
                        segments: App.markdownSegments(panel.plan.markdown || "")
                    }
                }
            }

            // ---- Decision ----
            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.s2
                visible: panel.pending
                spacing: Theme.s2
                GField {
                    Layout.fillWidth: true
                    label: qsTr("Feedback")
                    description: qsTr("Used when you ask for changes.")
                    GTextField {
                        id: feedback
                        width: parent.width
                        placeholderText: qsTr("For example: keep the sweeper, it also expires CSRF tokens")
                    }
                }
                GButton {
                    Layout.fillWidth: true
                    text: panel.edited ? qsTr("Approve edited plan") : qsTr("Approve and run")
                    iconName: "play"
                    variant: "primary"
                    onClicked: panel.approve("Approve and execute")
                }
                GButton {
                    Layout.fillWidth: true
                    visible: panel.hasOption("Approve and compact context")
                    text: qsTr("Approve and compact context first")
                    iconName: "compress"
                    onClicked: panel.approve("Approve and compact context")
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s2
                    GButton {
                        Layout.fillWidth: true
                        text: qsTr("Ask for changes")
                        iconName: "pencil"
                        variant: "ghost"
                        onClicked: panel.workspace.rejectPlan(feedback.text.length ? feedback.text : qsTr("Revise the plan."))
                    }
                    GButton {
                        Layout.fillWidth: true
                        text: qsTr("Leave plan mode")
                        variant: "ghost"
                        onClicked: { panel.workspace.rejectPlan(""); panel.workspace.setMode("default") }
                    }
                }
            }

            // ---- Empty ----
            GEmptyState {
                Layout.fillWidth: true
                Layout.topMargin: Theme.s6
                visible: !panel.pending && (!panel.todos || panel.todos.total === 0)
                iconName: "plan"
                title: qsTr("No plan yet")
                body: panel.workspace && panel.workspace.mode === "plan"
                      ? qsTr("Describe the change. The agent explores read-only, then proposes a plan here for you to approve.")
                      : qsTr("Switch to Plan to have the agent propose an approach before it edits anything. Task lists the agent keeps appear here too.")
                GButton {
                    visible: panel.workspace && panel.workspace.mode !== "plan"
                    text: qsTr("Switch to Plan")
                    iconName: "plan"
                    onClicked: panel.workspace.setMode("plan")
                }
            }
        }
    }
}
