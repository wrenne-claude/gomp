pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Instrument readouts for the current workspace. Numbers are tabular and tween like gauges.
Rectangle {
    id: strip
    property Workspace workspace: App.workspaces.current
    implicitHeight: Theme.statusHeight
    color: Theme.surface0
    Rectangle { width: parent.width; height: 1; color: Theme.hairline }

    readonly property real ctxFraction: workspace && workspace.contextWindow > 0 ? workspace.contextTokens / workspace.contextWindow : 0

    component Readout: Row {
        property alias icon: ic.name
        property alias text: label.text
        property color tint: Theme.textTertiary
        spacing: Theme.s1 + 2
        GIcon { id: ic; size: 14; color: parent.tint; anchors.verticalCenter: parent.verticalCenter }
        GText { id: label; role: "readout"; color: Theme.textSecondary; anchors.verticalCenter: parent.verticalCenter }
    }
    component Sep: Rectangle { width: 1; height: 12; color: Theme.hairline; Layout.alignment: Qt.AlignVCenter }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s3
        anchors.rightMargin: Theme.s3
        spacing: Theme.s3
        visible: strip.workspace !== null

        Row {
            spacing: Theme.s1 + 2
            GStatusLight {
                anchors.verticalCenter: parent.verticalCenter
                diameter: 7
                tint: strip.workspace ? Theme.modeColor(strip.workspace.mode) : Theme.textTertiary
            }
            GText {
                anchors.verticalCenter: parent.verticalCenter
                text: strip.workspace ? Theme.modeTitle(strip.workspace.mode) : ""
                role: "caption"
                color: Theme.textPrimary
            }
        }
        Sep {}
        Readout {
            icon: "chip"
            text: strip.workspace && strip.workspace.modelName ? strip.workspace.modelName + (strip.workspace.thinkingLevel ? "  " + strip.workspace.thinkingLevel : "") : qsTr("No model")
        }
        Sep {}
        Row {
            spacing: Theme.s2
            Accessible.role: Accessible.ProgressBar
            Accessible.name: qsTr("Context %1 used").arg(Fmt.percent(strip.ctxFraction))
            GMeter {
                width: 72
                anchors.verticalCenter: parent.verticalCenter
                value: strip.ctxFraction
            }
            GText {
                id: ctxText
                anchors.verticalCenter: parent.verticalCenter
                role: "readout"
                color: Theme.textSecondary
                property real shownTokens: strip.workspace ? strip.workspace.contextTokens : 0
                Behavior on shownTokens { NumberAnimation { duration: Theme.dMeter; easing.type: Theme.easeOut } }
                text: Fmt.tokens(shownTokens) + " / " + (strip.workspace && strip.workspace.contextWindow > 0 ? Fmt.tokens(strip.workspace.contextWindow) : "—")
            }
        }
        Sep {}
        Readout {
            icon: "coin"
            property real shownCost: strip.workspace ? strip.workspace.cost : 0
            Behavior on shownCost { NumberAnimation { duration: Theme.dMeter; easing.type: Theme.easeOut } }
            text: Fmt.cost(shownCost)
        }
        Readout {
            visible: strip.workspace && strip.workspace.busy && strip.workspace.tokensPerSecond > 0
            icon: "gauge"
            text: Math.round(strip.workspace ? strip.workspace.tokensPerSecond : 0) + " tok/s"
        }

        Item { Layout.fillWidth: true }

        Readout {
            visible: strip.workspace && strip.workspace.git.isRepository
            icon: "branch"
            text: strip.workspace ? strip.workspace.git.branch + (strip.workspace.git.changedFiles > 0 ? "  ±" + strip.workspace.git.changedFiles : "") : ""
        }
        Readout {
            visible: strip.width > 1000
            icon: "folder"
            text: strip.workspace ? Fmt.shortPath(strip.workspace.cwd) : ""
        }
        Row {
            spacing: Theme.s1 + 2
            visible: strip.workspace !== null
            GStatusLight {
                anchors.verticalCenter: parent.verticalCenter
                diameter: 6
                tint: !strip.workspace ? Theme.textDisabled
                      : strip.workspace.connection === Workspace.Ready ? Theme.success
                      : strip.workspace.connection === Workspace.Failed ? Theme.danger : Theme.caution
                pulse: strip.workspace && (strip.workspace.connection === Workspace.Starting || strip.workspace.connection === Workspace.Switching)
            }
            GText {
                anchors.verticalCenter: parent.verticalCenter
                role: "caption"
                tone: "tertiary"
                text: !strip.workspace ? "" : strip.workspace.mock ? qsTr("mock")
                      : strip.workspace.connection === Workspace.Ready ? "omp " + App.omp.version + (strip.workspace.transport === "acp" ? qsTr(" (plan)") : "")
                      : strip.workspace.connection === Workspace.Starting ? qsTr("Starting omp")
                      : strip.workspace.connection === Workspace.Failed ? qsTr("omp stopped") : qsTr("Disconnected")
            }
        }
    }
}
