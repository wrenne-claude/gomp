import QtQuick
import QtQuick.Templates as T
import Gomp

// Tooltip: frosted chip with optional shortcut caps. Appears after a short delay.
T.ToolTip {
    id: tip
    property string shortcut
    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    delay: 450
    timeout: -1
    x: parent ? (parent.width - implicitWidth) / 2 : 0
    y: parent ? parent.height + Theme.s1 + 2 : 0
    margins: Theme.s2
    padding: Theme.s2
    topPadding: Theme.s1 + 2
    bottomPadding: Theme.s1 + 2
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutsideParent | T.Popup.CloseOnReleaseOutsideParent

    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dFast; easing.type: Theme.easeOut } }
    exit: Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.dFast * 0.75; easing.type: Theme.easeIn } }

    contentItem: Row {
        spacing: Theme.s2
        GText {
            text: tip.text
            role: "caption"
            color: Theme.textPrimary
            anchors.verticalCenter: parent.verticalCenter
        }
        GKbd {
            sequence: tip.shortcut
            visible: tip.shortcut.length > 0
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    background: Rectangle {
        radius: Theme.rSm + 2
        color: Theme.dark ? Theme.surface3 : Theme.textPrimary
        border.width: Theme.dark ? 1 : 0
        border.color: Theme.hairlineStrong
    }
}
