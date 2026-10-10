import QtQuick
import Gomp
import Gomp.Core

// The Corona: gomp's activity visualisation (docs/design.md). Every input comes from the
// workspace's real state. Idle repaints at 20 fps for the slow breathing; activity runs at the
// display rate. Reduce effects swaps the shader for the static mark, tinted by state.
Item {
    id: corona
    property Workspace workspace: null
    property int size: 20
    readonly property int activity: workspace ? workspace.activity : Workspace.Idle
    readonly property string mode: workspace ? workspace.mode : "default"
    readonly property bool active: activity !== Workspace.Idle
    implicitWidth: size
    implicitHeight: size
    Accessible.role: Accessible.Indicator
    Accessible.name: qsTr("Agent status: %1").arg(Theme.activityLabel(activity))

    property real t: 0
    FrameAnimation {
        running: corona.visible && corona.active && !Theme.reduceMotion && !Theme.reduceEffects
        onTriggered: corona.t += frameTime
    }
    Timer {
        interval: 50
        repeat: true
        running: corona.visible && !corona.active && !Theme.reduceMotion && !Theme.reduceEffects
        onTriggered: corona.t += 0.05
    }

    ShaderEffect {
        id: fx
        anchors.fill: parent
        visible: !Theme.reduceEffects
        blending: true
        fragmentShader: "qrc:/qt/qml/Gomp/shaders/corona.frag.qsb"

        property real time: corona.t
        property real intensity: corona.activity === Workspace.Idle ? 0.35
                                : corona.activity === Workspace.Approval ? 0.55
                                : corona.activity === Workspace.Waiting ? 0.6 : 1.0
        property real thinking: corona.activity === Workspace.Thinking ? 1 : corona.activity === Workspace.Waiting ? 0.45 : 0
        property real streaming: corona.activity === Workspace.Streaming ? 1 : 0
        property real tools: corona.activity === Workspace.Tools ? 1 : 0
        property real approval: corona.activity === Workspace.Approval ? 1 : 0
        property real errorFlash: corona.activity === Workspace.Error ? 1 : 0
        property real rate: corona.workspace ? Math.min(1.2, 0.1 + corona.workspace.tokensPerSecond / 110) : 0.1
        property real segments: corona.workspace ? Math.max(1, corona.workspace.runningTools) : 1
        property real satellites: corona.workspace && corona.workspace.subagents ? Math.min(8, corona.workspace.subagents.running) : 0
        property real pixelSize: 1.0 / Math.max(1, corona.width)
        property color color: Theme.activityColor(corona.activity, corona.mode)
        property color dimColor: Theme.textTertiary

        Behavior on intensity { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
        Behavior on thinking { NumberAnimation { duration: 400; easing.type: Easing.InOutQuad } }
        Behavior on streaming { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
        Behavior on tools { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }
        Behavior on approval { NumberAnimation { duration: 250 } }
        Behavior on errorFlash { NumberAnimation { duration: 200 } }
        Behavior on rate { NumberAnimation { duration: 600 } }
        Behavior on color { ColorAnimation { duration: 300 } }
    }
    GIcon {
        anchors.centerIn: parent
        visible: Theme.reduceEffects
        name: "corona"
        size: corona.size
        color: corona.active ? Theme.activityColor(corona.activity, corona.mode) : Theme.textTertiary
    }
}
