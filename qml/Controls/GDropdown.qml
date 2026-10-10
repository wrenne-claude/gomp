pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Gomp

// Select from a list. `options` is [{value, label, description?}]. Lists longer than eight get a
// filter field. The popup is a frosted elevation-2 surface.
T.ComboBox {
    id: control
    property var options: []
    property var value
    property string placeholder: qsTr("Choose…")
    signal picked(var value)

    readonly property int selectedIndex: {
        for (let i = 0; i < options.length; ++i)
            if (String(options[i].value) === String(value))
                return i
        return -1
    }
    readonly property string selectedLabel: selectedIndex >= 0 ? options[selectedIndex].label : (value !== undefined && value !== null && String(value).length ? String(value) : "")
    property string filter: ""
    readonly property var filtered: filter.length === 0 ? options
        : options.filter(o => (o.label + " " + (o.description || "") + " " + o.value).toLowerCase().indexOf(filter.toLowerCase()) >= 0)

    implicitWidth: 220
    implicitHeight: Theme.controlMd
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    model: filtered
    textRole: "label"
    leftPadding: Theme.s3
    rightPadding: Theme.s6
    Accessible.role: Accessible.ComboBox
    Accessible.name: selectedLabel

    onActivated: index => {
        const o = filtered[index]
        if (o) control.picked(o.value)
    }

    contentItem: GText {
        text: control.selectedLabel.length ? control.selectedLabel : control.placeholder
        color: control.selectedLabel.length ? (control.enabled ? Theme.textPrimary : Theme.textDisabled) : Theme.textTertiary
        verticalAlignment: Text.AlignVCenter
        lineHeight: 1.0
    }
    indicator: GIcon {
        x: control.width - width - Theme.s2 - 2
        y: (control.height - height) / 2
        name: "chevron-down"
        size: Theme.iconSm
        color: Theme.textTertiary
        rotation: control.popup.visible ? 180 : 0
        Behavior on rotation { NumberAnimation { duration: Theme.dBase; easing.type: Theme.easeOut } }
    }
    background: Rectangle {
        radius: Theme.rMd
        color: control.down ? Theme.surface2 : Theme.surface1
        border.width: control.visualFocus ? 2 : 1
        border.color: control.visualFocus ? Theme.accent : control.hovered ? Theme.hairlineStrong : Theme.hairline
    }
    delegate: T.ItemDelegate {
        id: item
        required property var modelData
        required property int index
        width: ListView.view ? ListView.view.width : 0
        implicitHeight: item.modelData.description ? 48 : Theme.controlMd
        hoverEnabled: true
        highlighted: control.highlightedIndex === index
        leftPadding: Theme.s3
        rightPadding: Theme.s3
        contentItem: Column {
            spacing: 2
            GText {
                width: parent.width - Theme.s5
                text: item.modelData.label
                role: "body"
                lineHeight: 1.0
                color: Theme.textPrimary
            }
            GText {
                width: parent.width
                text: item.modelData.description || ""
                visible: text.length > 0
                role: "caption"
                tone: "tertiary"
            }
        }
        background: Rectangle {
            radius: Theme.rSm + 2
            color: item.highlighted || item.hovered ? Theme.surface2 : "transparent"
            GIcon {
                anchors.right: parent.right
                anchors.rightMargin: Theme.s2
                anchors.verticalCenter: parent.verticalCenter
                name: "check"
                size: Theme.iconSm
                color: Theme.accent
                visible: String(item.modelData.value) === String(control.value)
            }
        }
    }
    popup: T.Popup {
        y: control.height + Theme.s1
        width: Math.max(control.width, 260)
        implicitHeight: Math.min(contentItem.implicitHeight + topPadding + bottomPadding, 360)
        padding: Theme.s1
        onOpened: { control.filter = ""; if (filterField.visible) filterField.forceActiveFocus() }
        enter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dFast; easing.type: Theme.easeOut }
                NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: Theme.dBase; easing.type: Theme.easeOut }
            }
        }
        exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: Theme.dFast * 0.75 } }
        contentItem: Column {
            spacing: Theme.s1
            GSearchField {
                id: filterField
                width: parent.width
                visible: control.options.length > 8
                placeholderText: qsTr("Filter")
                onTextChanged: control.filter = text
                Keys.onDownPressed: list.forceActiveFocus()
            }
            ListView {
                id: list
                width: parent.width
                height: Math.min(contentHeight, 320 - (filterField.visible ? filterField.height : 0))
                clip: true
                model: control.delegateModel
                currentIndex: control.highlightedIndex
                boundsBehavior: Flickable.StopAtBounds
                T.ScrollBar.vertical: GScrollBar {}
            }
        }
        background: GFloating {
            level: 2
            sourceItem: control.popup.contentItem
        }
    }
}
