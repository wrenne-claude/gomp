import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Reasoning. Live while streaming (the last few lines, fading upward); collapsed to one line
// with its duration once finished, expandable on click or Enter.
Item {
    id: block
    required property string text
    required property bool streaming
    required property double durationMs
    property bool expanded: false
    implicitHeight: col.implicitHeight

    ColumnLayout {
        id: col
        width: parent.width
        spacing: Theme.s2

        Item {
            id: headerRow
            Layout.fillWidth: true
            implicitHeight: 24
            activeFocusOnTab: true
            Accessible.role: Accessible.Button
            Accessible.name: block.expanded ? qsTr("Hide reasoning") : qsTr("Show reasoning")
            Keys.onReturnPressed: block.expanded = !block.expanded
            Keys.onSpacePressed: block.expanded = !block.expanded
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s2
                GIcon {
                    name: "chevron-right"
                    size: Theme.iconSm
                    color: Theme.textTertiary
                    rotation: block.expanded ? 90 : 0
                    anchors.verticalCenter: parent.verticalCenter
                    Behavior on rotation { NumberAnimation { duration: Theme.dBase; easing.type: Theme.easeOut } }
                }
                GIcon { name: "orbit"; size: Theme.iconSm; color: block.streaming ? Theme.accent : Theme.textTertiary; anchors.verticalCenter: parent.verticalCenter }
                GText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: block.streaming ? qsTr("Thinking") : qsTr("Reasoned for %1").arg(Fmt.duration(block.durationMs))
                    role: "strong"
                    tone: "secondary"
                }
            }
            HoverHandler { cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: block.expanded = !block.expanded }
            GFocusRing { target: headerRow; visible: headerRow.activeFocus }
        }

        // Live tail while streaming.
        Item {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.s5
            visible: block.streaming && !block.expanded
            implicitHeight: Math.min(tail.implicitHeight, 66)
            clip: true
            GText {
                id: tail
                width: parent.width
                anchors.bottom: parent.bottom
                text: block.text
                tone: "tertiary"
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
            }
            Rectangle {
                width: parent.width
                height: 28
                gradient: Gradient {
                    GradientStop { position: 0; color: Theme.hull }
                    GradientStop { position: 1; color: Theme.alpha(Theme.hull, 0) }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.s2
            visible: block.expanded
            implicitHeight: full.implicitHeight + Theme.s3 * 2
            color: "transparent"
            Rectangle { width: 2; height: parent.height; radius: 1; color: Theme.hairlineStrong }
            TextEdit {
                id: full
                x: Theme.s4
                y: Theme.s3
                width: parent.width - Theme.s4
                text: block.text
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                font: Theme.fBody
                color: Theme.textSecondary
                selectionColor: Theme.selection
            }
        }
    }
}
