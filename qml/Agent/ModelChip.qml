pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// The current model and reasoning effort; opens the model picker.
GButton {
    id: chip
    property Workspace workspace: null
    text: workspace && workspace.modelName ? workspace.modelName : qsTr("Model")
    iconName: "chip"
    trailingIcon: "chevron-down"
    size: "md"
    onClicked: picker.open()
    GToolTip {
        visible: chip.hovered && !picker.visible
        text: chip.workspace ? chip.workspace.modelSelector + (chip.workspace.thinkingLevel ? qsTr(", effort %1").arg(chip.workspace.thinkingLevel) : "") : ""
        shortcut: App.shortcuts.map["model.pick"] || ""
    }
    ModelPicker {
        id: picker
        workspace: chip.workspace
        y: chip.height + Theme.s1
        x: chip.width - width
    }
    Connections {
        target: Ui
        function onOpenOverlay(name, props) { if (name === "model" && chip.visible) picker.open() }
    }
    Shortcut {
        sequences: App.shortcuts.map["model.pick"] ? [App.shortcuts.map["model.pick"]] : []
        enabled: chip.visible && chip.workspace !== null
        onActivated: picker.open()
    }
}
