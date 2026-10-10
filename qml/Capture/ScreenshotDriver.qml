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

    property var steps: [
        { name: "01-agent-empty", until: () => ws && ws.connection === Workspace.Ready, then: 600 },
        { name: "02-agent-streaming", run: () => send("Explain how session expiry works"), until: () => ws.activity === Workspace.Streaming, then: 900 },
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
        { name: "13-agent-subagents-done", run: () => Ui.go("agent"), until: () => !ws.busy, then: 600 },
        { name: "14-inspector-context", run: () => { Ui.inspectorTab = "context"; ws.compact("") }, until: () => !ws.compacting, then: 800 },
        { name: "15-error", run: () => { Ui.inspectorTab = "plan"; send("Check why the deploy failed") }, until: () => !ws.busy, then: 600 },
        { name: "16-light", run: () => App.prefs.themeMode = "light", then: 800 },
        { name: "17-ultrawide", run: () => { App.prefs.themeMode = "dark"; size(2560, 720) }, then: 900 },
        { name: "18-narrow", run: () => size(980, 760), then: 900 },
        { name: "19-reduced-effects", run: () => { size(1440, 900); App.prefs.effects = "reduced" }, then: 700 },
        { name: "20-shortcuts", run: () => { App.prefs.effects = "auto"; Ui.shortcutsOpen = true }, then: 700 },
        { name: "21-model-picker", run: () => { Ui.shortcutsOpen = false; Ui.openOverlay("model", {}) }, then: 800 }
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
