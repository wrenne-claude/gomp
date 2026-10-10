import QtQuick
import Gomp
import Gomp.Core

// Low-alpha light in the mode's hue behind the content; its strength follows activity.
ShaderEffect {
    id: amb
    property Workspace workspace: null
    readonly property int activity: workspace ? workspace.activity : Workspace.Idle
    visible: !Theme.reduceEffects && workspace !== null
    blending: true
    fragmentShader: "qrc:/qt/qml/Gomp/shaders/ambient.frag.qsb"

    property real time: 0
    property real strength: activity === Workspace.Idle ? 0.35 : activity === Workspace.Approval ? 0.75 : 0.85
    property real aspect: width / Math.max(1, height)
    property color color: Theme.activityColor(activity, workspace ? workspace.mode : "default")
    property point origin: Qt.point(0.62, -0.08)

    Behavior on strength { NumberAnimation { duration: 900; easing.type: Easing.InOutQuad } }
    Behavior on color { ColorAnimation { duration: 900 } }
    Timer {
        interval: 100
        repeat: true
        running: amb.visible && !Theme.reduceMotion && amb.activity !== Workspace.Idle
        onTriggered: amb.time += 0.1
    }
}
