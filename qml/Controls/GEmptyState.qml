import QtQuick
import QtQuick.Layouts
import Gomp

// Empty and error states: what this place is for and the one thing to do next.
ColumnLayout {
    id: es
    property string iconName: "circle-dashed"
    property string title
    property string body
    property string tone: "neutral" // neutral | danger
    default property alias actions: actionRow.data
    spacing: Theme.s3
    width: Math.min(parent ? parent.width - Theme.s6 : 420, 420)

    Rectangle {
        Layout.alignment: Qt.AlignHCenter
        width: 48
        height: 48
        radius: Theme.rLg
        color: es.tone === "danger" ? Theme.alpha(Theme.danger, 0.12) : Theme.surface2
        border.width: 1
        border.color: Theme.hairline
        GIcon {
            anchors.centerIn: parent
            name: es.iconName
            size: 24
            color: es.tone === "danger" ? Theme.danger : Theme.textSecondary
        }
    }
    GText {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: es.title
        role: "heading"
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
    }
    GText {
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        text: es.body
        tone: "secondary"
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        visible: text.length > 0
    }
    Row {
        id: actionRow
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Theme.s1
        spacing: Theme.s2
    }
}
