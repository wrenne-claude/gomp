pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Gomp

T.Slider {
    id: control
    implicitWidth: 200
    implicitHeight: 24
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.Slider

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: Theme.surface3
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: control.enabled ? Theme.accent : Theme.textDisabled
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 16
        height: 16
        radius: 8
        color: control.pressed ? Theme.accentPressed : Theme.surface1
        border.width: 2
        border.color: control.enabled ? Theme.accent : Theme.textDisabled
        scale: control.pressed ? 1.15 : control.hovered ? 1.06 : 1
        Behavior on scale { NumberAnimation { duration: Theme.dFast; easing.type: Theme.easeOut } }
        GFocusRing { target: control; radius_: 8 }
    }
}
