import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

Window {
    id: window
    width: Launch.size.width > 0 ? Launch.size.width : 1440
    height: Launch.size.height > 0 ? Launch.size.height : 900
    minimumWidth: 760
    minimumHeight: 480
    visible: true
    color: Theme.hull
    flags: App.prefs.customTitlebar ? (Qt.Window | Qt.FramelessWindowHint) : Qt.Window
    title: App.workspaces.current ? App.workspaces.current.name + " - gomp" : "gomp"

    readonly property bool frameless: App.prefs.customTitlebar
    // Short displays (e.g. a 2560x720 ultrawide) get the compact vertical rhythm.
    Binding {
        target: Theme
        property: "compact"
        value: App.prefs.density === "compact" || (App.prefs.density === "auto" && window.height < 820)
    }
    Component.onCompleted: {
        Ui.backdrop = backdrop
        Ui.screen = App.prefs.lastScreen || "agent"
        Ui.inspectorOpen = App.prefs.inspectorOpen
    }
    Connections {
        target: Ui
        function onScreenChanged() { App.prefs.lastScreen = Ui.screen }
        function onInspectorOpenChanged() { App.prefs.inspectorOpen = Ui.inspectorOpen }
    }
    Connections {
        target: App
        function onRaiseRequested() { window.raise(); window.requestActivate() }
        function onToast(level, message) { Ui.toast(level, message) }
    }

    // Everything frosted layers may blur lives in `backdrop`; overlays sit outside it.
    Item {
        id: backdrop
        anchors.fill: parent

        // Opaque base: screenshots and the frosted layers sample this, not the window clear colour.
        Rectangle { anchors.fill: parent; color: Theme.hull }

        AmbientLight {
            anchors.fill: parent
            workspace: App.workspaces.current
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TitleBar {
                Layout.fillWidth: true
                frameless: window.frameless
            }
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                NavRail { Layout.fillHeight: true }
                Item {
                    id: screens
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    // Screens stay alive once visited so scroll positions and drafts survive.
                    component ScreenSlot: Loader {
                        id: slot
                        property string name
                        readonly property bool shown: Ui.screen === name
                        anchors.fill: parent
                        active: shown || item !== null
                        visible: opacity > 0
                        opacity: shown ? 1 : 0
                        z: shown ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: Theme.dBase; easing.type: Theme.easeOut } }
                        // A short rise on entry gives the crossfade direction without slowing it.
                        transform: Translate { y: slot.shown ? 0 : (Theme.reduceMotion ? 0 : 6); Behavior on y { NumberAnimation { duration: Theme.dBase; easing.type: Theme.easeOut } } }
                    }
                    ScreenSlot { name: "agent"; source: "Agent/AgentScreen.qml" }
                    ScreenSlot { name: "sessions"; source: "Sessions/SessionsScreen.qml" }
                    ScreenSlot { name: "subagents"; source: "Subagents/SubagentsScreen.qml" }
                    ScreenSlot { name: "settings"; source: "Settings/SettingsScreen.qml" }
                }
            }
            StatusStrip { Layout.fillWidth: true }
        }
    }

    FirstRun {
        anchors.fill: parent
        visible: App.needsFirstRun
    }

    ToastHost {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Theme.s4
        anchors.bottomMargin: Theme.statusHeight + Theme.s4
    }
    CommandPalette { id: palette }
    ShortcutSheet { id: shortcutSheet }
    UiRequestHost { workspace: App.workspaces.current }

    Loader {
        active: App.screenshotDir.length > 0
        source: "Capture/ScreenshotDriver.qml"
    }

    // ---- Global shortcuts -------------------------------------------------------------------------
    component AppShortcut: Shortcut {
        property string action
        sequences: App.shortcuts.map[action] ? [App.shortcuts.map[action]] : []
        context: Qt.ApplicationShortcut
        enabled: !App.needsFirstRun
    }
    AppShortcut { action: "palette.open"; onActivated: Ui.paletteOpen = !Ui.paletteOpen }
    AppShortcut { action: "help.shortcuts"; onActivated: Ui.shortcutsOpen = !Ui.shortcutsOpen }
    AppShortcut { action: "app.quit"; onActivated: Qt.quit() }
    AppShortcut { action: "nav.agent"; onActivated: Ui.go("agent") }
    AppShortcut { action: "nav.sessions"; onActivated: Ui.go("sessions") }
    AppShortcut { action: "nav.subagents"; onActivated: Ui.go("subagents") }
    AppShortcut { action: "nav.settings"; onActivated: Ui.go("settings") }
    AppShortcut { action: "inspector.toggle"; onActivated: Ui.inspectorOpen = !Ui.inspectorOpen }
    AppShortcut { action: "workspace.next"; onActivated: App.workspaces.next() }
    AppShortcut { action: "workspace.previous"; onActivated: App.workspaces.previous() }
    AppShortcut { action: "workspace.close"; onActivated: if (App.workspaces.count > 0) App.workspaces.close(App.workspaces.currentIndex) }
    AppShortcut { action: "composer.focus"; onActivated: { Ui.go("agent"); Ui.focusComposer() } }
    AppShortcut {
        action: "agent.stop"
        onActivated: if (App.workspaces.current) App.workspaces.current.stop()
    }
    AppShortcut {
        action: "mode.plan"
        onActivated: {
            const w = App.workspaces.current
            if (w) w.setMode(w.mode === "plan" ? "default" : "plan")
        }
    }
    AppShortcut {
        action: "thinking.cycle"
        onActivated: {
            const w = App.workspaces.current
            if (!w) return
            const levels = w.thinkingLevels
            const i = levels.indexOf(w.thinkingLevel)
            w.setThinkingLevel(levels[(i + 1) % levels.length])
            Ui.toast("info", qsTr("Reasoning effort: %1").arg(levels[(i + 1) % levels.length]))
        }
    }

    // ---- Frameless resize edges ---------------------------------------------------------------------
    component Edge: Item {
        property int edges
        property int cursor
        z: 100
        visible: window.frameless && window.visibility !== Window.Maximized && window.visibility !== Window.FullScreen
        HoverHandler { cursorShape: parent.cursor }
        DragHandler {
            target: null
            grabPermissions: PointerHandler.CanTakeOverFromAnything
            onActiveChanged: if (active) window.startSystemResize(parent.edges)
        }
    }
    Edge { edges: Qt.LeftEdge; cursor: Qt.SizeHorCursor; x: 0; y: 8; width: 5; height: parent.height - 16 }
    Edge { edges: Qt.RightEdge; cursor: Qt.SizeHorCursor; x: parent.width - 5; y: 8; width: 5; height: parent.height - 16 }
    Edge { edges: Qt.TopEdge; cursor: Qt.SizeVerCursor; x: 8; y: 0; width: parent.width - 16; height: 4 }
    Edge { edges: Qt.BottomEdge; cursor: Qt.SizeVerCursor; x: 8; y: parent.height - 5; width: parent.width - 16; height: 5 }
    Edge { edges: Qt.LeftEdge | Qt.TopEdge; cursor: Qt.SizeFDiagCursor; x: 0; y: 0; width: 8; height: 8 }
    Edge { edges: Qt.RightEdge | Qt.TopEdge; cursor: Qt.SizeBDiagCursor; x: parent.width - 8; y: 0; width: 8; height: 8 }
    Edge { edges: Qt.LeftEdge | Qt.BottomEdge; cursor: Qt.SizeBDiagCursor; x: 0; y: parent.height - 8; width: 8; height: 8 }
    Edge { edges: Qt.RightEdge | Qt.BottomEdge; cursor: Qt.SizeFDiagCursor; x: parent.width - 8; y: parent.height - 8; width: 8; height: 8 }
}
