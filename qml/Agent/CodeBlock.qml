pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Highlighted code with a language label, line count and copy. Long lines scroll sideways rather
// than wrap; very tall blocks cap their height and scroll.
Rectangle {
    id: block
    property string code
    property string language
    property bool streaming: false
    property int maxHeight: 420
    radius: Theme.rMd
    color: Theme.codeBg
    border.width: 1
    border.color: Theme.hairline
    implicitHeight: header.height + Math.min(body.implicitHeight + Theme.s3 * 2, maxHeight)
    clip: true

    readonly property int lineCount: code.length ? code.split("\n").length : 0

    RowLayout {
        id: header
        width: parent.width
        height: 32
        spacing: Theme.s2
        GText {
            Layout.leftMargin: Theme.s3
            text: block.language.length ? block.language : qsTr("text")
            role: "caption"
            tone: "tertiary"
        }
        GText {
            text: Fmt.count(block.lineCount, qsTr("line"), qsTr("lines"))
            role: "caption"
            tone: "tertiary"
            visible: block.lineCount > 3
        }
        Item { Layout.fillWidth: true }
        GIconButton {
            Layout.rightMargin: 2
            size: Theme.controlSm - 2
            iconName: copied.running ? "check" : "copy"
            iconColor: copied.running ? Theme.success : hovered ? Theme.textPrimary : Theme.textTertiary
            tooltip: copied.running ? qsTr("Copied") : qsTr("Copy code")
            onClicked: { App.copyText(block.code); copied.restart() }
            Timer { id: copied; interval: 1400 }
        }
    }
    Rectangle { y: header.height; width: parent.width; height: 1; color: Theme.hairline }

    Flickable {
        id: flick
        y: header.height + 1
        width: parent.width
        height: parent.height - y
        contentWidth: Math.max(width, body.implicitWidth + Theme.s3 * 2)
        contentHeight: body.implicitHeight + Theme.s3 * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        flickableDirection: Flickable.AutoFlickIfNeeded
        interactive: contentHeight > height || contentWidth > width
        T.ScrollBar.horizontal: GScrollBar {}
        T.ScrollBar.vertical: GScrollBar {}
        TextEdit {
            id: body
            x: Theme.s3
            y: Theme.s3
            readOnly: true
            selectByMouse: true
            textFormat: TextEdit.RichText
            text: App.highlight(block.code, block.language)
            font: Theme.fCode
            color: Theme.textPrimary
            selectionColor: Theme.selection
            wrapMode: TextEdit.NoWrap
            Accessible.role: Accessible.StaticText
            Accessible.name: block.code
        }
    }
}
