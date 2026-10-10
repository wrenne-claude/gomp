pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// Text with omp's working shimmer: a band of the accent sweeps across dim text, at omp's pace of
// 30 characters a second with a 20-character rest between passes. With reduced motion or effects
// the text is static in the accent.
GText {
    id: label
    property bool running: true
    property color highColor: Theme.accent
    readonly property bool animated: running && visible && !Theme.reduceMotion && !Theme.reduceEffects
    readonly property real charWidth: text.length ? contentWidth / text.length : 1
    property real t: 0

    color: animated ? "white" : highColor
    layer.enabled: animated
    layer.effect: ShaderEffect {
        readonly property real cycle: label.text.length + 20
        property real sweep: ((label.t * 30) % cycle - 10) * label.charWidth
        property real halfWidth: 6 * label.charWidth
        property real span: label.width
        property color low: Theme.textTertiary
        property color mid: Theme.textSecondary
        property color high: label.highColor
        fragmentShader: "qrc:/qt/qml/Gomp/shaders/shimmer.frag.qsb"
    }
    FrameAnimation {
        running: label.animated
        onTriggered: label.t += frameTime
    }
}
