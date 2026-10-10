pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Gomp

// Square icon-only button with a tooltip that doubles as its accessible name.
T.AbstractButton {
    id: control
    property string iconName
    property string tooltip
    property string shortcutText
    property int size: Theme.controlMd
    property color iconColor: checked ? Theme.accent : hovered ? Theme.textPrimary : Theme.textSecondary
    property bool subtle: true

    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: tooltip
    Accessible.role: Accessible.Button

    contentItem: Item {
        GIcon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.size <= Theme.controlSm ? Theme.iconSm : Theme.iconMd
            color: control.enabled ? control.iconColor : Theme.textDisabled
        }
    }
    background: Rectangle {
        radius: Theme.rMd
        color: control.down ? Theme.surface3 : control.checked ? Theme.accentSoft : control.hovered ? Theme.surface2
              : control.subtle ? "transparent" : Theme.surface1
        border.width: control.subtle ? 0 : 1
        border.color: Theme.hairline
        Behavior on color { ColorAnimation { duration: Theme.dFast } }
        GFocusRing { target: control }
    }
    GToolTip {
        visible: control.hovered && control.tooltip.length > 0 && !control.down
        text: control.tooltip
        shortcut: control.shortcutText
    }
}
