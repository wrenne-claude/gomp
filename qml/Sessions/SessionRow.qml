pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

T.ItemDelegate {
    id: row
    required property int index
    required property string path
    required property string title
    required property string project
    required property double modified
    required property int messages
    required property string model
    required property string firstPrompt
    required property bool branched
    required property bool compacted
    property bool selected: false
    property bool open: false

    implicitHeight: 76
    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    Accessible.name: title

    background: Rectangle {
        radius: Theme.rMd
        color: row.selected ? Theme.accentSoft : row.hovered ? Theme.surface2 : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.dFast } }
        Rectangle {
            visible: row.selected
            width: 3
            height: parent.height - Theme.s4
            radius: 1.5
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.accent
        }
    }
    contentItem: ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s3
        anchors.rightMargin: Theme.s3
        anchors.topMargin: Theme.s2 + 2
        anchors.bottomMargin: Theme.s2 + 2
        spacing: 3
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s2
            GStatusLight { visible: row.open; tint: Theme.success; diameter: 6 }
            GText { text: row.title; role: "strong"; Layout.fillWidth: true }
            GText { text: Fmt.relativeTime(row.modified); role: "caption"; tone: "tertiary" }
        }
        GText {
            Layout.fillWidth: true
            text: row.firstPrompt
            role: "caption"
            tone: "secondary"
        }
        RowLayout {
            spacing: Theme.s2
            GBadge { text: row.project; iconName: "folder" }
            GText { text: Fmt.count(row.messages, qsTr("message"), qsTr("messages")); role: "readout"; tone: "tertiary"; font.pixelSize: 12 }
            GIcon { visible: row.branched; name: "branch"; size: 14; color: Theme.blueprint }
            GIcon { visible: row.compacted; name: "compress"; size: 14; color: Theme.textTertiary }
            GText { text: row.model.split("/").pop(); role: "caption"; tone: "tertiary"; Layout.fillWidth: true }
        }
    }
}
