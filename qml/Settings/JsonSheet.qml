import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Editor for list and key/value settings, validated as JSON before it is staged.
GSheet {
    id: sheet
    property string settingId
    property string label
    property string kind // array | record
    property string text
    property string error
    title: label
    subtitle: kind === "array" ? qsTr("A JSON list, for example [\"smol\", \"default\"].") : qsTr("A JSON object of keys and values.")
    iconName: "brackets"
    onOpened: { editor.text = text; error = ""; editor.forceActiveFocus() }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 240
        radius: Theme.rMd
        color: Theme.codeBg
        border.width: editor.activeFocus ? 2 : 1
        border.color: sheet.error.length ? Theme.danger : editor.activeFocus ? Theme.accent : Theme.hairline
        Flickable {
            id: flick
            anchors.fill: parent
            anchors.margins: Theme.s3
            contentHeight: editor.implicitHeight
            clip: true
            TextEdit {
                id: editor
                width: flick.width
                font: Theme.fCode
                color: Theme.textPrimary
                selectByMouse: true
                selectionColor: Theme.selection
                wrapMode: TextEdit.Wrap
            }
        }
    }
    GText { visible: sheet.error.length > 0; text: sheet.error; tone: "danger"; Layout.fillWidth: true; wrapMode: Text.WordWrap }
    footer: [
        Item { Layout.fillWidth: true },
        GButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: sheet.close() },
        GButton {
            text: qsTr("Stage change")
            variant: "primary"
            onClicked: {
                sheet.error = App.settings.setValueText(sheet.settingId, editor.text)
                if (!sheet.error.length) sheet.close()
            }
        }
    ]
}
