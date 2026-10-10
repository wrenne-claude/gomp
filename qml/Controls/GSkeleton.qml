import QtQuick
import Gomp

// Loading placeholder with a slow sheen (static when Reduce motion is on).
Rectangle {
    id: sk
    radius: Theme.rSm
    color: Theme.surface2
    clip: true
    Rectangle {
        width: parent.width * 0.6
        height: parent.height
        visible: !Theme.reduceMotion
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 0.5; color: Theme.alpha(Theme.textPrimary, Theme.dark ? 0.05 : 0.5) }
            GradientStop { position: 1; color: "transparent" }
        }
        NumberAnimation on x {
            from: -sk.width * 0.6
            to: sk.width
            duration: 1400
            loops: Animation.Infinite
            running: sk.visible && !Theme.reduceMotion
        }
    }
}
