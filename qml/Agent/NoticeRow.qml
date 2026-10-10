import QtQuick
import QtQuick.Layouts
import Gomp

Item {
    id: row
    required property string text
    required property string level
    implicitHeight: layout.implicitHeight
    RowLayout {
        id: layout
        width: parent.width
        spacing: Theme.s2
        GIcon {
            Layout.alignment: Qt.AlignTop
            name: row.level === "warning" ? "alert" : row.level === "error" ? "x-circle" : "info"
            size: Theme.iconSm
            color: row.level === "warning" ? Theme.caution : row.level === "error" ? Theme.danger : Theme.textTertiary
        }
        GText {
            Layout.fillWidth: true
            text: row.text
            role: "caption"
            tone: row.level === "warning" ? "caution" : "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }
}
