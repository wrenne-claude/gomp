pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// Horizontal meter with an instrument-like eased fill. Turns caution, then danger, near the limit.
Item {
    id: meter
    property real value: 0        // 0..1
    property real cautionAt: 0.75
    property real dangerAt: 0.9
    property color tint: value >= dangerAt ? Theme.danger : value >= cautionAt ? Theme.caution : Theme.accent
    implicitWidth: 96
    implicitHeight: 4
    Accessible.role: Accessible.ProgressBar

    property real shown: Math.max(0, Math.min(1, value))
    Behavior on shown { NumberAnimation { duration: Theme.dMeter; easing.type: Theme.easeOut } }

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: Theme.surface3
    }
    Rectangle {
        width: Math.max(height, parent.width * meter.shown)
        height: parent.height
        radius: height / 2
        color: meter.tint
        visible: meter.shown > 0.001
        Behavior on color { ColorAnimation { duration: Theme.dBase } }
    }
}
