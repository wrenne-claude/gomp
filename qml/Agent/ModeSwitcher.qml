pragma ComponentBehavior: Bound
import QtQuick
import Gomp
import Gomp.Core

// Build / Plan / Goal. Each mode has its own light; the selection travels on a spring and the
// whole header picks up the mode colour through the ambient light behind it.
GSegmented {
    id: switcher
    property Workspace workspace: null
    size: "md"
    showLabels: width > 0
    value: workspace ? workspace.mode : "default"
    enabled: workspace !== null && workspace.connection === Workspace.Ready && !workspace.modeSwitching
    opacity: enabled || (workspace && workspace.modeSwitching) ? 1 : 0.6
    options: [
        { value: "default", label: qsTr("Build"), icon: "corona", color: Theme.accent },
        { value: "plan", label: qsTr("Plan"), icon: "plan", color: Theme.blueprint },
        { value: "goal", label: qsTr("Goal"), icon: "target", color: Theme.lock }
    ]
    onActivated: v => {
        if (!workspace || v === workspace.mode) return
        if (v === "goal") Ui.openOverlay("goal", {})
        else workspace.setMode(v)
    }
    GToolTip {
        visible: hover.hovered
        text: switcher.workspace && switcher.workspace.modeSwitching ? qsTr("Switching mode…")
              : switcher.value === "plan" ? qsTr("Plan: read-only exploration, then a plan for you to approve")
              : switcher.value === "goal" ? qsTr("Goal: works toward an objective until it is met")
              : qsTr("Build: the agent reads, edits and runs tools")
        shortcut: App.shortcuts.map["mode.plan"] || ""
    }
    HoverHandler { id: hover }
}
