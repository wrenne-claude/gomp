import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// The model's prose. No card: long answers read as a document, not as chat bubbles.
Item {
    id: msg
    required property string text
    required property var segments
    required property bool streaming
    implicitHeight: col.implicitHeight

    ColumnLayout {
        id: col
        width: parent.width
        spacing: Theme.s1
        MarkdownView {
            Layout.fillWidth: true
            segments: msg.segments
            streaming: msg.streaming
        }
        Row {
            spacing: 2
            opacity: hover.hovered && !msg.streaming ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.dFast } }
            GIconButton { size: Theme.controlSm - 2; iconName: "copy"; tooltip: qsTr("Copy as markdown"); onClicked: App.copyText(msg.text) }
        }
    }
    HoverHandler { id: hover }
}
