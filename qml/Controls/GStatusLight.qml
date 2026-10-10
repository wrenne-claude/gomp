import QtQuick
import Gomp

// A small state lamp. `pulse` breathes it for states that want attention.
Rectangle {
    id: light
    property color tint: Theme.textTertiary
    property bool pulse: false
    property int diameter: 8
    width: diameter
    height: diameter
    radius: diameter / 2
    color: tint
    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 2.4
        height: width
        radius: width / 2
        color: Theme.alpha(light.tint, 0.25)
        visible: light.pulse && !Theme.reduceMotion
        opacity: 0
        SequentialAnimation on opacity {
            running: light.pulse && !Theme.reduceMotion
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: 1; duration: 500; easing.type: Easing.OutQuad }
            NumberAnimation { from: 1; to: 0; duration: 700; easing.type: Easing.InQuad }
        }
    }
}
