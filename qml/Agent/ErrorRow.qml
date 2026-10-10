pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// What failed, in omp's words, and the one useful next step.
Rectangle {
    id: row
    required property string text
    required property bool retryable
    property Workspace workspace: null
    implicitHeight: layout.implicitHeight + Theme.s3 * 2
    radius: Theme.rMd + 2
    color: Theme.alpha(Theme.danger, Theme.dark ? 0.08 : 0.06)
    border.width: 1
    border.color: Theme.alpha(Theme.danger, 0.4)
    RowLayout {
        id: layout
        x: Theme.s3
        y: Theme.s3
        width: parent.width - Theme.s3 * 2
        spacing: Theme.s3
        GIcon { Layout.alignment: Qt.AlignTop; name: "x-circle"; color: Theme.danger }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            GText { text: qsTr("The turn failed"); role: "strong" }
            GText {
                Layout.fillWidth: true
                text: row.text
                tone: "secondary"
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
        }
        GButton {
            Layout.alignment: Qt.AlignVCenter
            visible: row.retryable && row.workspace
            text: qsTr("Retry")
            iconName: "refresh"
            size: "sm"
            onClicked: row.workspace.retry()
        }
        GButton {
            Layout.alignment: Qt.AlignVCenter
            visible: row.workspace
            text: qsTr("Change model")
            variant: "ghost"
            size: "sm"
            onClicked: Ui.openOverlay("model", {})
        }
    }
}
