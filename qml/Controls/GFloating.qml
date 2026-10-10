pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import Gomp

// Background for floating layers (popovers, menus, palette, sheets): frosted glass over the live
// window content, a shadow sized by elevation, a hairline and an inner top highlight.
// With Reduce effects the glass becomes an opaque surface and the shadow tightens.
Item {
    id: root
    property int level: 2              // 2: popover, 3: modal
    property real radius: level >= 3 ? Theme.rXl : Theme.rLg
    property Item sourceItem: null     // unused; kept for call-site symmetry
    readonly property bool frosted: !Theme.reduceEffects && Ui.backdrop !== null && visible

    RectangularShadow {
        anchors.fill: parent
        radius: root.radius
        offset.y: root.level >= 3 ? 16 : 8
        blur: Theme.reduceEffects ? 12 : (root.level >= 3 ? 48 : 24)
        spread: -4
        color: Theme.alpha(Theme.shadow, Theme.dark ? 0.55 : 0.22)
    }

    ShaderEffectSource {
        id: backdropSample
        sourceItem: root.frosted ? Ui.backdrop : null
        live: root.frosted
        visible: false
        hideSource: false
        sourceRect: Qt.rect(0, 0, 1, 1)
        function refresh() {
            if (!root.frosted || !Ui.backdrop)
                return
            const p = root.mapToItem(Ui.backdrop, 0, 0)
            sourceRect = Qt.rect(p.x, p.y, root.width, root.height)
        }
    }
    // Popups animate in and can be repositioned; re-sample their position while visible.
    Timer {
        interval: 50
        repeat: true
        running: root.frosted
        triggeredOnStart: true
        onTriggered: backdropSample.refresh()
    }
    onWidthChanged: backdropSample.refresh()
    onHeightChanged: backdropSample.refresh()

    Item {
        id: mask
        anchors.fill: parent
        layer.enabled: root.frosted
        visible: false
        Rectangle { anchors.fill: parent; radius: root.radius; color: "black" }
    }
    MultiEffect {
        anchors.fill: parent
        visible: root.frosted
        source: backdropSample
        blurEnabled: true
        blur: root.level >= 3 ? Theme.blurModal : Theme.blurPopover
        blurMax: 48
        saturation: -0.15
        maskEnabled: true
        maskSource: mask
        autoPaddingEnabled: false
    }
    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: Theme.alpha(Theme.dark ? Theme.surface1 : Theme.surface1,
                           root.frosted ? (root.level >= 3 ? Theme.surfaceTintModal : Theme.surfaceTintPopover) : 1)
        border.width: 1
        border.color: Theme.dark ? Theme.hairlineStrong : Theme.hairline
        Rectangle {
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
            height: 1
            radius: root.radius
            color: Theme.innerHighlight
        }
    }
}
