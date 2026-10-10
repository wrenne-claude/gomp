import QtQuick
import QtQuick.Templates as T
import Gomp

// On/off switch. The thumb travels on a spring; the track lights with the accent when on.
T.Switch {
    id: control
    implicitWidth: indicator.width + (text.length ? Theme.s2 + label.implicitWidth : 0)
    implicitHeight: 24
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.CheckBox
    Accessible.name: text

    indicator: Rectangle {
        width: 36
        height: 20
        y: (control.height - height) / 2
        radius: 10
        color: control.checked ? (control.enabled ? Theme.accent : Theme.alpha(Theme.accent, 0.4))
                               : (control.hovered ? Theme.surface3 : Theme.surface2)
        border.width: control.checked ? 0 : 1
        border.color: Theme.hairlineStrong
        Behavior on color { ColorAnimation { duration: Theme.dBase } }
        Rectangle {
            id: thumb
            width: 14
            height: 14
            radius: 7
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked ? Theme.onAccent : (control.enabled ? Theme.textSecondary : Theme.textDisabled)
            Behavior on x {
                enabled: !Theme.reduceMotion
                SpringAnimation { spring: Theme.springStiffness; damping: Theme.springDamping; epsilon: 0.25 }
            }
        }
        GFocusRing { target: control; radius_: 10 }
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
