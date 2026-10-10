pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// Text set in one of the type-scale roles with a semantic tone.
Text {
    property string role: "body"     // display | title | heading | body | strong | caption | readout | readoutLarge | code
    property string tone: "primary"  // primary | secondary | tertiary | accent | danger | caution | success | disabled | onAccent

    font: role === "display" ? Theme.fDisplay
        : role === "title" ? Theme.fTitle
        : role === "heading" ? Theme.fHeading
        : role === "strong" ? Theme.fBodyStrong
        : role === "caption" ? Theme.fCaption
        : role === "readout" ? Theme.fReadout
        : role === "readoutLarge" ? Theme.fReadoutLarge
        : role === "code" ? Theme.fCode
        : Theme.fBody
    color: tone === "secondary" ? Theme.textSecondary
         : tone === "tertiary" ? Theme.textTertiary
         : tone === "accent" ? Theme.accent
         : tone === "danger" ? Theme.danger
         : tone === "caution" ? Theme.caution
         : tone === "success" ? Theme.success
         : tone === "disabled" ? Theme.textDisabled
         : tone === "onAccent" ? Theme.onAccent
         : Theme.textPrimary
    // Body line height only matters when text wraps; single-line labels stay optically centred.
    lineHeight: (role === "body" || role === "strong") && wrapMode !== Text.NoWrap ? Theme.lhBody : 1.0
    textFormat: Text.PlainText
    renderType: Text.NativeRendering
    elide: Text.ElideRight
}
