import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// The escape hatch: config.yml as text, validated against omp's schema as you type, saved through
// a diff review with a backup.
Item {
    id: page
    readonly property string title: qsTr("Edit config files")
    readonly property string subtitle: qsTr("For anything the pages above do not cover. Known settings are checked as you type; unknown keys are kept.")
    property string scope: "global"
    property string original: ""
    property var problems: []
    property var diffLines: []
    function load() {
        original = App.settings.rawText(scope)
        editor.text = original
        problems = []
    }
    Component.onCompleted: load()
    onScopeChanged: load()

    Timer {
        id: validate
        interval: 300
        onTriggered: page.problems = App.settings.validateRaw(editor.text)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s6
        anchors.rightMargin: Theme.s6
        anchors.bottomMargin: Theme.s4
        spacing: Theme.s3

        RowLayout {
            Layout.fillWidth: true
            GSegmented {
                size: "sm"
                value: page.scope
                options: [{ value: "global", label: qsTr("Global config.yml") }].concat(App.settings.hasProject ? [{ value: "project", label: qsTr("Project config.yml") }] : [])
                onActivated: v => page.scope = v
            }
            GText { text: Fmt.shortPath(App.settings.rawPath(page.scope)); font: Theme.fCodeSmall; tone: "tertiary"; Layout.fillWidth: true; Layout.leftMargin: Theme.s3 }
            GButton { text: qsTr("Open backups"); iconName: "folder-open"; variant: "ghost"; size: "sm"; onClicked: App.revealPath(App.settings.backupDir()) }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.rMd
            color: Theme.codeBg
            border.width: editor.activeFocus ? 2 : 1
            border.color: page.problems.length ? Theme.danger : editor.activeFocus ? Theme.accent : Theme.hairline
            Flickable {
                id: flick
                anchors.fill: parent
                anchors.margins: Theme.s3
                contentHeight: editor.implicitHeight
                contentWidth: Math.max(width, editor.implicitWidth)
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                T.ScrollBar.vertical: GScrollBar {}
                TextEdit {
                    id: editor
                    font: Theme.fCode
                    color: Theme.textPrimary
                    selectByMouse: true
                    selectionColor: Theme.selection
                    tabStopDistance: 16
                    onTextChanged: validate.restart()
                    Accessible.name: qsTr("config.yml contents")
                }
            }
        }
        Repeater {
            model: page.problems
            GText { required property string modelData; text: modelData; tone: "danger"; role: "caption"; Layout.fillWidth: true }
        }
        RowLayout {
            Layout.fillWidth: true
            GText {
                text: editor.text === page.original ? qsTr("No changes") : page.problems.length ? qsTr("Fix the problems above to save") : qsTr("Valid")
                tone: editor.text === page.original ? "tertiary" : page.problems.length ? "danger" : "success"
                role: "caption"
                Layout.fillWidth: true
            }
            GButton { text: qsTr("Revert"); variant: "ghost"; enabled: editor.text !== page.original; onClicked: page.load() }
            GButton {
                text: qsTr("Review and save")
                variant: "primary"
                iconName: "diff"
                enabled: editor.text !== page.original && page.problems.length === 0
                onClicked: { page.diffLines = App.settings.rawDiff(page.scope, editor.text); review.open() }
            }
        }
    }

    GSheet {
        id: review
        title: qsTr("Save config.yml?")
        subtitle: qsTr("A backup of the current file is kept.")
        iconName: "diff"
        preferredWidth: 820
        DiffView { Layout.fillWidth: true; lines: page.diffLines; path: "config.yml" }
        footer: [
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Keep editing"); variant: "ghost"; onClicked: review.close() },
            GButton {
                text: qsTr("Save")
                variant: "primary"
                onClicked: {
                    const err = App.settings.writeRaw(page.scope, editor.text)
                    if (err.length) Ui.toast("error", err)
                    else { Ui.toast("success", qsTr("Saved. omp reads it on the next session start.")); page.original = editor.text }
                    review.close()
                }
            }
        ]
    }
}
