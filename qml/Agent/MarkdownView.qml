pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// Assistant markdown: prose through Qt's CommonMark renderer (selectable), fenced code through
// CodeBlock. Segments come pre-split from C++ (ConversationModel), so streaming stays cheap.
Column {
    id: md
    property var segments: []
    property bool streaming: false
    property color textColor: Theme.textPrimary
    spacing: Theme.s3

    Repeater {
        model: md.segments
        delegate: Loader {
            id: seg
            required property var modelData
            required property int index
            width: md.width
            sourceComponent: seg.modelData.kind === "code" ? codeComp : proseComp
            Component {
                id: proseComp
                TextEdit {
                    width: seg.width
                    text: App.renderMarkdown(seg.modelData.text)
                    textFormat: TextEdit.RichText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    font: Theme.fBody
                    color: md.textColor
                    selectionColor: Theme.selection
                    selectedTextColor: Theme.textPrimary
                    onLinkActivated: link => App.openUrl(link)
                    HoverHandler { cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.IBeamCursor }
                    Accessible.role: Accessible.StaticText
                    Accessible.name: seg.modelData.text
                }
            }
            Component {
                id: codeComp
                CodeBlock {
                    width: seg.width
                    code: seg.modelData.text
                    language: seg.modelData.language
                    streaming: seg.modelData.open && md.streaming
                }
            }
        }
    }
}
