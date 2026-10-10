import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Before/after of the last compaction as two bars on the same scale, plus what was kept.
ColumnLayout {
    id: res
    property var result: ({})
    readonly property real before: result.tokensBefore || 0
    readonly property real after: result.tokensAfter || 0
    visible: before > 0
    spacing: Theme.s2

    component Bar: RowLayout {
        property string label
        property real tokens
        property color tint
        Layout.fillWidth: true
        spacing: Theme.s2
        GText { text: parent.label; role: "caption"; tone: "tertiary"; Layout.preferredWidth: 44 }
        Item {
            Layout.fillWidth: true
            implicitHeight: 10
            Rectangle { anchors.fill: parent; radius: 3; color: Theme.surface2 }
            Rectangle {
                id: fill
                height: parent.height
                radius: 3
                color: parent.parent.tint
                width: res.before > 0 ? parent.width * Math.min(1, parent.parent.tokens / res.before) : 0
                Behavior on width { NumberAnimation { duration: Theme.dMeter; easing.type: Theme.easeOut } }
            }
        }
        GText { text: Fmt.tokens(parent.tokens); role: "readout"; Layout.preferredWidth: 48; horizontalAlignment: Text.AlignRight }
    }

    GText { text: qsTr("Last compaction"); role: "caption"; tone: "tertiary" }
    Bar { label: qsTr("Before"); tokens: res.before; tint: Theme.textTertiary }
    Bar { label: qsTr("After"); tokens: res.after; tint: Theme.accent }
    GText {
        visible: res.before > 0 && res.after > 0
        text: qsTr("Freed %1 tokens (%2)").arg(Fmt.tokens(res.before - res.after)).arg(Fmt.percent(1 - res.after / res.before))
        role: "caption"
        tone: "success"
    }
    GText {
        Layout.fillWidth: true
        visible: (res.result.summary || "").length > 0
        text: res.result.summary || ""
        tone: "secondary"
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        maximumLineCount: 8
    }
}
