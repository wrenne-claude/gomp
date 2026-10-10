import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// One session: what it was about, where, when, with which model, its branches, and actions.
Item {
    id: detail
    property var session: ({})
    property bool showBack: false
    signal back()
    signal resumeRequested()
    signal deleted()
    readonly property bool has: session && session.path !== undefined && session.path.length > 0
    readonly property int openIndex: has ? App.workspaces.indexOfSession(session.path) : -1

    SessionTreeModel {
        id: tree
        path: detail.has ? detail.session.path : ""
    }

    GEmptyState {
        anchors.centerIn: parent
        visible: !detail.has
        iconName: "history"
        title: qsTr("Select a session")
        body: qsTr("Resume it, fork it, export it, or follow its branches.")
    }

    Flickable {
        id: flick
        anchors.fill: parent
        visible: detail.has
        contentHeight: col.implicitHeight + Theme.s6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: GScrollBar {}

        ColumnLayout {
            id: col
            x: Theme.s6
            y: Theme.s5
            width: Math.min(flick.width - Theme.s6 * 2, 860)
            spacing: Theme.s4

            GButton { visible: detail.showBack; text: qsTr("All sessions"); iconName: "chevron-left"; variant: "ghost"; size: "sm"; onClicked: detail.back() }

            // Title with inline rename.
            Item {
                Layout.fillWidth: true
                implicitHeight: Math.max(titleText.implicitHeight, titleEdit.implicitHeight)
                GText {
                    id: titleText
                    width: parent.width
                    visible: !titleEdit.visible
                    text: detail.has ? detail.session.title : ""
                    role: "display"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
                GTextField {
                    id: titleEdit
                    width: parent.width
                    visible: false
                    font: Theme.fTitle
                    implicitHeight: Theme.controlLg
                    onAccepted: { App.sessions.rename(detail.session.path, text); visible = false }
                    Keys.onEscapePressed: visible = false
                }
            }

            RowLayout {
                spacing: Theme.s2
                GButton {
                    text: detail.openIndex >= 0 ? qsTr("Switch to it") : qsTr("Resume")
                    iconName: "play"
                    variant: "primary"
                    onClicked: detail.resumeRequested()
                }
                GButton { text: qsTr("Rename"); iconName: "pencil"; onClicked: { titleEdit.text = detail.session.title; titleEdit.visible = true; titleEdit.forceActiveFocus(); titleEdit.selectAll() } }
                GButton { text: qsTr("Export HTML"); iconName: "export"; onClicked: App.sessions.exportHtml(detail.session.path) }
                GButton { text: qsTr("Show file"); iconName: "folder-open"; variant: "ghost"; onClicked: App.revealPath(detail.session.path) }
                Item { Layout.fillWidth: true }
                GButton { text: qsTr("Delete"); iconName: "trash"; variant: "danger"; onClicked: confirmDelete.open() }
            }

            // Facts
            GridLayout {
                Layout.fillWidth: true
                columns: width > 560 ? 4 : 2
                columnSpacing: Theme.s5
                rowSpacing: Theme.s3
                component Fact: ColumnLayout {
                    property string label
                    property string value
                    property bool mono: false
                    spacing: 2
                    Layout.fillWidth: true
                    GText { text: parent.label; role: "caption"; tone: "tertiary" }
                    GText { text: parent.value; Layout.fillWidth: true; font.features: parent.mono ? { "tnum": 1, "liga": 0 } : { "liga": 0 } }
                }
                Fact { label: qsTr("Project"); value: detail.has ? Fmt.shortPath(detail.session.cwd) : "" }
                Fact { label: qsTr("Last active"); value: detail.has ? Fmt.relativeTime(detail.session.modified) : "" }
                Fact { label: qsTr("Messages"); value: detail.has ? String(detail.session.messages) : ""; mono: true }
                Fact { label: qsTr("Model"); value: detail.has ? detail.session.model.split("/").pop() : "" }
                Fact { label: qsTr("Started"); value: detail.has ? Fmt.relativeTime(detail.session.created) : "" }
                Fact { label: qsTr("Branches"); value: String(Math.max(1, tree.laneCount)); mono: true }
                Fact { label: qsTr("Size"); value: detail.has ? Fmt.bytes(detail.session.size) : ""; mono: true }
                Fact { label: qsTr("Context"); value: detail.has && detail.session.compacted ? qsTr("Compacted") : qsTr("Full history") }
            }

            GDivider { Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                GText { text: qsTr("Timeline"); role: "heading"; Layout.fillWidth: true }
                GText {
                    visible: tree.branchCount > 0
                    text: Fmt.count(tree.branchCount, qsTr("side branch"), qsTr("side branches"))
                    role: "caption"
                    color: Theme.blueprint
                }
            }
            GText {
                Layout.fillWidth: true
                visible: tree.branchCount > 0
                text: qsTr("The highlighted line is the branch the session continues on. Other lines are earlier alternatives.")
                role: "caption"
                tone: "tertiary"
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
            BranchTimeline {
                Layout.fillWidth: true
                model: tree
                workspace: detail.openIndex >= 0 ? App.workspaces.at(detail.openIndex) : null
            }
        }
    }

    GSheet {
        id: confirmDelete
        title: qsTr("Delete this session?")
        subtitle: qsTr("“%1” moves to the trash. You can restore it from there.").arg(detail.has ? detail.session.title : "")
        iconName: "trash"
        preferredWidth: 460
        footer: [
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Keep it"); variant: "ghost"; onClicked: confirmDelete.close() },
            GButton {
                text: qsTr("Move to trash")
                variant: "danger"
                onClicked: {
                    if (App.sessions.remove(detail.session.path)) detail.deleted()
                    confirmDelete.close()
                }
            }
        ]
    }
}
