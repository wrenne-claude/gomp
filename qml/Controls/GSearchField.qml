pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// Search input with a leading glyph, a clear button, and Escape to clear.
GTextField {
    id: field
    iconName: "search"
    rightPadding: text.length ? Theme.s6 : Theme.s3
    Keys.onEscapePressed: event => {
        if (text.length) {
            clear()
            event.accepted = true
        } else {
            event.accepted = false
        }
    }
    GIconButton {
        anchors.right: parent.right
        anchors.rightMargin: 2
        anchors.verticalCenter: parent.verticalCenter
        size: Theme.controlSm - 2
        iconName: "close"
        tooltip: qsTr("Clear")
        visible: field.text.length > 0
        focusPolicy: Qt.NoFocus
        onClicked: field.clear()
    }
}
