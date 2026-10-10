pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp

// Modal dialog. Focus is trapped inside, Escape closes, the backdrop is scrimmed. The title row,
// body (default content) and footer (`footer` property) follow one layout everywhere.
T.Popup {
    id: sheet
    property string title
    property string subtitle
    property string iconName
    property alias footer: footerRow.data
    property int preferredWidth: 560
    default property alias body: bodyColumn.data
    property bool dismissable: true

    parent: T.Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(preferredWidth, parent ? parent.width - Theme.s6 * 2 : preferredWidth)
    height: Math.min(implicitHeight, parent ? parent.height - Theme.s6 * 2 : implicitHeight)
    implicitHeight: contentItem.implicitHeight
    modal: true
    focus: true
    padding: 0
    closePolicy: dismissable ? (T.Popup.CloseOnEscape | T.Popup.CloseOnPressOutside) : T.Popup.NoAutoClose

    T.Overlay.modal: Rectangle {
        color: Theme.scrim
        Behavior on opacity { NumberAnimation { duration: Theme.dBase } }
    }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dBase; easing.type: Theme.easeOut }
            NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.dSlow; easing.type: Theme.easeOut }
        }
    }
    exit: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; to: 0; duration: Theme.dFast; easing.type: Theme.easeIn }
            NumberAnimation { property: "scale"; to: 0.98; duration: Theme.dFast; easing.type: Theme.easeIn }
        }
    }
    background: GFloating { level: 3 }

    contentItem: ColumnLayout {
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.s5
            Layout.bottomMargin: Theme.s3
            spacing: Theme.s3
            visible: sheet.title.length > 0
            GIcon {
                name: sheet.iconName
                size: Theme.iconMd
                color: Theme.accent
                Layout.alignment: Qt.AlignTop
                Layout.topMargin: 2
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                GText { text: sheet.title; role: "heading"; Layout.fillWidth: true }
                GText {
                    text: sheet.subtitle
                    tone: "secondary"
                    visible: text.length > 0
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                    Layout.fillWidth: true
                }
            }
            GIconButton {
                iconName: "close"
                tooltip: qsTr("Close")
                shortcutText: "Esc"
                visible: sheet.dismissable
                Layout.alignment: Qt.AlignTop
                onClicked: sheet.close()
            }
        }
        // The body scrolls when the sheet is taller than the window.
        Flickable {
            id: bodyFlick
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.preferredHeight: bodyColumn.implicitHeight
            contentHeight: bodyColumn.implicitHeight
            clip: contentHeight > height
            interactive: contentHeight > height
            boundsBehavior: Flickable.StopAtBounds
            T.ScrollBar.vertical: GScrollBar {}
            ColumnLayout {
                id: bodyColumn
                x: Theme.s5
                width: bodyFlick.width - Theme.s5 * 2
                spacing: Theme.s3
            }
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.topMargin: Theme.s4
            implicitHeight: 1
            color: Theme.hairline
            visible: footerRow.children.length > 0
        }
        RowLayout {
            id: footerRow
            Layout.fillWidth: true
            Layout.margins: Theme.s4
            Layout.leftMargin: Theme.s5
            Layout.rightMargin: Theme.s5
            spacing: Theme.s2
            visible: children.length > 0
        }
    }
}
