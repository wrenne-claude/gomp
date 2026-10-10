import QtQuick
import QtQuick.Templates as T
import Gomp

T.TextField {
    id: control
    property string iconName
    property bool invalid: false
    property bool mono: false

    implicitWidth: 240
    implicitHeight: Theme.controlMd
    leftPadding: iconName.length ? Theme.s6 : Theme.s3
    rightPadding: Theme.s3
    verticalAlignment: TextInput.AlignVCenter
    font: mono ? Theme.fCode : Theme.fBody
    color: enabled ? Theme.textPrimary : Theme.textDisabled
    placeholderTextColor: Theme.textTertiary
    selectionColor: Theme.selection
    selectedTextColor: Theme.textPrimary
    selectByMouse: true
    hoverEnabled: true
    Accessible.role: Accessible.EditableText
    Accessible.name: placeholderText

    GText {
        x: control.leftPadding
        width: control.width - control.leftPadding - control.rightPadding
        height: control.height
        verticalAlignment: Text.AlignVCenter
        text: control.placeholderText
        color: control.placeholderTextColor
        font: control.font
        visible: !control.length && !control.preeditText
    }
    background: Rectangle {
        radius: Theme.rMd
        color: control.enabled ? Theme.surface1 : Theme.surface2
        border.width: control.activeFocus ? 2 : 1
        border.color: control.invalid ? Theme.danger : control.activeFocus ? Theme.accent
                    : control.hovered ? Theme.hairlineStrong : Theme.hairline
        Behavior on border.color { ColorAnimation { duration: Theme.dFast } }
        GIcon {
            x: Theme.s2 + 2
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: Theme.iconSm
            color: control.activeFocus ? Theme.textSecondary : Theme.textTertiary
        }
    }
}
