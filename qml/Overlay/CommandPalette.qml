pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Ctrl+K: every action, setting, session, model and slash command, fuzzy-matched.
T.Popup {
    id: palette
    parent: T.Overlay.overlay
    x: Math.round((parent.width - width) / 2)
    y: Math.round(parent.height * 0.14)
    width: Math.min(640, parent.width - Theme.s6 * 2)
    height: Math.min(implicitHeight, parent.height * 0.7)
    implicitHeight: contentItem.implicitHeight
    padding: 0
    modal: true
    focus: true
    Connections {
        target: Ui
        function onPaletteOpenChanged() { if (Ui.paletteOpen) palette.open(); else palette.close() }
    }
    onClosed: { Ui.paletteOpen = false; App.palette.query = "" }
    onOpened: { field.text = ""; field.forceActiveFocus(); list.currentIndex = 0; registerActions() }
    T.Overlay.modal: Rectangle { color: Theme.alpha(Theme.scrim, 0.5) }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dFast; easing.type: Theme.easeOut }
            NumberAnimation { property: "scale"; from: 0.98; to: 1; duration: Theme.dBase; easing.type: Theme.easeOut }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.dFast * 0.75 } }
    background: GFloating { level: 3 }

    function ws() { return App.workspaces.current }
    // Actions live here so their handlers can reach the UI; C++ ranks them with everything else.
    readonly property var actionList: [
        { id: "workspace.new", title: qsTr("Open a project"), category: qsTr("Workspace"), icon: "folder-open", run: () => Ui.openOverlay("openProject", {}) },
        { id: "demo.open", title: qsTr("Open the demo workspace"), category: qsTr("Workspace"), icon: "play", run: () => App.openDemo() },
        { id: "session.new", title: qsTr("Start a new session"), category: qsTr("Session"), icon: "plus", run: () => ws() && ws().newSession() },
        { id: "session.compact", title: qsTr("Compact the conversation"), category: qsTr("Session"), icon: "compress", run: () => Ui.openOverlay("compact", {}) },
        { id: "session.fork", title: qsTr("Fork the session"), category: qsTr("Session"), icon: "fork", run: () => ws() && ws().fork() },
        { id: "session.rename", title: qsTr("Rename the session"), category: qsTr("Session"), icon: "pencil", run: () => Ui.openOverlay("rename", {}) },
        { id: "session.export", title: qsTr("Export the session as HTML"), category: qsTr("Session"), icon: "export", run: () => ws() && ws().exportHtml("") },
        { id: "mode.plan", title: qsTr("Switch to plan mode"), category: qsTr("Mode"), icon: "plan", run: () => ws() && ws().setMode("plan") },
        { id: "mode.default", title: qsTr("Switch to build mode"), category: qsTr("Mode"), icon: "corona", run: () => ws() && ws().setMode("default") },
        { id: "mode.goal", title: qsTr("Start a goal"), category: qsTr("Mode"), icon: "target", run: () => Ui.openOverlay("goal", {}) },
        { id: "model.pick", title: qsTr("Choose a model"), category: qsTr("Model"), icon: "chip", run: () => Ui.openOverlay("model", {}) },
        { id: "agent.stop", title: qsTr("Stop the agent"), category: qsTr("Agent"), icon: "stop", run: () => ws() && ws().stop() },
        { id: "agent.retry", title: qsTr("Retry the last turn"), category: qsTr("Agent"), icon: "refresh", run: () => ws() && ws().retry() },
        { id: "nav.sessions", title: qsTr("Browse sessions"), category: qsTr("Go to"), icon: "history", run: () => Ui.go("sessions") },
        { id: "nav.subagents", title: qsTr("Show subagents"), category: qsTr("Go to"), icon: "nodes", run: () => Ui.go("subagents") },
        { id: "nav.settings", title: qsTr("Open settings"), category: qsTr("Go to"), icon: "sliders", run: () => Ui.openSettings("") },
        { id: "settings.providers", title: qsTr("Manage providers and API keys"), category: qsTr("Settings"), icon: "key", run: () => Ui.openSettings("providers") },
        { id: "settings.models", title: qsTr("Assign models to roles"), category: qsTr("Settings"), icon: "chip", run: () => Ui.openSettings("models") },
        { id: "settings.permissions", title: qsTr("Tool approval and permissions"), category: qsTr("Settings"), icon: "shield", run: () => Ui.openSettings("permissions") },
        { id: "settings.mcp", title: qsTr("MCP servers"), category: qsTr("Settings"), icon: "plug", run: () => Ui.openSettings("mcp") },
        { id: "settings.keys", title: qsTr("Keyboard shortcuts settings"), category: qsTr("Settings"), icon: "keyboard", run: () => Ui.openSettings("keybindings") },
        { id: "settings.raw", title: qsTr("Edit config.yml directly"), category: qsTr("Settings"), icon: "file-text", run: () => Ui.openSettings("advanced") },
        { id: "theme.toggle", title: Theme.dark ? qsTr("Use the light theme") : qsTr("Use the dark theme"), category: qsTr("Appearance"), icon: Theme.dark ? "sun" : "moon", run: () => App.prefs.themeMode = Theme.dark ? "light" : "dark" },
        { id: "effects.toggle", title: Theme.reduceEffects ? qsTr("Turn visual effects on") : qsTr("Reduce visual effects"), category: qsTr("Appearance"), icon: "layers", run: () => App.prefs.effects = Theme.reduceEffects ? "full" : "reduced" },
        { id: "inspector.toggle", title: Ui.inspectorOpen ? qsTr("Hide the inspector") : qsTr("Show the inspector"), category: qsTr("View"), icon: "inspector", run: () => Ui.inspectorOpen = !Ui.inspectorOpen },
        { id: "help.shortcuts", title: qsTr("Show keyboard shortcuts"), category: qsTr("Help"), icon: "keyboard", run: () => Ui.shortcutsOpen = true }
    ]
    function registerActions() {
        App.palette.setActions(actionList.map(a => ({ id: a.id, title: a.title, category: a.category, icon: a.icon, shortcut: App.shortcuts.map[a.id] || "" })))
        const w = ws()
        if (w) {
            const names = [], descs = []
            const cmds = w.commands
            cmds.query = ""
            for (let i = 0; i < cmds.count; ++i) { names.push(cmds.nameAt(i)); descs.push("") }
            App.palette.setCommands(names, descs)
        }
    }
    function run(row) {
        const e = App.palette.get(row)
        if (!e.kind) return
        palette.close()
        if (e.kind === "action") {
            const a = actionList.find(x => x.id === e.id)
            if (a) a.run()
        } else if (e.kind === "setting") {
            Ui.openSettings("search:" + e.payload)
        } else if (e.kind === "session") {
            const s = App.sessions.get(e.payload)
            App.openSession(e.payload, s.cwd || "")
            Ui.go("agent")
        } else if (e.kind === "model") {
            if (ws()) ws().setModel(e.payload)
        } else if (e.kind === "command") {
            Ui.go("agent")
            Ui.openOverlay("composerText", { text: "/" + e.payload + " " })
        }
    }

    contentItem: ColumnLayout {
        spacing: 0
        implicitHeight: header.height + list.contentHeight + footer.height + Theme.s2
        Item {
            id: header
            Layout.fillWidth: true
            implicitHeight: 52
            GIcon { x: Theme.s4; anchors.verticalCenter: parent.verticalCenter; name: "search"; color: Theme.textTertiary }
            TextInput {
                id: field
                anchors.left: parent.left
                anchors.leftMargin: Theme.s4 + Theme.iconMd + Theme.s3
                anchors.right: parent.right
                anchors.rightMargin: Theme.s4
                anchors.verticalCenter: parent.verticalCenter
                font: Theme.fHeading
                color: Theme.textPrimary
                selectionColor: Theme.selection
                selectByMouse: true
                onTextChanged: { App.palette.query = text; list.currentIndex = 0 }
                Keys.onDownPressed: list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1)
                Keys.onUpPressed: list.currentIndex = Math.max(0, list.currentIndex - 1)
                Keys.onReturnPressed: palette.run(list.currentIndex)
                Keys.onEnterPressed: palette.run(list.currentIndex)
                Accessible.role: Accessible.EditableText
                Accessible.name: qsTr("Search commands, settings and sessions")
                GText {
                    anchors.fill: parent
                    verticalAlignment: Text.AlignVCenter
                    text: qsTr("Search commands, settings, sessions and models")
                    font: Theme.fHeading
                    color: Theme.textTertiary
                    visible: field.text.length === 0
                }
            }
            GDivider { anchors.bottom: parent.bottom; width: parent.width }
        }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 420)
            Layout.topMargin: Theme.s1
            clip: true
            model: App.palette
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            reuseItems: true
            section.property: "category"
            section.criteria: ViewSection.FullString
            section.delegate: Item {
                id: d146
                required property string section
                width: list.width
                height: 28
                visible: App.palette.query.length === 0
                GText { x: Theme.s4; anchors.bottom: parent.bottom; anchors.bottomMargin: 4; text: d146.section; role: "caption"; tone: "tertiary" }
            }
            delegate: Item {
                id: entry
                required property int index
                required property string kind
                required property string highlighted
                required property string subtitle
                required property string category
                required property string shortcut
                required property string icon
                width: list.width
                height: 40
                readonly property bool current: ListView.isCurrentItem
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s2
                    anchors.rightMargin: Theme.s2
                    radius: Theme.rMd
                    color: entry.current ? Theme.accentSoft : entryHover.hovered ? Theme.surface2 : "transparent"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s4
                    anchors.rightMargin: Theme.s4
                    spacing: Theme.s3
                    GIcon { name: entry.icon || "circle"; size: Theme.iconSm + 2; color: entry.current ? Theme.accent : Theme.textTertiary }
                    Text {
                        text: entry.highlighted
                        textFormat: Text.StyledText
                        font: Theme.fBody
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                    }
                    GText {
                        text: entry.subtitle
                        role: "caption"
                        tone: "tertiary"
                        visible: text.length > 0
                        horizontalAlignment: Text.AlignRight
                        Layout.maximumWidth: 220
                    }
                    GBadge {
                        visible: App.palette.query.length > 0 && entry.kind !== "action"
                        text: entry.kind === "setting" ? qsTr("Setting") : entry.kind === "session" ? qsTr("Session") : entry.kind === "model" ? qsTr("Model") : qsTr("Command")
                    }
                    GKbd { sequence: entry.shortcut }
                }
                HoverHandler { id: entryHover }
                TapHandler { onTapped: palette.run(entry.index) }
            }
            GText {
                anchors.centerIn: parent
                visible: list.count === 0
                text: qsTr("Nothing matches “%1”").arg(field.text)
                tone: "tertiary"
            }
            T.ScrollBar.vertical: GScrollBar {}
        }
        Item {
            id: footer
            Layout.fillWidth: true
            implicitHeight: 36
            GDivider { width: parent.width }
            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.s4
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.s4
                Row { spacing: Theme.s1 + 2; GKbd { sequence: "Up" } GKbd { sequence: "Down" } GText { text: qsTr("move"); role: "caption"; tone: "tertiary"; anchors.verticalCenter: parent.verticalCenter } }
                Row { spacing: Theme.s1 + 2; GKbd { sequence: "Return" } GText { text: qsTr("run"); role: "caption"; tone: "tertiary"; anchors.verticalCenter: parent.verticalCenter } }
                Row { spacing: Theme.s1 + 2; GKbd { sequence: "Esc" } GText { text: qsTr("close"); role: "caption"; tone: "tertiary"; anchors.verticalCenter: parent.verticalCenter } }
            }
        }
    }
}
