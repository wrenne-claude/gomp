pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// A vertical edge that resizes the panel beside it. Drag to resize, double-click to reset; with
// keyboard focus, Left/Right resize in 24px steps. The hairline lights up while hovered or held.
Item {
    id: handle
    // Width of the panel this edge belongs to. The panel lies to the right of the handle, so
    // dragging left makes it wider.
    property real value: 400
    property real from: 300
    property real to: 720
    property real defaultValue: 400
    signal moved(real value)      // continuously while dragging
    signal committed(real value)  // on release, reset or key step: worth persisting

    width: 9
    activeFocusOnTab: true
    Accessible.role: Accessible.Separator
    Accessible.name: qsTr("Resize panel")

    function clamp(v) { return Math.round(Math.max(from, Math.min(to, v))) }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: drag.active || hover.hovered || handle.activeFocus ? 2 : 0
        height: parent.height
        color: Theme.accent
        opacity: drag.active ? 1 : 0.7
        Behavior on width { NumberAnimation { duration: Theme.dFast } }
    }
    HoverHandler { id: hover; cursorShape: Qt.SplitHCursor }
    property real startValue: 0
    DragHandler {
        id: drag
        target: null
        xAxis.enabled: true
        yAxis.enabled: false
        cursorShape: Qt.SplitHCursor
        onActiveChanged: {
            if (active) handle.startValue = handle.value
            else handle.committed(handle.value)
        }
        onTranslationChanged: if (active) handle.moved(handle.clamp(handle.startValue - translation.x))
    }
    TapHandler {
        onDoubleTapped: { handle.moved(handle.clamp(handle.defaultValue)); handle.committed(handle.clamp(handle.defaultValue)) }
    }
    Keys.onLeftPressed: { const v = handle.clamp(handle.value + 24); handle.moved(v); handle.committed(v) }
    Keys.onRightPressed: { const v = handle.clamp(handle.value - 24); handle.moved(v); handle.committed(v) }
}
