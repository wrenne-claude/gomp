import QtQuick
import Gomp

// Elevation-1 container: surface, hairline, inner top highlight. Docked, so no shadow.
Rectangle {
    id: card
    property bool interactive: false
    property bool selected: false
    property bool hovered: hover.hovered
    radius: Theme.rLg
    color: selected ? Theme.mix(Theme.surface1, Theme.accent, Theme.dark ? 0.06 : 0.04) : interactive && hovered ? Theme.surface2 : Theme.surface1
    border.width: 1
    border.color: selected ? Theme.alpha(Theme.accent, 0.55) : Theme.hairline
    Behavior on color { ColorAnimation { duration: Theme.dFast } }
    Rectangle {
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
        height: 1
        radius: card.radius
        color: Theme.innerHighlight
    }
    HoverHandler { id: hover; enabled: card.interactive }
}
