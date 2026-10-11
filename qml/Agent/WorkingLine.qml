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
    // Like omp's own status line: "Working…" with its shimmer while the agent works; a plain,
    // static label when it is waiting on you or has failed.
    readonly property bool working: line.workspace !== null && line.workspace.activity !== Workspace.Approval
                                    && line.workspace.activity !== Workspace.Error
    GShimmerText {
        anchors.verticalCenter: parent.verticalCenter
        text: line.working ? qsTr("Working…") : line.workspace ? Theme.activityLabel(line.workspace.activity) : ""
        role: "strong"
        running: line.working
        highColor: line.workspace ? Theme.activityColor(line.workspace.activity, line.workspace.mode) : Theme.textSecondary
        Accessible.name: line.workspace ? Theme.activityLabel(line.workspace.activity) : ""
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
            onTriggered: {
                elapsed.seconds = line.workspace.turnSeconds
                line.quiet = line.workspace.quietSeconds()
            }
        }
    }
    // A long silence while the model should be producing tokens: say so, so a stalled provider
    // is not mistaken for gomp hanging. Tools and questions are legitimately quiet.
    property real quiet: 0
    readonly property bool stalled: quiet >= 8 && line.workspace !== null
                                    && (line.workspace.activity === Workspace.Waiting
                                        || line.workspace.activity === Workspace.Thinking
                                        || line.workspace.activity === Workspace.Streaming)
    GText {
        anchors.verticalCenter: parent.verticalCenter
        visible: line.stalled
        text: qsTr("no output from the model for %1").arg(Fmt.duration(Math.floor(line.quiet) * 1000))
        role: "caption"
        tone: "caution"
    }
    Row {
        anchors.verticalCenter: parent.verticalCenter
        readonly property string key: App.shortcuts.map["agent.stop"] || ""
        visible: line.working && key.length > 0
        spacing: Theme.s1 + 2
        leftPadding: Theme.s2
        GKbd { sequence: parent.key; anchors.verticalCenter: parent.verticalCenter }
        GText { text: qsTr("stop"); role: "caption"; tone: "tertiary"; anchors.verticalCenter: parent.verticalCenter }
    }
    GText {
        anchors.verticalCenter: parent.verticalCenter
        visible: line.workspace && line.workspace.queuedMessages.length > 0
        text: line.workspace ? qsTr("%1 queued").arg(line.workspace.queuedMessages.length) : ""
        role: "caption"
        tone: "tertiary"
    }
}
