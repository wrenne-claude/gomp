pragma ComponentBehavior: Bound
import QtQuick
import Gomp
import Gomp.Core

// --screenshots <dir>: walks through screens and states against the mock backend, saves a PNG of
// each, then quits. Used for visual review and as a UI smoke test (a QML error stops the run).
// Steps either wait a fixed time or until a condition holds (with a timeout).
Item {
    id: driver
    readonly property Window win: Window.window
    readonly property Workspace ws: App.workspaces.current
    property int step: 0
    property int waited: 0

    function size(w, h) { win.width = w; win.height = h }
    function send(text) { ws.send(text) }
    function findSession(title) { return App.sessions.findByTitle(title) }

    property var steps: [
        { name: "01-agent-empty", until: () => ws && ws.connection === Workspace.Ready, then: 600 },
        { name: "01b-composer-text", run: () => Ui.openOverlay("composerText", { text: "How can I check how much size a folder is taking up on my linux system?" }), then: 400 },
        { name: "01c-composer-multiline", run: () => Ui.openOverlay("composerText", { text: "First line of a longer prompt\nSecond line, with gjpqy descenders\nThird line" }), then: 400 },
        { name: "02-agent-streaming", run: () => { Ui.openOverlay("composerText", { text: "" }); send("Explain how session expiry works") }, until: () => ws.activity === Workspace.Streaming, then: 900 },
        { name: "02b-working-a", then: 130 },
        { name: "02c-working-b", then: 130 },
        { name: "03-agent-answer", until: () => !ws.busy, then: 500 },
        { name: "04-agent-approval", run: () => send("Fix the failing rate limiter test"), until: () => ws.pendingApprovals > 0, then: 700 },
        { name: "05-agent-edit-done", run: () => ws.approveAll(), until: () => !ws.busy, then: 600 },
        { name: "06-palette", run: () => Ui.paletteOpen = true, then: 700 },
        { name: "07-palette-search", run: () => App.palette.query = "appro", then: 500 },
        { name: "08-plan-review", run: () => { Ui.paletteOpen = false; ws.setMode("plan") }, until: () => ws.mode === "plan", then: 200,
          after: () => send("Plan a move from in-memory sessions to Redis") },
        { name: "09-plan-waiting", until: () => ws.planPending, then: 900 },
        { name: "10-plan-running", run: () => ws.answerPlan("Approve and execute", ""), then: 1600 },
        { name: "11-subagents-running", run: () => send("Audit the API for security issues in parallel"), until: () => ws.subagents.running > 0, then: 1600 },
        { name: "12-subagents-screen", run: () => Ui.go("subagents"), then: 900 },
        { name: "12b-subagent-drawer", run: () => Ui.openOverlay("subagent", { id: "AuditInput" }), then: 900 },
        { name: "13-agent-subagents-done", run: () => Ui.go("agent"), until: () => !ws.busy, then: 600 },
        { name: "14-inspector-context", run: () => { Ui.inspectorTab = "context"; ws.compact("") }, until: () => !ws.compacting, then: 800 },
        { name: "15-error", run: () => { Ui.inspectorTab = "plan"; send("Check why the deploy failed") }, until: () => !ws.busy, then: 600 },
        { name: "15b-crash", run: () => send("Simulate a crash"), until: () => ws.connection === Workspace.Failed, then: 600 },
        { name: "15c-crash-log", run: () => Ui.openOverlay("diagnostics", {}), then: 900 },
        { name: "15d-restarted", run: () => { Ui.openOverlay("closeDiagnostics", {}); ws.restart() }, until: () => ws.connection === Workspace.Ready, then: 600 },
        { name: "16-light", run: () => App.prefs.themeMode = "light", then: 800 },
        { name: "17-ultrawide", run: () => { App.prefs.themeMode = "dark"; size(2560, 720) }, then: 900 },
        { name: "18-narrow", run: () => size(980, 760), then: 900 },
        { name: "19-reduced-effects", run: () => { size(1440, 900); App.prefs.effects = "reduced" }, then: 700 },
        { name: "20-shortcuts", run: () => { App.prefs.effects = "auto"; Ui.shortcutsOpen = true }, then: 700 },
        { name: "21-model-picker", run: () => { Ui.shortcutsOpen = false; Ui.openOverlay("model", {}) }, then: 800 },
        { name: "22-sessions", run: () => { Ui.go("sessions") }, then: 1200 },
        { name: "23-sessions-branched", run: () => { const s = driver.findSession("Move sessions to Redis"); if (s) Ui.selectSession(s) }, then: 900 },
        { name: "24-sessions-light", run: () => App.prefs.themeMode = "light", then: 800 },
        { name: "30-settings-providers", run: () => { App.prefs.themeMode = "dark"; Ui.openSettings("providers") }, then: 1000 },
        { name: "30b-settings-providers-light", run: () => App.prefs.themeMode = "light", then: 900 },
        { name: "31-settings-models", run: () => { App.prefs.themeMode = "dark"; Ui.openSettings("models") }, then: 900 },
        { name: "32-settings-permissions", run: () => Ui.openSettings("permissions"), then: 900 },
        { name: "33-settings-modes", run: () => Ui.openSettings("modes"), then: 900 },
        { name: "34-settings-mcp", run: () => Ui.openSettings("mcp"), then: 900 },
        { name: "35-settings-extensions", run: () => Ui.openSettings("extensions"), then: 900 },
        { name: "36-settings-appearance", run: () => Ui.openSettings("appearance"), then: 900 },
        { name: "37-settings-keyboard", run: () => Ui.openSettings("keybindings"), then: 900 },
        { name: "38-settings-tab", run: () => Ui.openSettings("tab:interaction"), then: 900 },
        { name: "39-settings-staged", run: () => { App.settings.setValue("tools.approvalMode", "write"); App.settings.setValue("task.maxConcurrency", 6) }, then: 700 },
        { name: "40-settings-review", run: () => Ui.openOverlay("reviewSettings", {}), then: 900 },
        { name: "41-settings-search", run: () => { Ui.openOverlay("closeReview", {}); App.settings.discard(); Ui.openSettings("search:compaction") }, then: 900 },
        { name: "42-settings-advanced", run: () => Ui.openSettings("advanced"), then: 900 },
        { name: "50-no-project", run: () => { Ui.go("agent"); App.workspaces.close(0) }, until: () => App.workspaces.current === null, then: 700 },
        { name: "51-first-run", run: () => Ui.previewFirstRun = true, then: 900 }
    ]

    // GOMP_SHOT_FILTER limits which states are saved; every step still runs to reach them.
    readonly property string filter: App.shotFilter
    function shoot(name, next) {
        if (filter.length && name.indexOf(filter) < 0) { next(); return }
        win.contentItem.grabToImage(result => {
            const path = App.screenshotDir + "/" + name + ".png"
            if (!result.saveToFile(path)) console.warn("could not save", path)
            else console.log("saved", path)
            next()
        })
    }
    function advance() {
        if (step >= steps.length) { Qt.quit(); return }
        const s = steps[step]
        waited = 0
        if (s.run) s.run()
        poll.start()
    }
    Timer {
        id: poll
        interval: 100
        repeat: true
        onTriggered: {
            const s = driver.steps[driver.step]
            driver.waited += interval
            const ready = !s.until || s.until()
            if (!ready && driver.waited < 15000)
                return
            if (!ready) console.warn("timed out waiting for", s.name)
            poll.stop()
            if (s.after) s.after()
            settle.interval = s.then || 500
            settle.start()
        }
    }
    Timer {
        id: settle
        onTriggered: driver.shoot(driver.steps[driver.step].name, () => { driver.step++; driver.advance() })
    }
    Component.onCompleted: advance()
}
