pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// The agent view: header (session, mode, model), conversation, composer, and the inspector,
// docked on wide windows and an overlay drawer on narrow ones.
Item {
    id: screen
    readonly property Workspace ws: App.workspaces.current
    readonly property bool wide: width >= 1100
    readonly property bool inspectorDocked: wide && Ui.inspectorOpen
    readonly property bool drawerOpen: !wide && Ui.inspectorOpen && ws !== null
    readonly property int inspectorWidth: width >= 1900 ? 440 : 360

    EmptyAgent {
        anchors.fill: parent
        visible: screen.ws === null
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: screen.ws !== null

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            AgentHeader {
                Layout.fillWidth: true
                workspace: screen.ws
            }
            ConnectionBanner {
                Layout.fillWidth: true
                workspace: screen.ws
            }
            Conversation {
                id: conversation
                Layout.fillWidth: true
                Layout.fillHeight: true
                workspace: screen.ws
            }
            Composer {
                Layout.fillWidth: true
                Layout.maximumWidth: Theme.proseMaxWidth + Theme.s6 * 2 + 160
                Layout.alignment: Qt.AlignHCenter
                workspace: screen.ws
            }
        }
        Inspector {
            Layout.fillHeight: true
            Layout.preferredWidth: screen.inspectorDocked ? screen.inspectorWidth : 0
            visible: screen.inspectorDocked
            workspace: screen.ws
            docked: true
        }
    }

    // Narrow windows: the inspector slides over the conversation as a frosted drawer, with a
    // scrim that dims what it covers; clicking the scrim closes it.
    Rectangle {
        anchors.fill: parent
        color: Theme.scrim
        opacity: screen.drawerOpen ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.dBase } }
        TapHandler { onTapped: Ui.inspectorOpen = false }
    }
    Inspector {
        id: drawer
        visible: screen.drawerOpen
        workspace: screen.ws
        docked: false
        width: Math.min(360, screen.width - Theme.s6)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: visible ? screen.width - width : screen.width
        Behavior on x { NumberAnimation { duration: Theme.dSlow; easing.type: Theme.easeOut } }
    }

    OverlayRouter { workspace: screen.ws }
}
