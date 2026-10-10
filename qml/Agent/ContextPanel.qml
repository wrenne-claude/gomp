pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// How full the context is, what it cost, and compaction with a before/after view.
Item {
    id: panel
    property Workspace workspace: null
    readonly property real fraction: workspace && workspace.contextWindow > 0 ? workspace.contextTokens / workspace.contextWindow : 0

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: col.implicitHeight + Theme.s5
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: GScrollBar {}

        ColumnLayout {
            id: col
            x: Theme.s4
            y: Theme.s4
            width: flick.width - Theme.s4 * 2
            spacing: Theme.s4

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                GText { text: qsTr("Context window"); role: "caption"; tone: "tertiary" }
                RowLayout {
                    spacing: Theme.s2
                    GText {
                        property real shown: panel.workspace ? panel.workspace.contextTokens : 0
                        Behavior on shown { NumberAnimation { duration: Theme.dMeter; easing.type: Theme.easeOut } }
                        text: Fmt.tokens(shown)
                        role: "readoutLarge"
                    }
                    GText {
                        Layout.alignment: Qt.AlignBaseline
                        text: panel.workspace && panel.workspace.contextWindow > 0 ? qsTr("of %1").arg(Fmt.tokens(panel.workspace.contextWindow)) : ""
                        role: "readout"
                        tone: "tertiary"
                    }
                    Item { Layout.fillWidth: true }
                    GText { text: Fmt.percent(panel.fraction); role: "readout"; tone: panel.fraction > 0.9 ? "danger" : panel.fraction > 0.75 ? "caution" : "secondary" }
                }
                GMeter { Layout.fillWidth: true; implicitHeight: 6; value: panel.fraction }
                GText {
                    Layout.fillWidth: true
                    visible: panel.fraction > 0.75
                    text: qsTr("Getting full. omp compacts automatically near the limit; compacting now lets you choose what to keep.")
                    role: "caption"
                    tone: "caution"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                rowSpacing: Theme.s2
                columnSpacing: Theme.s3
                component Stat: ColumnLayout {
                    property string label
                    property string value
                    Layout.fillWidth: true
                    spacing: 0
                    GText { text: parent.label; role: "caption"; tone: "tertiary" }
                    GText { text: parent.value; role: "readout"; font.pixelSize: 16 }
                }
                Stat { label: qsTr("Input tokens"); value: panel.workspace ? Fmt.tokens(panel.workspace.inputTokens) : "—" }
                Stat { label: qsTr("Output tokens"); value: panel.workspace ? Fmt.tokens(panel.workspace.outputTokens) : "—" }
                Stat { label: qsTr("Session cost"); value: panel.workspace ? Fmt.cost(panel.workspace.cost) : "—" }
                Stat { label: qsTr("Model"); value: panel.workspace ? panel.workspace.modelName : "—" }
            }

            GDivider { Layout.fillWidth: true }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                GText { text: qsTr("Compaction"); role: "strong" }
                GText {
                    Layout.fillWidth: true
                    text: qsTr("Replaces older turns with a summary so the agent keeps working with room to spare.")
                    role: "caption"
                    tone: "tertiary"
                    wrapMode: Text.WordWrap
                    elide: Text.ElideNone
                }
                GButton {
                    text: panel.workspace && panel.workspace.compacting ? qsTr("Compacting") : qsTr("Compact now…")
                    iconName: "compress"
                    loading: panel.workspace && panel.workspace.compacting
                    enabled: panel.workspace && !panel.workspace.busy
                    onClicked: Ui.openOverlay("compact", {})
                }
                CompactionResult {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.s2
                    result: panel.workspace ? panel.workspace.lastCompaction : ({})
                }
            }
        }
    }
}
