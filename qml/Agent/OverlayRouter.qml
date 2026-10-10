import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import Gomp
import Gomp.Core

// Opens the agent screen's dialogs by name (Ui.openOverlay), so menus, the palette and shortcuts
// all reach the same sheet.
Item {
    id: router
    property Workspace workspace: null

    Connections {
        target: Ui
        function onOpenOverlay(name, props) {
            if (name === "compact" && router.workspace) compactSheet.open()
            else if (name === "goal" && router.workspace) goalSheet.open()
            else if (name === "diagnostics" && router.workspace) diagnostics.open()
            else if (name === "openProject") folderDialog.open()
        }
    }
    Shortcut {
        sequences: App.shortcuts.map["session.compact"] ? [App.shortcuts.map["session.compact"]] : []
        enabled: router.workspace !== null
        onActivated: compactSheet.open()
    }
    Shortcut {
        sequences: App.shortcuts.map["session.fork"] ? [App.shortcuts.map["session.fork"]] : []
        enabled: router.workspace !== null
        onActivated: router.workspace.fork()
    }
    Shortcut {
        sequences: App.shortcuts.map["session.new"] ? [App.shortcuts.map["session.new"]] : []
        enabled: router.workspace !== null
        onActivated: router.workspace.newSession()
    }
    Shortcut {
        sequences: App.shortcuts.map["session.rename"] ? [App.shortcuts.map["session.rename"]] : []
        enabled: router.workspace !== null
        onActivated: Ui.openOverlay("rename", {})
    }
    Shortcut {
        sequences: App.shortcuts.map["approval.approve"] ? [App.shortcuts.map["approval.approve"]] : []
        enabled: router.workspace !== null && router.workspace.pendingApprovals > 0
        onActivated: router.workspace.approveAll()
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Open a project")
        onAccepted: App.openProject(selectedFolder.toString())
    }

    GSheet {
        id: compactSheet
        title: qsTr("Compact the conversation")
        subtitle: qsTr("Older turns become a summary. The transcript stays on disk; only the model's context shrinks.")
        iconName: "compress"
        onOpened: instructions.text = ""
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s3
            GText { text: qsTr("Now"); role: "caption"; tone: "tertiary" }
            GMeter {
                Layout.fillWidth: true
                value: router.workspace && router.workspace.contextWindow > 0 ? router.workspace.contextTokens / router.workspace.contextWindow : 0
            }
            GText { text: router.workspace ? Fmt.tokens(router.workspace.contextTokens) : ""; role: "readout" }
        }
        GField {
            Layout.fillWidth: true
            label: qsTr("What to keep")
            description: qsTr("Optional. Tell the summariser what matters, for example open decisions or failing tests.")
            GTextField {
                id: instructions
                width: parent.width
                placeholderText: qsTr("Keep the migration plan and the list of changed files")
                onAccepted: compactSheet.run()
            }
        }
        function run() {
            router.workspace.compact(instructions.text)
            compactSheet.close()
            Ui.inspectorTab = "context"
            Ui.inspectorOpen = true
        }
        footer: [
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: compactSheet.close() },
            GButton {
                text: qsTr("Compact")
                iconName: "compress"
                variant: "primary"
                enabled: router.workspace && !router.workspace.busy
                onClicked: compactSheet.run()
            }
        ]
    }

    GSheet {
        id: goalSheet
        title: qsTr("Start a goal")
        subtitle: qsTr("The agent keeps working, across turns, until the objective is met or you stop it.")
        iconName: "target"
        onOpened: { objective.text = router.workspace ? router.workspace.goalObjective : ""; objective.forceActiveFocus() }
        GField {
            Layout.fillWidth: true
            label: qsTr("Objective")
            description: qsTr("Make it checkable: the agent decides when it is done.")
            GTextField {
                id: objective
                width: parent.width
                placeholderText: qsTr("All rate limiter tests pass and the README documents the new limits")
                onAccepted: goalSheet.start()
            }
        }
        function start() {
            if (!objective.text.trim().length) return
            router.workspace.startGoal(objective.text)
            goalSheet.close()
        }
        footer: [
            Item { Layout.fillWidth: true },
            GButton {
                visible: router.workspace && router.workspace.mode === "goal"
                text: qsTr("Stop the current goal")
                variant: "danger"
                onClicked: { router.workspace.stopGoal(); goalSheet.close() }
            },
            GButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: goalSheet.close() },
            GButton { text: qsTr("Start goal"); iconName: "target"; variant: "primary"; enabled: objective.text.trim().length > 0; onClicked: goalSheet.start() }
        ]
    }

    GSheet {
        id: diagnostics
        title: qsTr("omp log")
        subtitle: qsTr("The last lines omp wrote to stderr for this workspace.")
        iconName: "terminal"
        preferredWidth: 760
        CodeBlock {
            Layout.fillWidth: true
            code: router.workspace ? router.workspace.diagnostics().join("\n") || qsTr("(empty)") : ""
            language: "console"
            maxHeight: 420
        }
        footer: [
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Copy"); iconName: "copy"; variant: "ghost"; onClicked: App.copyText(router.workspace.diagnostics().join("\n")) },
            GButton { text: qsTr("Close"); variant: "primary"; onClicked: diagnostics.close() }
        ]
    }
}
