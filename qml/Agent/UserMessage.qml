pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// A prompt you sent: a raised card with an accent rule, so turns are easy to find while scrolling.
Item {
    id: msg
    required property string text
    required property double timestamp
    required property int imageCount
    required property int index
    property Workspace workspace: null
    implicitHeight: card.height

    GCard {
        id: card
        width: parent.width
        height: content.implicitHeight + Theme.s3 * 2
        Rectangle {
            x: 0
            width: 3
            height: parent.height - Theme.s4
            anchors.verticalCenter: parent.verticalCenter
            radius: 1.5
            color: Theme.accent
        }
        ColumnLayout {
            id: content
            x: Theme.s4
            y: Theme.s3
            width: parent.width - Theme.s4 - Theme.s3
            spacing: Theme.s1 + 2
            TextEdit {
                Layout.fillWidth: true
                text: msg.text
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                font: Theme.fBody
                color: Theme.textPrimary
                selectionColor: Theme.selection
                textFormat: TextEdit.PlainText
            }
            RowLayout {
                spacing: Theme.s2
                GBadge { visible: msg.imageCount > 0; iconName: "image"; text: Fmt.count(msg.imageCount, qsTr("image"), qsTr("images")) }
                GText { text: Fmt.relativeTime(msg.timestamp); role: "caption"; tone: "tertiary" }
                Item { Layout.fillWidth: true }
                Row {
                    spacing: 2
                    opacity: cardHover.hovered ? 1 : 0
                    Behavior on opacity { NumberAnimation { duration: Theme.dFast } }
                    GIconButton { size: Theme.controlSm - 2; iconName: "copy"; tooltip: qsTr("Copy prompt"); onClicked: App.copyText(msg.text) }
                    GIconButton {
                        size: Theme.controlSm - 2
                        iconName: "pencil"
                        tooltip: qsTr("Edit and send again")
                        onClicked: Ui.openOverlay("composerText", { text: msg.text })
                    }
                }
            }
        }
        HoverHandler { id: cardHover }
    }
}
