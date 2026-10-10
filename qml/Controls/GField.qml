pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp

// Labelled form field: label, optional description, the control (default content), error text.
ColumnLayout {
    id: field
    property string label
    property string description
    property string error
    default property alias control: slot.data
    spacing: Theme.s1 + 2
    GText { text: field.label; role: "strong"; visible: text.length > 0; Layout.fillWidth: true }
    GText {
        text: field.description
        role: "caption"
        tone: "tertiary"
        visible: text.length > 0
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        Layout.fillWidth: true
    }
    Item {
        id: slot
        Layout.fillWidth: true
        implicitHeight: childrenRect.height
        Layout.topMargin: 2
    }
    GText {
        text: field.error
        role: "caption"
        tone: "danger"
        visible: text.length > 0
        wrapMode: Text.WordWrap
        elide: Text.ElideNone
        Layout.fillWidth: true
    }
}
