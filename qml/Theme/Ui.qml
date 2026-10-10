pragma Singleton
import QtQuick

// Shell state shared across the component tree: the backdrop frosted layers blur, the toast and
// overlay entry points, and the current navigation. Main fills in the item references.
QtObject {
    // The window content beneath floating layers; frosted surfaces sample it for blur.
    property Item backdrop: null
    // Screen navigation: agent | sessions | subagents | settings
    property string screen: "agent"
    property string settingsPage: "providers"
    property bool inspectorOpen: true
    property string inspectorTab: "plan"
    property bool paletteOpen: false
    property bool shortcutsOpen: false

    signal toastRequested(string level, string message, string actionText, var action)
    signal openOverlay(string name, var props)
    signal focusComposer()

    function toast(level, message, actionText, action) {
        toastRequested(level, message, actionText || "", action || null)
    }
    function go(name) {
        screen = name
    }
    function openSettings(page) {
        if (page)
            settingsPage = page
        screen = "settings"
    }
}
