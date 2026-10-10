pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// One omp setting, rendered from its schema: label, description, the right control for its type,
// override and unsaved markers, and reset to default. Used by every generic settings list.
Item {
    id: row
    required property string settingId
    required property string label
    required property string description
    required property string type
    required property var value
    required property string valueText
    required property var defaultValue
    required property bool isDefault
    required property bool staged
    required property bool overridden
    required property var options
    property string error: ""
    readonly property bool stacked: width < 720
    implicitHeight: layout.implicitHeight + Theme.s4 * 2

    function commit(v) {
        error = App.settings.setValue(settingId, v)
    }

    Rectangle {
        anchors.fill: parent
        radius: Theme.rMd
        color: hover.hovered ? Theme.alpha(Theme.surface2, 0.6) : "transparent"
        HoverHandler { id: hover }
    }

    GridLayout {
        id: layout
        x: Theme.s4
        y: Theme.s4
        width: parent.width - Theme.s4 * 2
        columns: row.stacked ? 1 : 2
        columnSpacing: Theme.s6
        rowSpacing: Theme.s3

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            spacing: 3
            RowLayout {
                spacing: Theme.s2
                GText { text: row.label; role: "strong" }
                GBadge {
                    visible: row.overridden
                    text: App.settings.scope === "project" ? qsTr("Set for this project") : qsTr("Overridden in this project")
                    tone: "plan"
                    iconName: "folder"
                }
                Row {
                    visible: row.staged
                    spacing: 4
                    GStatusLight { tint: Theme.caution; diameter: 6; anchors.verticalCenter: parent.verticalCenter }
                    GText { text: qsTr("Unsaved"); role: "caption"; tone: "caution"; anchors.verticalCenter: parent.verticalCenter }
                }
            }
            GText {
                Layout.fillWidth: true
                text: row.description.length ? row.description : qsTr("omp setting %1").arg(row.settingId)
                tone: row.description.length ? "secondary" : "tertiary"
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                maximumLineCount: 4
            }
            GText {
                visible: row.error.length > 0
                text: row.error
                role: "caption"
                tone: "danger"
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
            }
        }

        // Controls share one right-aligned column so every row lines up.
        RowLayout {
            Layout.alignment: row.stacked ? Qt.AlignLeft : (Qt.AlignRight | Qt.AlignTop)
            Layout.preferredWidth: row.stacked ? -1 : 340
            Layout.maximumWidth: row.stacked ? -1 : 340
            Layout.fillWidth: false
            spacing: Theme.s2
            Item { Layout.fillWidth: true; visible: !row.stacked }

            Loader {
                Layout.fillWidth: row.type === "string" || row.type === "number" || row.type === "array" || row.type === "record"
                sourceComponent: row.type === "boolean" ? toggleComp
                               : row.type === "enum" && row.options.length > 0 && row.options.length <= 3
                                 && row.options.reduce((n, o) => n + o.label.length, 0) < 26 ? segmentComp
                               : row.type === "enum" ? dropdownComp
                               : row.type === "number" ? numberComp
                               : row.type === "array" || row.type === "record" ? jsonComp
                               : textComp
            }
            GIconButton {
                size: Theme.controlSm
                iconName: "undo"
                tooltip: row.staged ? qsTr("Undo this change") : App.settings.scope === "project" && row.overridden ? qsTr("Remove the project override") : qsTr("Reset to default (%1)").arg(String(row.defaultValue))
                opacity: !row.isDefault || row.staged ? 1 : 0
                enabled: !row.isDefault || row.staged
                onClicked: {
                    row.error = ""
                    if (row.staged) App.settings.unstage(row.settingId)
                    else App.settings.resetValue(row.settingId)
                }
            }
        }
    }

    Component {
        id: toggleComp
        GToggle {
            checked: row.value === true
            Accessible.name: row.label
            onToggled: row.commit(checked)
        }
    }
    Component {
        id: segmentComp
        GSegmented {
            size: "sm"
            value: String(row.value)
            options: row.options.map(o => ({ value: String(o.value), label: o.label }))
            Accessible.name: row.label
            onActivated: v => row.commit(v)
        }
    }
    Component {
        id: dropdownComp
        GDropdown {
            implicitWidth: 240
            value: String(row.value)
            options: row.options.map(o => ({ value: String(o.value), label: o.label, description: o.description || "" }))
            Accessible.name: row.label
            onPicked: v => row.commit(v)
        }
    }
    Component {
        id: numberComp
        GTextField {
            text: row.valueText
            invalid: row.error.length > 0
            horizontalAlignment: TextInput.AlignRight
            font: Theme.fReadout
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            Accessible.name: row.label
            onEditingFinished: if (text !== row.valueText) row.commit(Number(text))
        }
    }
    Component {
        id: textComp
        GTextField {
            text: row.valueText
            placeholderText: qsTr("Not set")
            invalid: row.error.length > 0
            Accessible.name: row.label
            onEditingFinished: if (text !== row.valueText) row.commit(text)
        }
    }
    Component {
        id: jsonComp
        RowLayout {
            spacing: Theme.s2
            GText {
                Layout.fillWidth: true
                text: Array.isArray(row.value) ? (row.value.length ? row.value.join(", ") : qsTr("Empty list"))
                      : row.value && typeof row.value === "object" ? (Object.keys(row.value).length ? Object.keys(row.value).map(k => k + ": " + row.value[k]).join(", ") : qsTr("No entries"))
                      : qsTr("Not set")
                tone: "secondary"
                font: Theme.fCodeSmall
            }
            GButton {
                text: qsTr("Edit")
                size: "sm"
                onClicked: jsonSheet.open()
            }
            JsonSheet {
                id: jsonSheet
                settingId: row.settingId
                label: row.label
                kind: row.type
                text: row.valueText
            }
        }
    }
}
