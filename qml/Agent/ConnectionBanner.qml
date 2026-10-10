import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Shown when omp is starting slowly, stopped, or failed. Says what happened and offers Restart.
Rectangle {
    id: banner
    property Workspace workspace: null
    readonly property bool problem: workspace && (workspace.connection === Workspace.Failed || workspace.connection === Workspace.Disconnected)
    visible: problem || (workspace && workspace.modeSwitching)
    implicitHeight: visible ? 44 : 0
    color: problem ? Theme.alpha(Theme.danger, 0.10) : Theme.alpha(Theme.blueprint, 0.10)
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.hairline }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s5
        anchors.rightMargin: Theme.s4
        spacing: Theme.s3
        GIcon { name: banner.problem ? "alert" : "plan"; color: banner.problem ? Theme.danger : Theme.blueprint }
        GText {
            Layout.fillWidth: true
            text: !banner.workspace ? "" : banner.problem ? banner.workspace.connectionMessage
                  : banner.workspace.mode === "plan" ? qsTr("Returning to build mode. The session continues where it left off.")
                  : qsTr("Opening this session in plan mode. omp switches to its planning transport.")
        }
        GButton {
            visible: banner.problem
            text: qsTr("Show log")
            variant: "ghost"
            size: "sm"
            onClicked: Ui.openOverlay("diagnostics", {})
        }
        GButton {
            visible: banner.problem
            text: qsTr("Restart omp")
            iconName: "refresh"
            size: "sm"
            onClicked: banner.workspace.restart()
        }
    }
}
