import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// A fresh session: the Corona at rest, where it is pointed, and three ways to start.
ColumnLayout {
    id: empty
    property Workspace workspace: null
    width: Math.min(560, parent ? parent.width - Theme.s6 * 2 : 560)
    spacing: Theme.s4

    Corona {
        Layout.alignment: Qt.AlignHCenter
        size: Theme.compact ? 96 : 140
        workspace: empty.workspace
    }
    GText {
        Layout.alignment: Qt.AlignHCenter
        text: empty.workspace ? qsTr("Ready in %1").arg(empty.workspace.name) : ""
        role: "title"
    }
    GText {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: empty.workspace && empty.workspace.modelName
              ? qsTr("%1 on %2. Describe a change, ask about the code, or switch to Plan to review an approach first.").arg(empty.workspace.modelName).arg(empty.workspace.git.isRepository ? empty.workspace.git.branch : Fmt.shortPath(empty.workspace.cwd))
              : qsTr("Connecting to omp…")
        tone: "secondary"
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
    }
    Flow {
        Layout.fillWidth: true
        Layout.topMargin: Theme.s2
        spacing: Theme.s2
        visible: empty.workspace && empty.workspace.connection === Workspace.Ready
        Repeater {
            model: [
                { text: qsTr("Explain how session expiry works"), icon: "info" },
                { text: qsTr("Fix the failing rate limiter test"), icon: "wrench" },
                { text: qsTr("Plan a move from in-memory sessions to Redis"), icon: "plan" },
                { text: qsTr("Audit the API for security issues in parallel"), icon: "nodes" }
            ]
            GButton {
                required property var modelData
                visible: empty.workspace && empty.workspace.mock
                text: modelData.text
                iconName: modelData.icon
                size: "sm"
                onClicked: empty.workspace.send(modelData.text)
            }
        }
    }
}
