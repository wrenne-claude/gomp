pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import Gomp
import Gomp.Core

// No workspace open: open a project, reopen a recent one, or try the demo.
Item {
    id: empty
    FolderDialog {
        id: folderDialog
        title: qsTr("Open a project")
        onAccepted: App.openProject(selectedFolder.toString())
    }
    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(520, parent.width - Theme.s6 * 2)
        spacing: Theme.s4
        Corona {
            Layout.alignment: Qt.AlignHCenter
            size: Theme.compact ? 96 : 160
            workspace: null
        }
        GText { Layout.alignment: Qt.AlignHCenter; text: qsTr("Open a project to start"); role: "title" }
        GText {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Each project runs its own omp session. Open several and switch between them from the tabs above.")
            tone: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Theme.s2
            GButton { text: qsTr("Open a project"); iconName: "folder-open"; variant: "primary"; onClicked: folderDialog.open() }
            GButton { text: qsTr("Try the demo"); iconName: "play"; variant: "ghost"; onClicked: App.openDemo() }
        }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.s4
            visible: App.prefs.recentProjects.length > 0
            spacing: Theme.s1
            GText { text: qsTr("Recent projects"); role: "caption"; tone: "tertiary" }
            Repeater {
                model: App.prefs.recentProjects.slice(0, 6)
                GListRow {
                    id: d47
                    required property string modelData
                    Layout.fillWidth: true
                    iconName: "folder"
                    title: d47.modelData.split("/").pop()
                    subtitle: Fmt.shortPath(d47.modelData)
                    dense: true
                    onClicked: App.openProject(d47.modelData)
                }
            }
        }
    }
}
