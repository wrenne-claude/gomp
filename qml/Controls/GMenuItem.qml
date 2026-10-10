pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Gomp

T.MenuItem {
    id: item
    property string iconName
    property string shortcut
    property bool destructive: false
    implicitWidth: Math.max(200, row.implicitWidth + leftPadding + rightPadding)
    implicitHeight: Theme.controlMd
    leftPadding: Theme.s2 + 2
    rightPadding: Theme.s2 + 2
    hoverEnabled: true
    contentItem: Item {
        Row {
            id: row
            spacing: Theme.s2 + 2
            anchors.verticalCenter: parent.verticalCenter
            GIcon {
                name: item.iconName
                size: Theme.iconSm
                color: item.destructive ? Theme.danger : Theme.textSecondary
                anchors.verticalCenter: parent.verticalCenter
            }
            GText {
                text: item.text
                color: !item.enabled ? Theme.textDisabled : item.destructive ? Theme.danger : Theme.textPrimary
                lineHeight: 1.0
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        GKbd {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            sequence: item.shortcut
        }
    }
    background: Rectangle {
        radius: Theme.rSm + 2
        color: item.highlighted || item.hovered ? (item.destructive ? Theme.alpha(Theme.danger, 0.12) : Theme.surface2) : "transparent"
    }
}
