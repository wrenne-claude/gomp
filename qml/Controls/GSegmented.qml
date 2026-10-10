import QtQuick
import Gomp

// Segmented control. `options` is [{value, label, icon?, color?}]; the indicator slides on a spring
// to the selection. Arrow keys move the selection.
FocusScope {
    id: root
    property var options: []
    property var value
    property string size: "md"
    property bool showLabels: true
    signal activated(var value)

    readonly property int currentIndex: {
        for (let i = 0; i < options.length; ++i)
            if (options[i].value === value)
                return i
        return -1
    }
    implicitHeight: size === "sm" ? Theme.controlSm : Theme.controlMd
    implicitWidth: row.implicitWidth + 4
    activeFocusOnTab: true
    Accessible.role: Accessible.PageTabList

    Keys.onLeftPressed: if (currentIndex > 0) activated(options[currentIndex - 1].value)
    Keys.onRightPressed: if (currentIndex < options.length - 1) activated(options[currentIndex + 1].value)

    Rectangle {
        anchors.fill: parent
        radius: Theme.rMd
        color: Theme.surface2
        border.width: 1
        border.color: Theme.hairline
    }
    Rectangle {
        id: indicator
        readonly property Item target: repeater.count > 0 && root.currentIndex >= 0 ? repeater.itemAt(root.currentIndex) : null
        visible: target !== null
        x: target ? target.x + 2 : 0
        y: 2
        width: target ? target.width : 0
        height: root.height - 4
        radius: Theme.rMd - 2
        color: Theme.surface1
        border.width: 1
        border.color: Theme.hairlineStrong
        Behavior on x { enabled: !Theme.reduceMotion; SpringAnimation { spring: Theme.springStiffness; damping: Theme.springDamping; epsilon: 0.25 } }
        Behavior on width { enabled: !Theme.reduceMotion; SpringAnimation { spring: Theme.springStiffness; damping: Theme.springDamping; epsilon: 0.25 } }
    }
    Row {
        id: row
        x: 2
        height: parent.height
        Repeater {
            id: repeater
            model: root.options
            delegate: Item {
                id: seg
                required property var modelData
                required property int index
                readonly property bool selected: index === root.currentIndex
                height: row.height
                width: content.implicitWidth + (root.size === "sm" ? Theme.s3 : Theme.s4) * 2
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                Accessible.selected: selected
                Row {
                    id: content
                    anchors.centerIn: parent
                    spacing: Theme.s1 + 2
                    GIcon {
                        name: seg.modelData.icon || ""
                        size: Theme.iconSm
                        color: seg.selected ? (seg.modelData.color || Theme.textPrimary) : hover.hovered ? Theme.textPrimary : Theme.textTertiary
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    GText {
                        text: seg.modelData.label
                        role: root.size === "sm" ? "caption" : "strong"
                        visible: root.showLabels
                        color: seg.selected ? Theme.textPrimary : hover.hovered ? Theme.textPrimary : Theme.textSecondary
                        anchors.verticalCenter: parent.verticalCenter
                        lineHeight: 1.0
                    }
                }
                HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
                TapHandler { onTapped: { root.forceActiveFocus(); root.activated(seg.modelData.value) } }
            }
        }
    }
    GFocusRing { target: root; visible: root.activeFocus }
}
