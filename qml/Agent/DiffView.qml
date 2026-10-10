import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp

// Line diff with old/new gutters. `lines` is [{kind: context|added|removed|gap, oldNo, newNo, text}].
// Large diffs show the first 300 lines until expanded.
Rectangle {
    id: dv
    property var lines: []
    property string path
    property int added: 0
    property int removed: 0
    property bool showHeader: true
    property int maxLines: expanded ? 5000 : 300
    property bool expanded: false
    radius: Theme.rMd
    color: Theme.codeBg
    border.width: 1
    border.color: Theme.hairline
    clip: true
    implicitHeight: (showHeader ? header.height : 0) + Math.min(column.implicitHeight, 520) + (moreButton.visible ? moreButton.height + Theme.s2 : 0)

    RowLayout {
        id: header
        visible: dv.showHeader
        width: parent.width
        height: 32
        spacing: Theme.s2
        GIcon { Layout.leftMargin: Theme.s3; name: "file"; size: Theme.iconSm; color: Theme.textTertiary }
        GText { text: dv.path; role: "code"; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideMiddle }
        GText { text: "+" + dv.added; role: "readout"; color: Theme.diffAddText; visible: dv.added > 0 }
        GText { text: "−" + dv.removed; role: "readout"; color: Theme.diffRemoveText; visible: dv.removed > 0; Layout.rightMargin: Theme.s3 }
    }
    Rectangle { visible: dv.showHeader; y: header.height; width: parent.width; height: 1; color: Theme.hairline }

    Flickable {
        y: dv.showHeader ? header.height + 1 : 0
        width: parent.width
        height: Math.min(column.implicitHeight, 520)
        contentWidth: Math.max(width, column.implicitWidth)
        contentHeight: column.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height || contentWidth > width
        T.ScrollBar.vertical: GScrollBar {}
        T.ScrollBar.horizontal: GScrollBar {}
        Column {
            id: column
            width: Math.max(implicitWidth, parent.width)
            topPadding: Theme.s1
            bottomPadding: Theme.s1
            Repeater {
                model: dv.lines.slice(0, dv.maxLines)
                delegate: Rectangle {
                    required property var modelData
                    width: Math.max(column.width, row.implicitWidth)
                    height: modelData.kind === "gap" ? 22 : 20
                    color: modelData.kind === "added" ? Theme.diffAddBg : modelData.kind === "removed" ? Theme.diffRemoveBg : "transparent"
                    Row {
                        id: row
                        height: parent.height
                        GText {
                            width: 44
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignRight
                            text: parent.parent.modelData.kind === "gap" ? "" : (parent.parent.modelData.oldNo || "")
                            role: "readout"
                            font.pixelSize: 11
                            tone: "tertiary"
                        }
                        GText {
                            width: 44
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignRight
                            text: parent.parent.modelData.kind === "gap" ? "" : (parent.parent.modelData.newNo || "")
                            role: "readout"
                            font.pixelSize: 11
                            tone: "tertiary"
                        }
                        GText {
                            width: 24
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: Text.AlignHCenter
                            text: parent.parent.modelData.kind === "added" ? "+" : parent.parent.modelData.kind === "removed" ? "−" : ""
                            font: Theme.fCode
                            color: parent.parent.modelData.kind === "added" ? Theme.diffAddText : Theme.diffRemoveText
                        }
                        Text {
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            text: parent.parent.modelData.kind === "gap" ? "⋯" : parent.parent.modelData.text
                            font: Theme.fCode
                            textFormat: Text.PlainText
                            rightPadding: Theme.s3
                            color: parent.parent.modelData.kind === "added" ? Theme.diffAddText
                                 : parent.parent.modelData.kind === "removed" ? Theme.diffRemoveText
                                 : parent.parent.modelData.kind === "gap" ? Theme.textTertiary : Theme.textPrimary
                        }
                    }
                }
            }
        }
    }
    GButton {
        id: moreButton
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.s1
        anchors.horizontalCenter: parent.horizontalCenter
        visible: !dv.expanded && dv.lines.length > 300
        text: qsTr("Show all %1 lines").arg(dv.lines.length)
        size: "sm"
        variant: "ghost"
        onClicked: dv.expanded = true
    }
}
