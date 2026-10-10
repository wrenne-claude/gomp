pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Session title (rename in place), the mode switcher, the model chip and session actions.
Rectangle {
    id: header
    property Workspace workspace: null
    implicitHeight: Theme.compact ? 48 : 56
    color: "transparent"
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.hairline }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s5
        anchors.rightMargin: Theme.s3
        spacing: Theme.s3

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 120
            spacing: 0
            Item {
                Layout.fillWidth: true
                implicitHeight: titleText.implicitHeight
                GText {
                    id: titleText
                    width: parent.width
                    visible: !titleEdit.visible
                    text: header.workspace && header.workspace.sessionTitle.length ? header.workspace.sessionTitle : qsTr("New session")
                    role: "heading"
                    color: header.workspace && header.workspace.sessionTitle.length ? Theme.textPrimary : Theme.textTertiary
                    HoverHandler { id: titleHover; cursorShape: Qt.IBeamCursor }
                    TapHandler { onDoubleTapped: header.startRename() }
                }
                GTextField {
                    id: titleEdit
                    visible: false
                    width: Math.min(parent.width, 420)
                    font: Theme.fHeading
                    onAccepted: { if (header.workspace) header.workspace.rename(text); visible = false }
                    Keys.onEscapePressed: visible = false
                    onActiveFocusChanged: if (!activeFocus) visible = false
                }
            }
            GText {
                Layout.fillWidth: true
                text: header.workspace ? Fmt.shortPath(header.workspace.cwd) : ""
                role: "caption"
                tone: "tertiary"
            }
        }

        ModeSwitcher {
            workspace: header.workspace
            Layout.alignment: Qt.AlignVCenter
        }

        ModelChip {
            workspace: header.workspace
            Layout.alignment: Qt.AlignVCenter
            visible: header.width > 760
        }

        GIconButton {
            iconName: "more"
            tooltip: qsTr("Session actions")
            onClicked: sessionMenu.popup()
            GMenu {
                id: sessionMenu
                GMenuItem { text: qsTr("Rename"); iconName: "pencil"; shortcut: App.shortcuts.map["session.rename"] || ""; onTriggered: header.startRename() }
                GMenuItem { text: qsTr("Compact conversation…"); iconName: "compress"; shortcut: App.shortcuts.map["session.compact"] || ""; onTriggered: Ui.openOverlay("compact", {}) }
                GMenuItem { text: qsTr("Fork into a new session"); iconName: "fork"; onTriggered: header.workspace.fork() }
                GMenuItem { text: qsTr("Export as HTML"); iconName: "export"; onTriggered: header.workspace.exportHtml("") }
                GMenuSeparator {}
                GMenuItem { text: qsTr("New session here"); iconName: "plus"; onTriggered: header.workspace.newSession() }
                GMenuItem { text: qsTr("Restart omp"); iconName: "refresh"; onTriggered: header.workspace.restart() }
            }
        }
        GIconButton {
            iconName: "inspector"
            tooltip: Ui.inspectorOpen ? qsTr("Hide inspector") : qsTr("Show inspector")
            shortcutText: App.shortcuts.map["inspector.toggle"] || ""
            checked: Ui.inspectorOpen
            onClicked: Ui.inspectorOpen = !Ui.inspectorOpen
        }
    }

    function startRename() {
        if (!workspace) return
        titleEdit.text = workspace.sessionTitle
        titleEdit.visible = true
        titleEdit.forceActiveFocus()
        titleEdit.selectAll()
    }
    Connections {
        target: Ui
        function onOpenOverlay(name, props) { if (name === "rename") header.startRename() }
    }
}
