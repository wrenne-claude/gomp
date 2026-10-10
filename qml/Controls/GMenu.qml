import QtQuick
import QtQuick.Templates as T
import Gomp

// Context menu. Items are GMenuItem; separators are GMenuSeparator.
T.Menu {
    id: menu
    implicitWidth: Math.max(220, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    padding: Theme.s1
    margins: Theme.s2
    overlap: 1
    transformOrigin: T.Popup.TopLeft
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dFast; easing.type: Theme.easeOut }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.dBase; easing.type: Theme.easeOut }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.dFast * 0.75 } }
    delegate: GMenuItem {}
    contentItem: ListView {
        implicitHeight: contentHeight
        implicitWidth: 220
        model: menu.contentModel
        interactive: Window.window ? contentHeight + menu.topPadding + menu.bottomPadding > menu.Window.window.height : false
        clip: true
        currentIndex: menu.currentIndex
    }
    background: GFloating { level: 2; radius: Theme.rMd + 2 }
}
