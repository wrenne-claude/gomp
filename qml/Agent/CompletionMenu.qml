pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp

// Inline completion list above the composer (slash commands, @files). Keyboard is driven by the
// composer (Up, Down, Tab, Enter) so typing never leaves the text field.
Item {
    id: menu
    property var model: null
    property string titleRole: "name"
    property bool richTitles: false
    property string prefix
    property string emptyText
    property bool loading: false
    readonly property int count: list.count
    signal chosen(int row)
    height: Math.min(list.contentHeight, 280) + Theme.s1 * 2
    z: 20

    function move(delta) { list.currentIndex = Math.max(0, Math.min(list.count - 1, list.currentIndex + delta)) }
    function accept() { if (list.currentIndex >= 0) chosen(list.currentIndex) }
    onVisibleChanged: list.currentIndex = 0

    GFloating { anchors.fill: parent; level: 2; radius: Theme.rMd + 2 }
    ListView {
        id: list
        anchors.fill: parent
        anchors.margins: Theme.s1
        model: menu.model
        clip: true
        currentIndex: 0
        highlightMoveDuration: 0
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: GScrollBar {}
        delegate: Item {
            id: row
            required property int index
            required property var model
            width: list.width
            height: 36
            readonly property bool current: ListView.isCurrentItem
            Rectangle {
                anchors.fill: parent
                radius: Theme.rSm + 2
                color: row.current ? Theme.accentSoft : rowHover.hovered ? Theme.surface2 : "transparent"
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s2 + 2
                anchors.rightMargin: Theme.s2 + 2
                spacing: Theme.s3
                Text {
                    text: menu.prefix + (row.model[menu.titleRole] || "")
                    textFormat: menu.richTitles ? Text.StyledText : Text.PlainText
                    font: Theme.fCodeSmall
                    color: Theme.textPrimary
                    elide: Text.ElideMiddle
                    Layout.preferredWidth: menu.richTitles ? row.width - Theme.s4 : Math.min(implicitWidth, 180)
                }
                GText {
                    text: row.model.hint || ""
                    font: Theme.fCodeSmall
                    tone: "tertiary"
                    visible: text.length > 0 && !menu.richTitles
                }
                GText {
                    Layout.fillWidth: true
                    text: row.model.description || ""
                    role: "caption"
                    tone: "tertiary"
                    visible: !menu.richTitles
                }
            }
            HoverHandler { id: rowHover }
            TapHandler { onTapped: menu.chosen(row.index) }
        }
    }
    GText {
        anchors.centerIn: parent
        visible: list.count === 0
        text: menu.loading ? qsTr("Indexing files…") : menu.emptyText
        role: "caption"
        tone: "tertiary"
    }
}
