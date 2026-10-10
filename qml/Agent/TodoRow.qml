import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// One plan step. Status glyph, text (double-click to edit while reviewing), remove.
ColumnLayout {
    id: row
    required property int index
    required property string phase
    required property string content
    required property string status
    required property string details
    required property int taskIndex
    property bool editable: false
    property TodoModel todos: null
    spacing: Theme.s1

    GText {
        visible: row.taskIndex === 0 && row.phase.length > 0
        Layout.topMargin: row.index === 0 ? 0 : Theme.s2
        text: row.phase
        role: "caption"
        tone: "tertiary"
    }
    Rectangle {
        id: box
        Layout.fillWidth: true
        implicitHeight: Math.max(Theme.controlMd, textItem.implicitHeight + Theme.s2 * 2)
        radius: Theme.rMd
        color: row.status === "in_progress" ? Theme.alpha(Theme.accent, 0.10) : rowHover.hovered && row.editable ? Theme.surface2 : "transparent"
        border.width: row.status === "in_progress" ? 1 : 0
        border.color: Theme.alpha(Theme.accent, 0.4)
        Behavior on color { ColorAnimation { duration: Theme.dBase } }

        GIcon {
            id: glyph
            x: Theme.s2
            y: Theme.s2 + 1
            size: 18
            name: row.status === "completed" ? "check-circle" : row.status === "in_progress" ? "circle-half"
                : row.status === "abandoned" ? "minus-circle" : row.status === "blocked" ? "alert" : "circle"
            color: row.status === "completed" ? Theme.success : row.status === "in_progress" ? Theme.accent
                 : row.status === "blocked" ? Theme.caution : Theme.textTertiary
            TapHandler {
                enabled: row.editable
                onTapped: row.todos.setStatus(row.index, row.status === "abandoned" ? "pending" : "abandoned")
            }
        }
        GText {
            id: textItem
            anchors.left: glyph.right
            anchors.leftMargin: Theme.s2
            anchors.right: removeButton.left
            anchors.rightMargin: Theme.s1
            y: Theme.s2
            visible: !editor.visible
            text: row.content
            lineHeight: 1.25
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
            color: row.status === "abandoned" ? Theme.textTertiary : row.status === "completed" ? Theme.textSecondary : Theme.textPrimary
            font.strikeout: row.status === "abandoned"
            TapHandler {
                enabled: row.editable
                onDoubleTapped: { editor.text = row.content; editor.visible = true; editor.forceActiveFocus(); editor.selectAll() }
            }
        }
        GTextField {
            id: editor
            anchors.left: textItem.left
            anchors.right: textItem.right
            anchors.verticalCenter: parent.verticalCenter
            visible: false
            onAccepted: { row.todos.setContent(row.index, text); visible = false }
            Keys.onEscapePressed: visible = false
            onActiveFocusChanged: if (!activeFocus && visible) { row.todos.setContent(row.index, text); visible = false }
        }
        GIconButton {
            id: removeButton
            anchors.right: parent.right
            anchors.rightMargin: 2
            y: 2
            width: row.editable ? Theme.controlSm : 0
            visible: row.editable
            opacity: rowHover.hovered ? 1 : 0
            size: Theme.controlSm
            iconName: "trash"
            tooltip: qsTr("Remove step")
            onClicked: row.todos.removeAt(row.index)
        }
        HoverHandler { id: rowHover }
    }
}
