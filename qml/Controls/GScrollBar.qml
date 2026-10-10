pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Gomp

T.ScrollBar {
    id: control
    implicitWidth: 10
    implicitHeight: 10
    padding: 2
    minimumSize: 0.06
    visible: control.policy !== T.ScrollBar.AlwaysOff && size < 1.0
    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: control.pressed ? Theme.textTertiary : Theme.hairlineStrong
        opacity: control.active || control.hovered ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.dBase } }
    }
}
