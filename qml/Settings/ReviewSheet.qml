pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// The diff of every staged change, per file, before anything is written. Each file is backed up
// first; global settings are applied through `omp config set` so omp validates each value.
GSheet {
    id: sheet
    title: qsTr("Review changes")
    subtitle: qsTr("Backups go to %1 before anything is written.").arg(Fmt.shortPath(App.settings.backupDir()))
    iconName: "diff"
    preferredWidth: 820
    property var files: []
    property bool applying: false
    onOpened: { files = App.settings.preview(); applying = false }

    Connections {
        target: App.settings
        function onApplied(ok, message) {
            sheet.applying = false
            if (ok) sheet.close()
            else sheet.files = App.settings.preview()
        }
    }

    Repeater {
        model: sheet.files
        ColumnLayout {
            id: d29
            required property var modelData
            Layout.fillWidth: true
            spacing: Theme.s2
            RowLayout {
                spacing: Theme.s2
                GBadge { text: d29.modelData.scope === "project" ? qsTr("Project") : qsTr("All projects"); tone: d29.modelData.scope === "project" ? "plan" : "neutral" }
                GText { text: Fmt.shortPath(d29.modelData.path); role: "code"; Layout.fillWidth: true }
            }
            GText {
                visible: d29.modelData.hasComments
                text: qsTr("This file has comments. omp rewrites the file when saving, so comments may be removed.")
                role: "caption"
                tone: "caution"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            GText {
                visible: d29.modelData.error.length > 0
                text: d29.modelData.error
                tone: "danger"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            DiffView {
                Layout.fillWidth: true
                visible: d29.modelData.error.length === 0
                lines: d29.modelData.lines
                path: d29.modelData.path.split("/").pop()
                added: d29.modelData.added
                removed: d29.modelData.removed
            }
        }
    }
    GText {
        visible: sheet.files.length === 0
        text: qsTr("Nothing to save.")
        tone: "tertiary"
    }
    footer: [
        GButton { text: qsTr("Discard all"); variant: "ghost"; onClicked: { App.settings.discard(); sheet.close() } },
        Item { Layout.fillWidth: true },
        GButton { text: qsTr("Keep editing"); variant: "ghost"; onClicked: sheet.close() },
        GButton {
            text: qsTr("Save")
            iconName: "check"
            variant: "primary"
            loading: sheet.applying
            enabled: sheet.files.length > 0 && sheet.files.every(f => f.error.length === 0)
            onClicked: { sheet.applying = true; App.settings.apply() }
        }
    ]
}
