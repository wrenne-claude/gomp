pragma Singleton
import QtQuick
import Gomp.Core

// Design tokens: the only place colours, type, spacing, radii, elevation and motion are defined.
// Components never use literal colours or sizes; see docs/design.md for the rationale.
QtObject {
    id: theme

    // ---- Inputs ----------------------------------------------------------------------------------
    readonly property bool dark: Launch.theme === "light" ? false
                                : Launch.theme === "dark" ? true
                                : App.prefs.themeMode === "system" ? App.systemDark
                                : App.prefs.themeMode !== "light"
    readonly property bool reduceEffects: Launch.reduceEffects
                                          || App.prefs.effects === "reduced"
                                          || (App.prefs.effects === "auto" && App.effects.softwareRendering)
    readonly property bool reduceMotion: App.prefs.reduceMotion
    // Set by Main from window height and the density preference.
    property bool compact: false

    // ---- Colour ----------------------------------------------------------------------------------
    readonly property color hull: dark ? "#0D1117" : "#EEF1F5"
    readonly property color surface0: dark ? "#121821" : "#F7F9FB"
    readonly property color surface1: dark ? "#18202B" : "#FFFFFF"
    readonly property color surface2: dark ? "#202A37" : "#F1F4F8"
    readonly property color surface3: dark ? "#283445" : "#E6EBF1"
    readonly property color hairline: dark ? "#263142" : "#D5DCE5"
    readonly property color hairlineStrong: dark ? "#33415A" : "#B9C3D0"
    readonly property color innerHighlight: dark ? Qt.rgba(1, 1, 1, 0.04) : Qt.rgba(1, 1, 1, 0.7)

    readonly property color textPrimary: dark ? "#E7ECF3" : "#121821"
    readonly property color textSecondary: dark ? "#A3AFBF" : "#4A5668"
    readonly property color textTertiary: dark ? "#7D8BA2" : "#5F6B7E"
    readonly property color textDisabled: dark ? "#4F5B6E" : "#A2ABB8"

    readonly property color sodium: dark ? "#FFB547" : "#B86E00"
    readonly property color blueprint: dark ? "#6CB4FF" : "#1F6FD1"
    readonly property color lock: dark ? "#5BD49B" : "#1E8A57"
    readonly property color ion: dark ? "#4FD1C5" : "#0F8A80"
    readonly property color rose: dark ? "#F27BA6" : "#C2316A"
    readonly property color danger: dark ? "#FF6B6B" : "#C62E2E"
    readonly property color caution: dark ? "#F2D35B" : "#8A6D00"
    readonly property color info: dark ? "#8FA7FF" : "#3A55C9"
    readonly property color success: lock

    readonly property color accent: accentNamed(App.prefs.accent)
    readonly property color accentHover: dark ? Qt.lighter(accent, 1.08) : Qt.darker(accent, 1.08)
    readonly property color accentPressed: dark ? Qt.darker(accent, 1.12) : Qt.darker(accent, 1.2)
    readonly property color onAccent: dark ? "#1A1206" : "#FFFFFF"
    readonly property color accentSoft: alpha(accent, dark ? 0.14 : 0.12)
    readonly property color focusRing: accent
    readonly property color selection: alpha(accent, dark ? 0.28 : 0.22)
    readonly property color scrim: alpha(dark ? "#05070A" : "#2A3341", dark ? 0.62 : 0.32)
    readonly property color shadow: dark ? "#000000" : "#1B2533"

    readonly property color codeBg: dark ? "#0F141C" : "#F4F6F9"
    readonly property color diffAddBg: alpha(lock, dark ? 0.12 : 0.10)
    readonly property color diffRemoveBg: alpha(danger, dark ? 0.12 : 0.09)
    readonly property color diffAddText: dark ? "#9FE5C0" : "#145C3A"
    readonly property color diffRemoveText: dark ? "#FFB0B0" : "#8E1F1F"

    function alpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a)
    }
    function mix(a, b, t) {
        return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t)
    }
    function accentNamed(name) {
        switch (name) {
        case "blueprint": return blueprint
        case "lock": return lock
        case "ion": return ion
        case "rose": return rose
        case "system": return App.systemAccent.valid && App.systemAccent.hslSaturation > 0.15 ? App.systemAccent : sodium
        default: return sodium
        }
    }
    // Mode light: the ambient hue of a mode. Controls keep the accent.
    function modeColor(mode) {
        if (mode === "plan") return blueprint
        if (mode === "goal") return lock
        return accent
    }
    function modeTitle(mode) {
        if (mode === "plan") return qsTr("Plan")
        if (mode === "goal") return qsTr("Goal")
        return qsTr("Build")
    }
    function modeIcon(mode) {
        if (mode === "plan") return "plan"
        if (mode === "goal") return "target"
        return "corona"
    }
    // Workspace.Activity -> colour
    function activityColor(activity, mode) {
        if (activity === Workspace.Approval) return sodium
        if (activity === Workspace.Error) return danger
        return modeColor(mode)
    }
    function activityLabel(activity) {
        switch (activity) {
        case Workspace.Waiting: return qsTr("Waiting for the model")
        case Workspace.Thinking: return qsTr("Thinking")
        case Workspace.Streaming: return qsTr("Writing")
        case Workspace.Tools: return qsTr("Running tools")
        case Workspace.Approval: return qsTr("Needs your input")
        case Workspace.Error: return qsTr("Error")
        default: return qsTr("Idle")
        }
    }

    // ---- Syntax ----------------------------------------------------------------------------------
    readonly property var syntax: ({
        text: dark ? "#D8DFEA" : "#1E2733",
        keyword: dark ? "#FFB547" : "#A35F00",
        type: dark ? "#8FA7FF" : "#3A55C9",
        string: dark ? "#9FD8A8" : "#2D7A3E",
        number: dark ? "#F2D35B" : "#8A6D00",
        comment: dark ? "#6B7890" : "#7A8597",
        function: dark ? "#6CB4FF" : "#1F6FD1",
        punctuation: dark ? "#8F9BAD" : "#5F6B7E",
        added: dark ? "#9FE5C0" : "#145C3A",
        removed: dark ? "#FFB0B0" : "#8E1F1F",
        meta: dark ? "#8FA7FF" : "#3A55C9",
        accent: dark ? "#FFB547" : "#A35F00",
        secondary: dark ? "#A3AFBF" : "#4A5668",
        codeText: dark ? "#D8DFEA" : "#1E2733",
        codeBg: dark ? "#1C2430" : "#E9EDF2",
        hairline: dark ? "#33415A" : "#C9D1DC",
        headerBg: dark ? "#18202B" : "#F1F4F8"
    })
    onSyntaxChanged: App.setSyntaxPalette(syntax)
    Component.onCompleted: App.setSyntaxPalette(syntax)

    // ---- Space and size (4px grid) ----------------------------------------------------------------
    readonly property int s1: 4
    readonly property int s2: 8
    readonly property int s3: 12
    readonly property int s4: 16
    readonly property int s5: 24
    readonly property int s6: 32
    readonly property int s7: 48
    readonly property int s8: 64
    // Vertical rhythm tightens on short displays (2560x720).
    readonly property int padV: compact ? s2 : s3
    readonly property int sectionGap: compact ? s4 : s5

    readonly property int controlSm: 28
    readonly property int controlMd: 32
    readonly property int controlLg: 40
    readonly property int iconSm: 16
    readonly property int iconMd: 20
    readonly property int railWidth: 56
    readonly property int titlebarHeight: compact ? 40 : 44
    readonly property int statusHeight: 28
    readonly property int proseMaxWidth: 760

    readonly property int rSm: 4
    readonly property int rMd: 8
    readonly property int rLg: 12
    readonly property int rXl: 16

    // ---- Type ----
    // Ligatures are off throughout: Barlow joins "--" into a dash, which corrupts flags and paths.---------------------------------------------------------------------------------
    readonly property string uiFamily: "Barlow"
    readonly property string readoutFamily: "Barlow Semi Condensed"
    readonly property string monoFamily: "JetBrains Mono"

    readonly property font fDisplay: Qt.font({ family: uiFamily, pixelSize: 28, weight: Font.DemiBold , features: { "liga": 0 } })
    readonly property font fTitle: Qt.font({ family: uiFamily, pixelSize: 20, weight: Font.DemiBold , features: { "liga": 0 } })
    readonly property font fHeading: Qt.font({ family: uiFamily, pixelSize: 16, weight: Font.DemiBold , features: { "liga": 0 } })
    readonly property font fBody: Qt.font({ family: uiFamily, pixelSize: 14, weight: Font.Normal , features: { "liga": 0 } })
    readonly property font fBodyStrong: Qt.font({ family: uiFamily, pixelSize: 14, weight: Font.Medium , features: { "liga": 0 } })
    readonly property font fCaption: Qt.font({ family: uiFamily, pixelSize: 12, weight: Font.Medium , features: { "liga": 0 } })
    readonly property font fReadout: Qt.font({ family: readoutFamily, pixelSize: 13, weight: Font.Medium, features: { "tnum": 1 } })
    readonly property font fReadoutLarge: Qt.font({ family: readoutFamily, pixelSize: 22, weight: Font.Medium, features: { "tnum": 1 } })
    // Ligatures off: code should show the characters the agent wrote.
    readonly property font fCode: Qt.font({ family: monoFamily, pixelSize: 13, weight: Font.Normal, features: { "calt": 0, "liga": 0 } })
    readonly property font fCodeSmall: Qt.font({ family: monoFamily, pixelSize: 12, weight: Font.Normal, features: { "calt": 0, "liga": 0 } })
    readonly property real lhBody: 1.4
    readonly property real lhCode: 1.54

    // ---- Motion -----------------------------------------------------------------------------------
    readonly property int dFast: reduceMotion ? 0 : 120
    readonly property int dBase: reduceMotion ? 0 : 180
    readonly property int dSlow: reduceMotion ? 80 : 260
    readonly property int dMeter: reduceMotion ? 0 : 400
    readonly property int dStagger: reduceMotion ? 0 : 24
    readonly property int easeOut: Easing.OutCubic
    readonly property int easeIn: Easing.InCubic
    readonly property real springStiffness: 4.5
    readonly property real springDamping: 0.32

    // ---- Elevation (floating layers only) ---------------------------------------------------------
    readonly property real blurPopover: reduceEffects ? 0 : 0.55
    readonly property real blurModal: reduceEffects ? 0 : 0.8
    readonly property real surfaceTintPopover: reduceEffects ? 1.0 : 0.86
    readonly property real surfaceTintModal: reduceEffects ? 1.0 : 0.82
}
