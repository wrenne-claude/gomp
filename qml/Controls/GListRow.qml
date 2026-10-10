pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Gomp

// List row: optional icon, title and subtitle, trailing slot, hover/selected/focus states.
T.ItemDelegate {
    id: row
    property string iconName
    property color iconColor: selected ? Theme.accent : Theme.textSecondary
    property string title
    property string subtitle
    property bool selected: false
    property bool dense: false
    default property alias trailing: trailingRow.data

    implicitHeight: subtitle.length ? (dense ? 44 : 52) : (dense ? Theme.controlMd : 36)
    leftPadding: Theme.s3
    rightPadding: Theme.s3
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: title
    Accessible.selected: selected

    contentItem: Item {
        GIcon {
            id: icon
            name: row.iconName
            size: Theme.iconSm + 2
            color: row.iconColor
            anchors.verticalCenter: parent.verticalCenter
        }
        Column {
            anchors.left: icon.visible ? icon.right : parent.left
            anchors.leftMargin: icon.visible ? Theme.s3 : 0
            anchors.right: trailingRow.left
            anchors.rightMargin: Theme.s2
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            GText {
                width: parent.width
                text: row.title
                role: row.selected ? "strong" : "body"
                lineHeight: 1.0
                color: row.enabled ? Theme.textPrimary : Theme.textDisabled
            }
            GText {
                width: parent.width
                text: row.subtitle
                role: "caption"
                tone: "tertiary"
                visible: text.length > 0
            }
        }
        Row {
            id: trailingRow
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.s2
        }
    }
    background: Rectangle {
        radius: Theme.rMd
        color: row.selected ? Theme.accentSoft : row.down ? Theme.surface3 : row.hovered ? Theme.surface2 : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.dFast } }
        Rectangle {
            visible: row.selected
            width: 3
            height: parent.height - Theme.s3
            radius: 1.5
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.accent
        }
        GFocusRing { target: row }
    }
}
