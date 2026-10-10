import QtQuick
import QtQuick.Templates as T
import Gomp

T.CheckBox {
    id: control
    implicitWidth: indicator.width + (text.length ? Theme.s2 + label.implicitWidth : 0)
    implicitHeight: Math.max(20, label.implicitHeight)
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    indicator: Rectangle {
        width: 18
        height: 18
        y: (control.height - height) / 2
        radius: Theme.rSm
        color: control.checked ? Theme.accent : control.hovered ? Theme.surface2 : Theme.surface1
        border.width: control.checked ? 0 : 1
        border.color: Theme.hairlineStrong
        Behavior on color { ColorAnimation { duration: Theme.dFast } }
        GIcon {
            anchors.centerIn: parent
            name: control.checkState === Qt.PartiallyChecked ? "minimize" : "check"
            size: 14
            color: Theme.onAccent
            visible: control.checked || control.checkState === Qt.PartiallyChecked
        }
        GFocusRing { target: control; radius_: Theme.rSm }
    }
    contentItem: GText {
        id: label
        text: control.text
        leftPadding: control.indicator.width + Theme.s2
        verticalAlignment: Text.AlignVCenter
        color: control.enabled ? Theme.textPrimary : Theme.textDisabled
        visible: text.length > 0
    }
}
