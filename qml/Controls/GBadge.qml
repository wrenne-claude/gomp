import QtQuick
import Gomp

// Small status label. Tones map to semantic colours; never decoration.
Rectangle {
    id: badge
    property string text
    property string tone: "neutral" // neutral | accent | success | danger | caution | info | plan | goal
    property string iconName
    readonly property color toneColor: tone === "accent" ? Theme.accent
                                      : tone === "success" ? Theme.success
                                      : tone === "danger" ? Theme.danger
                                      : tone === "caution" ? Theme.caution
                                      : tone === "info" ? Theme.info
                                      : tone === "plan" ? Theme.blueprint
                                      : tone === "goal" ? Theme.lock
                                      : Theme.textSecondary
    implicitHeight: 20
    implicitWidth: row.implicitWidth + Theme.s2 * 2 - (iconName.length ? 2 : 0)
    radius: Theme.rSm
    color: tone === "neutral" ? Theme.surface2 : Theme.alpha(toneColor, Theme.dark ? 0.14 : 0.12)
    border.width: tone === "neutral" ? 1 : 0
    border.color: Theme.hairline
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 3
        GIcon {
            name: badge.iconName
            size: 12
            color: badge.toneColor
            anchors.verticalCenter: parent.verticalCenter
        }
        GText {
            text: badge.text
            role: "caption"
            color: badge.tone === "neutral" ? Theme.textSecondary : badge.toneColor
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
