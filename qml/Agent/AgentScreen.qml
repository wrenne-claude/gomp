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
    // The inspector is as wide as you leave it (drag its edge; double-click resets), within what the
    // window allows. It docks while the conversation keeps a readable column beside it, and floats
    // over the conversation as a drawer otherwise.
    readonly property int minConversation: 560
    readonly property int maxInspector: Math.max(300, Math.min(900, width - (wide ? minConversation : 48)))
    property int liveWidth: App.prefs.inspectorWidth
    readonly property int inspectorWidth: Math.max(300, Math.min(maxInspector, liveWidth))
    readonly property bool wide: width - Math.max(300, Math.min(900, liveWidth)) >= minConversation
    readonly property bool inspectorDocked: wide && Ui.inspectorOpen
    readonly property bool drawerOpen: !wide && Ui.inspectorOpen && ws !== null
    Connections {
        target: App.prefs
        function onChanged() { screen.liveWidth = App.prefs.inspectorWidth }
    }

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
            id: dockedInspector
            Layout.fillHeight: true
            Layout.preferredWidth: screen.inspectorDocked ? screen.inspectorWidth : 0
            visible: screen.inspectorDocked
            workspace: screen.ws
            docked: true
            GSplitHandle {
                x: -width / 2
                height: parent.height
                value: screen.inspectorWidth
                from: 300
                to: screen.maxInspector
                onMoved: v => screen.liveWidth = v
                onCommitted: v => App.prefs.inspectorWidth = v
            }
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
        width: Math.min(screen.inspectorWidth, screen.width - Theme.s6)
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        x: visible ? screen.width - width : screen.width
        Behavior on x { NumberAnimation { duration: Theme.dSlow; easing.type: Theme.easeOut } }
        GSplitHandle {
            x: -width / 2 + Theme.s2
            height: parent.height
            value: drawer.width
            from: 300
            to: screen.maxInspector
            onMoved: v => screen.liveWidth = v
            onCommitted: v => App.prefs.inspectorWidth = v
        }
    }

    OverlayRouter { workspace: screen.ws }
}
