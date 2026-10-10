import QtQuick
import Gomp

// Keyboard focus indicator: a 2px accent ring offset 2px outside its parent. `target` is the
// control whose focus it reports (often the parent's owner, e.g. a button for its background).
Rectangle {
    property Item target: parent
    property real radius_: Theme.rMd
    anchors.fill: parent
    anchors.margins: -4
    radius: radius_ + 4
    color: "transparent"
    border.width: 2
    border.color: Theme.focusRing
    visible: target !== null && (target.visualFocus === true || (target.activeFocus === true && target.focusReason === Qt.TabFocusReason))
    z: 10
}
