import QtQuick
import Gomp

// Text tabs with a sliding underline. `tabs` is [{value, label, count?}].
FocusScope {
    id: root
    property var tabs: []
    property var value
    signal activated(var value)
    implicitHeight: Theme.controlMd + 2
    implicitWidth: row.implicitWidth
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTabList
    readonly property int currentIndex: {
        for (let i = 0; i < tabs.length; ++i)
            if (tabs[i].value === value)
                return i
        return -1
    }
    Keys.onLeftPressed: if (currentIndex > 0) activated(tabs[currentIndex - 1].value)
    Keys.onRightPressed: if (currentIndex < tabs.length - 1) activated(tabs[currentIndex + 1].value)

    Row {
        id: row
        height: parent.height
        spacing: Theme.s4
        Repeater {
            id: rep
            model: root.tabs
            Item {
                id: tab
                required property var modelData
                required property int index
                readonly property bool selected: index === root.currentIndex
                height: row.height
                width: label.implicitWidth + (count.visible ? count.width + Theme.s1 + 2 : 0)
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                GText {
                    id: label
                    anchors.verticalCenter: parent.verticalCenter
                    text: tab.modelData.label
                    role: "strong"
                    color: tab.selected ? Theme.textPrimary : th.hovered ? Theme.textPrimary : Theme.textSecondary
                }
                GBadge {
                    id: count
                    anchors.left: label.right
                    anchors.leftMargin: Theme.s1 + 2
                    anchors.verticalCenter: parent.verticalCenter
                    text: tab.modelData.count !== undefined ? String(tab.modelData.count) : ""
                    visible: tab.modelData.count !== undefined && tab.modelData.count > 0
                    tone: tab.selected ? "accent" : "neutral"
                    implicitHeight: 18
                }
                HoverHandler { id: th; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { root.forceActiveFocus(); root.activated(tab.modelData.value) } }
            }
        }
    }
    Rectangle {
        readonly property Item target: rep.count > 0 && root.currentIndex >= 0 ? rep.itemAt(root.currentIndex) : null
        visible: target !== null
        y: root.height - 2
        x: target ? target.x : 0
        width: target ? target.width : 0
        height: 2
        radius: 1
        color: Theme.accent
        Behavior on x { enabled: !Theme.reduceMotion; SpringAnimation { spring: Theme.springStiffness; damping: Theme.springDamping; epsilon: 0.25 } }
        Behavior on width { enabled: !Theme.reduceMotion; SpringAnimation { spring: Theme.springStiffness; damping: Theme.springDamping; epsilon: 0.25 } }
    }
    GFocusRing { target: root; visible: root.activeFocus }
}
