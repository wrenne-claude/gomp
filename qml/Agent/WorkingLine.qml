pragma ComponentBehavior: Bound
import QtQuick
import Gomp
import Gomp.Core

// The live status line under the transcript while a turn runs: what the agent is doing, for how
// long, and the queue. The Corona carries the motion; this line carries the words.
Row {
    id: line
    property Workspace workspace: null
    spacing: Theme.s2
    Corona {
        workspace: line.workspace
        size: 18
        anchors.verticalCenter: parent.verticalCenter
    }
    GText {
        anchors.verticalCenter: parent.verticalCenter
        text: line.workspace ? Theme.activityLabel(line.workspace.activity) : ""
        role: "strong"
        color: line.workspace ? Theme.activityColor(line.workspace.activity, line.workspace.mode) : Theme.textSecondary
    }
    GText {
        id: elapsed
        anchors.verticalCenter: parent.verticalCenter
        role: "readout"
        tone: "tertiary"
        text: line.workspace ? Fmt.duration(seconds * 1000) : ""
        property real seconds: 0
        Timer {
            interval: 1000
            repeat: true
            running: line.visible && line.workspace && line.workspace.busy
            triggeredOnStart: true
            onTriggered: elapsed.seconds = line.workspace.turnSeconds
        }
    }
    GText {
        anchors.verticalCenter: parent.verticalCenter
        visible: line.workspace && line.workspace.queuedMessages.length > 0
        text: line.workspace ? qsTr("%1 queued").arg(line.workspace.queuedMessages.length) : ""
        role: "caption"
        tone: "tertiary"
    }
}
