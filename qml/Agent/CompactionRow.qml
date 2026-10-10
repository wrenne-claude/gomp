import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// A marker where the context was compacted: before/after token counts and the summary omp kept.
Item {
    id: row
    required property string text
    required property double tokensBefore
    required property double tokensAfter
    property bool expanded: false
    implicitHeight: col.implicitHeight

    ColumnLayout {
        id: col
        width: parent.width
        spacing: Theme.s2
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s3
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairlineStrong }
            GIcon { name: "compress"; size: Theme.iconSm; color: Theme.textTertiary }
            GText {
                text: row.tokensBefore > 0
                      ? qsTr("Context compacted, %1 to %2").arg(Fmt.tokens(row.tokensBefore)).arg(row.tokensAfter > 0 ? Fmt.tokens(row.tokensAfter) : qsTr("a summary"))
                      : qsTr("Context compacted")
                role: "caption"
                tone: "secondary"
            }
            GIconButton {
                size: 22
                visible: row.text.length > 0
                iconName: row.expanded ? "chevron-up" : "chevron-down"
                tooltip: row.expanded ? qsTr("Hide summary") : qsTr("Show what was kept")
                onClicked: row.expanded = !row.expanded
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.hairlineStrong }
        }
        GText {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.s6
            Layout.rightMargin: Theme.s6
            visible: row.expanded
            text: row.text
            tone: "secondary"
            wrapMode: Text.WordWrap
            elide: Text.ElideNone
        }
    }
}
