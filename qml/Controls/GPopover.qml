import QtQuick
import QtQuick.Templates as T
import Gomp

// Non-modal floating panel anchored to a control. Closes on Escape or outside click.
T.Popup {
    id: pop
    implicitWidth: contentItem ? contentItem.implicitWidth + leftPadding + rightPadding : 0
    implicitHeight: contentItem ? contentItem.implicitHeight + topPadding + bottomPadding : 0
    padding: Theme.s2
    margins: Theme.s2
    closePolicy: T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside
    focus: true
    transformOrigin: T.Popup.Top
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dFast; easing.type: Theme.easeOut }
            NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: Theme.dBase; easing.type: Theme.easeOut }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.dFast * 0.75; easing.type: Theme.easeIn } }
    background: GFloating { level: 2 }
}
