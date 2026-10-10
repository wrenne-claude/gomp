pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// A subagent up close: its assignment, live output, and controls to steer or cancel it.
Item {
    id: drawer
    property Workspace workspace: null
    property string agentId
    property bool docked: true
    signal closeRequested()
    readonly property int row: workspace && agentId.length ? workspace.subagents.rowOf(agentId) : -1
    // Re-read the row whenever the model changes.
    property var info: ({})
    function refresh() { info = row >= 0 ? workspace.subagents.get(row) : ({}) }
    onRowChanged: refresh()
    Connections {
        target: drawer.workspace ? drawer.workspace.subagents : null
        function onDataChanged() { drawer.refresh() }
    }
    readonly property bool running: drawer.info.status === "running" || drawer.info.status === "pending"

    Rectangle {
        anchors.fill: parent
        visible: drawer.docked
        color: Theme.surface0
        Rectangle { width: 1; height: parent.height; color: Theme.hairline }
    }
    GFloating {
        anchors.fill: parent
        anchors.margins: Theme.s2
        visible: !drawer.docked
        level: 3
    }
    Keys.onEscapePressed: drawer.closeRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.s4
        anchors.leftMargin: Theme.s4 + 1
        spacing: Theme.s3
        visible: drawer.row >= 0

        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            GStatusLight {
                tint: drawer.info.status === "completed" ? Theme.success : drawer.info.status === "failed" || drawer.info.status === "aborted" ? Theme.danger : Theme.accent
                pulse: drawer.running
            }
            GText { text: drawer.info.agentId || ""; role: "heading"; Layout.fillWidth: true }
            GIconButton { iconName: "close"; tooltip: qsTr("Close"); shortcutText: "Esc"; onClicked: drawer.closeRequested() }
        }
        GText {
            Layout.fillWidth: true
            text: drawer.info.task || ""
            tone: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
        RowLayout {
            spacing: Theme.s2
            GBadge { text: drawer.info.agent || "" }
            GBadge { text: drawer.info.modelRole || ""; tone: "info"; visible: (drawer.info.modelRole || "").length > 0 }
            GText { text: Fmt.tokens(drawer.info.tokens || 0) + qsTr(" tokens"); role: "readout"; tone: "tertiary" }
            GText { text: Fmt.cost(drawer.info.cost || 0); role: "readout"; tone: "tertiary" }
        }
        GText { text: qsTr("Output"); role: "caption"; tone: "tertiary"; Layout.topMargin: Theme.s2 }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rMd
            color: Theme.codeBg
            border.width: 1
            border.color: Theme.hairline
            Flickable {
                id: outFlick
                anchors.fill: parent
                anchors.margins: Theme.s3
                contentHeight: outText.implicitHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                T.ScrollBar.vertical: GScrollBar {}
                // Follow the tail while the agent writes.
                onContentHeightChanged: if (drawer.running) contentY = Math.max(0, contentHeight - height)
                TextEdit {
                    id: outText
                    width: outFlick.width
                    text: (drawer.info.output || "").length ? drawer.info.output : (drawer.running ? qsTr("Waiting for output…") : qsTr("This agent wrote no output."))
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    font: Theme.fCodeSmall
                    color: (drawer.info.output || "").length ? Theme.textPrimary : Theme.textTertiary
                    selectionColor: Theme.selection
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: drawer.running
            spacing: Theme.s2
            GTextField {
                id: steerField
                Layout.fillWidth: true
                placeholderText: qsTr("Tell this agent something")
                onAccepted: { drawer.workspace.steerSubagent(drawer.agentId, text); text = "" }
            }
            GButton { text: qsTr("Steer"); iconName: "steer"; enabled: steerField.text.length > 0; onClicked: steerField.accepted() }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            GButton {
                text: qsTr("Copy output")
                iconName: "copy"
                variant: "ghost"
                onClicked: App.copyText(drawer.info.output || "")
            }
            Item { Layout.fillWidth: true }
            GButton {
                visible: drawer.running
                text: qsTr("Cancel agent")
                iconName: "stop"
                variant: "danger"
                onClicked: drawer.workspace.cancelSubagent(drawer.agentId)
            }
        }
    }
}
