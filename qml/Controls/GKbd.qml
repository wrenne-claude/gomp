import QtQuick
import Gomp
import Gomp.Core

// Key caps for a shortcut: "Ctrl+Shift+K" renders as three caps.
Row {
    id: kbd
    property string sequence
    property bool onDark: false
    spacing: 3
    visible: sequence.length > 0
    Repeater {
        model: kbd.sequence.length ? App.shortcuts.portableText(kbd.sequence).split("+").filter(k => k.length > 0) : []
        Rectangle {
            required property string modelData
            height: 18
            width: Math.max(18, label.implicitWidth + 8)
            radius: Theme.rSm
            color: Theme.surface1
            border.width: 1
            border.color: Theme.hairlineStrong
            Rectangle {
                anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 1 }
                height: 1
                color: Theme.hairline
            }
            GText {
                id: label
                anchors.centerIn: parent
                text: modelData === "Return" ? "Enter" : modelData
                role: "readout"
                font.pixelSize: 11
                color: Theme.textSecondary
            }
        }
    }
}
