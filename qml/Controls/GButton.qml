pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Gomp

// Push button: primary (accent fill), secondary (raised), ghost (text only), danger.
T.Button {
    id: control
    property string variant: "secondary"
    property string size: "md"
    property string iconName
    property string trailingIcon
    property bool loading: false
    property color tint: variant === "danger" ? Theme.danger : Theme.accent

    implicitHeight: size === "sm" ? Theme.controlSm : size === "lg" ? Theme.controlLg : Theme.controlMd
    implicitWidth: Math.max(implicitHeight, row.implicitWidth + leftPadding + rightPadding)
    leftPadding: text.length ? (size === "sm" ? Theme.s2 + 2 : Theme.s3) : 0
    rightPadding: leftPadding
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    enabled: !loading
    Accessible.name: text
    Accessible.role: Accessible.Button

    readonly property color fg: !enabled && !loading ? Theme.textDisabled
                               : variant === "primary" ? Theme.onAccent
                               : variant === "danger" ? (hovered ? Theme.onAccent : Theme.danger)
                               : variant === "ghost" ? (hovered ? Theme.textPrimary : Theme.textSecondary)
                               : Theme.textPrimary

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight
        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.s2 - 2
            opacity: control.loading ? 0 : 1
            GIcon {
                name: control.iconName
                color: control.fg
                size: control.size === "sm" ? Theme.iconSm : Theme.iconMd - 2
                anchors.verticalCenter: parent.verticalCenter
            }
            GText {
                text: control.text
                role: "strong"
                color: control.fg
                visible: text.length > 0
                anchors.verticalCenter: parent.verticalCenter
                lineHeight: 1.0
            }
            GIcon {
                name: control.trailingIcon
                color: control.fg
                size: Theme.iconSm
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        // Loading: three dots that step, not a spinner.
        Row {
            anchors.centerIn: parent
            spacing: 4
            visible: control.loading
            Repeater {
                model: 3
                Rectangle {
                    id: dot
                    required property int index
                    width: 5; height: 5; radius: 2.5
                    color: control.fg
                    opacity: 0.35
                    SequentialAnimation on opacity {
                        running: control.loading && !Theme.reduceMotion
                        loops: Animation.Infinite
                        PauseAnimation { duration: dot.index * 140 }
                        NumberAnimation { to: 1; duration: 220 }
                        NumberAnimation { to: 0.35; duration: 220 }
                        PauseAnimation { duration: (2 - dot.index) * 140 + 200 }
                    }
                }
            }
        }
    }

    background: Rectangle {
        radius: Theme.rMd
        color: {
            if (control.variant === "primary")
                return !control.enabled ? Theme.surface2 : control.down ? Theme.accentPressed : control.hovered ? Theme.accentHover : Theme.accent
            if (control.variant === "danger")
                return control.down ? Qt.darker(Theme.danger, 1.15) : control.hovered ? Theme.danger : Theme.alpha(Theme.danger, 0.1)
            if (control.variant === "ghost")
                return control.down ? Theme.surface3 : control.hovered ? Theme.surface2 : "transparent"
            return control.down ? Theme.surface3 : control.hovered ? Theme.surface2 : Theme.surface1
        }
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: control.hovered ? Theme.hairlineStrong : Theme.hairline
        Behavior on color { ColorAnimation { duration: Theme.dFast } }
        // Inner top highlight gives raised buttons their edge.
        Rectangle {
            visible: control.variant === "secondary" || control.variant === "primary"
            anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
            height: 1
            radius: Theme.rMd
            color: control.variant === "primary" ? Qt.rgba(1, 1, 1, 0.22) : Theme.innerHighlight
        }
        GFocusRing { target: control; radius_: Theme.rMd }
    }
}
