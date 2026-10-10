import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// gomp's own look and behaviour, plus omp's terminal theme for when you use omp directly.
Item {
    id: page
    readonly property string title: qsTr("Appearance")
    readonly property string subtitle: qsTr("How gomp looks and moves. These settings belong to gomp and do not change omp.")

    component Row_: RowLayout {
        property string label
        property string help
        default property alias control: slot.data
        Layout.fillWidth: true
        spacing: Theme.s6
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            GText { text: parent.parent.label; role: "strong"; Layout.fillWidth: true }
            GText { text: parent.parent.help; tone: "secondary"; wrapMode: Text.WordWrap; elide: Text.ElideNone; Layout.fillWidth: true; visible: text.length > 0 }
        }
        Row { id: slot; spacing: Theme.s2 }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: col.implicitHeight + Theme.s6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: GScrollBar {}
        ColumnLayout {
            id: col
            x: Theme.s6
            width: Math.min(flick.width - Theme.s6 * 2, 900)
            spacing: Theme.s5

            Row_ {
                label: qsTr("Theme")
                help: qsTr("System follows your desktop's light or dark setting.")
                GSegmented {
                    value: App.prefs.themeMode
                    options: [{ value: "system", label: qsTr("System"), icon: "monitor" }, { value: "dark", label: qsTr("Dark"), icon: "moon" }, { value: "light", label: qsTr("Light"), icon: "sun" }]
                    onActivated: v => App.prefs.themeMode = v
                }
            }
            Row_ {
                label: qsTr("Accent")
                help: qsTr("Colours buttons, focus and the agent's light in Build mode. Plan and Goal keep their own light.")
                Repeater {
                    model: [
                        { value: "sodium", label: qsTr("Sodium"), color: Theme.sodium },
                        { value: "blueprint", label: qsTr("Blueprint"), color: Theme.blueprint },
                        { value: "lock", label: qsTr("Lock"), color: Theme.lock },
                        { value: "ion", label: qsTr("Ion"), color: Theme.ion },
                        { value: "rose", label: qsTr("Rose"), color: Theme.rose },
                        { value: "system", label: qsTr("Desktop accent"), color: App.systemAccent }
                    ]
                    T.AbstractButton {
                        id: swatch
                        required property var modelData
                        readonly property bool chosen: App.prefs.accent === modelData.value
                        width: 32
                        height: 32
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: modelData.label
                        Accessible.role: Accessible.RadioButton
                        Accessible.checked: chosen
                        onClicked: App.prefs.accent = modelData.value
                        background: Rectangle {
                            radius: 16
                            color: swatch.modelData.color
                            border.width: swatch.chosen ? 3 : 0
                            border.color: Theme.hull
                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: -3
                                radius: 19
                                color: "transparent"
                                border.width: 2
                                border.color: swatch.chosen || swatch.visualFocus ? swatch.modelData.color : "transparent"
                            }
                            GIcon { anchors.centerIn: parent; name: "monitor"; size: 14; color: Theme.onAccent; visible: swatch.modelData.value === "system" }
                        }
                        GToolTip { visible: swatch.hovered; text: swatch.modelData.label }
                    }
                }
            }
            GDivider { Layout.fillWidth: true }
            Row_ {
                label: qsTr("Visual effects")
                help: App.effects.softwareRendering
                      ? qsTr("Automatic has turned blur, shaders and particles off: this machine renders in software (%1).").arg(App.effects.renderer || App.effects.graphicsApi)
                      : qsTr("Blur, the activity visualisation and ambient light. Automatic turns them off on software rendering. Renderer: %1.").arg(App.effects.renderer || App.effects.graphicsApi || qsTr("detecting"))
                GSegmented {
                    value: App.prefs.effects
                    options: [{ value: "auto", label: qsTr("Automatic") }, { value: "full", label: qsTr("On") }, { value: "reduced", label: qsTr("Reduced") }]
                    onActivated: v => App.prefs.effects = v
                }
            }
            Row_ {
                label: qsTr("Reduce motion")
                help: qsTr("Replaces movement with quick fades and freezes the activity visualisation.")
                GToggle { checked: App.prefs.reduceMotion; onToggled: App.prefs.reduceMotion = checked; Accessible.name: qsTr("Reduce motion") }
            }
            Row_ {
                label: qsTr("Density")
                help: qsTr("Compact tightens vertical spacing. Automatic uses it on short displays such as 2560 by 720.")
                GSegmented {
                    value: App.prefs.density
                    options: [{ value: "auto", label: qsTr("Automatic") }, { value: "comfortable", label: qsTr("Comfortable") }, { value: "compact", label: qsTr("Compact") }]
                    onActivated: v => App.prefs.density = v
                }
            }
            Row_ {
                label: qsTr("gomp title bar")
                help: qsTr("Off uses your window manager's title bar. Takes effect after a restart.")
                GToggle { checked: App.prefs.customTitlebar; onToggled: App.prefs.customTitlebar = checked; Accessible.name: qsTr("gomp title bar") }
            }
            GDivider { Layout.fillWidth: true }
            Row_ {
                label: qsTr("Desktop notifications")
                help: qsTr("When gomp is in the background: approvals, questions and plans always notify; finished turns notify after 20 seconds.")
                GToggle { checked: App.prefs.notifications; onToggled: App.prefs.notifications = checked; Accessible.name: qsTr("Desktop notifications") }
            }
            Row_ {
                label: qsTr("Notify when long turns finish")
                help: ""
                GToggle { enabled: App.prefs.notifications; checked: App.prefs.notifyOnFinish; onToggled: App.prefs.notifyOnFinish = checked; Accessible.name: qsTr("Notify when long turns finish") }
            }
            GDivider { Layout.fillWidth: true }
            GText { text: qsTr("omp in the terminal"); role: "heading" }
            GText {
                text: qsTr("omp's own terminal theme and glyphs, used when you run omp outside gomp.")
                tone: "tertiary"
                role: "caption"
            }
            GButton {
                text: qsTr("Open omp's terminal UI settings")
                iconName: "terminal"
                onClicked: Ui.openSettings("tab:appearance")
            }
        }
    }
}
