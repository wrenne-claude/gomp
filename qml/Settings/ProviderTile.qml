pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// One sign-in as a square tile: the provider's logo (resources/providers/<id>.svg|png), or its
// initials when no logo has been added yet. The name is the tooltip and the accessible name.
T.AbstractButton {
    id: tile
    required property var provider            // { id, name, available, authenticated }
    property bool busy: false                  // this tile's login is in progress
    readonly property bool connected: provider.authenticated
    readonly property string logo: App.providerLogo(provider.id)
    readonly property string label: provider.name + (connected ? qsTr(" (connected)") : "")

    implicitWidth: 56
    implicitHeight: 56
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: label
    Accessible.role: Accessible.Button

    // "Anthropic (Claude Pro/Max)" -> "A", "Google Cloud Code Assist" -> "GC"
    readonly property string initials: {
        const words = provider.name.replace(/\(.*?\)/g, "").trim().split(/[\s\/&.-]+/).filter(w => w.length > 0)
        if (words.length === 0) return "?"
        return (words.length === 1 ? words[0].substring(0, 2) : words[0][0] + words[1][0]).toUpperCase()
    }

    contentItem: Item {
        Image {
            anchors.centerIn: parent
            width: 30
            height: 30
            visible: tile.logo.length > 0
            // Single-tone (all black or all white) logos are tinted to the theme's text colour;
            // coloured logos are drawn as supplied.
            source: tile.logo.length > 0 ? "image://providerlogo/" + tile.provider.id + "/" + Theme.textPrimary.toString() : ""
            sourceSize: Qt.size(Math.ceil(30 * Screen.devicePixelRatio), Math.ceil(30 * Screen.devicePixelRatio))
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            opacity: tile.enabled || tile.busy ? (tile.busy ? 0.35 : 1) : 0.4
        }
        GText {
            anchors.centerIn: parent
            visible: tile.logo.length === 0
            text: tile.initials
            role: "heading"
            color: tile.enabled ? Theme.textSecondary : Theme.textDisabled
        }
        // Connected: a green tick in the corner, so state never relies on colour alone.
        Rectangle {
            visible: tile.connected
            x: parent.width - width - 4
            y: 4
            width: 16
            height: 16
            radius: 8
            color: Theme.lock
            GIcon { anchors.centerIn: parent; name: "check"; size: 12; color: Theme.onAccent }
        }
        Row {
            visible: tile.busy
            anchors.centerIn: parent
            spacing: 4
            Repeater {
                model: 3
                Rectangle {
                    id: dot
                    required property int index
                    width: 6; height: 6; radius: 3
                    color: Theme.accent
                    opacity: 0.35
                    SequentialAnimation on opacity {
                        running: tile.busy && !Theme.reduceMotion
                        loops: Animation.Infinite
                        PauseAnimation { duration: dot.index * 120 }
                        NumberAnimation { to: 1; duration: 240 }
                        NumberAnimation { to: 0.35; duration: 240 }
                        PauseAnimation { duration: (2 - dot.index) * 120 }
                    }
                }
            }
        }
    }
    background: Rectangle {
        radius: Theme.rLg
        color: tile.down ? Theme.surface3 : tile.hovered ? Theme.surface2 : Theme.surface1
        border.width: 1
        border.color: tile.connected ? Theme.alpha(Theme.lock, 0.7) : tile.hovered ? Theme.hairlineStrong : Theme.hairline
        Behavior on color { ColorAnimation { duration: Theme.dFast } }
        GFocusRing { target: tile }
    }
    GToolTip {
        visible: tile.hovered && !tile.down
        text: tile.label
    }
}
