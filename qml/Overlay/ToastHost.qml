import QtQuick
import Gomp

// Transient messages, bottom right. At most three; each stays 4 s (errors 8 s) and pauses while
// hovered. Toasts never steal focus.
Item {
    id: host
    width: 380
    height: column.height

    ListModel { id: toasts }
    property int nextId: 1

    Connections {
        target: Ui
        function onToastRequested(level, message, actionText, action) {
            if (!message || !message.length)
                return
            // Collapse repeats of the newest toast instead of stacking duplicates.
            if (toasts.count > 0 && toasts.get(toasts.count - 1).message === message)
                return
            toasts.append({ uid: host.nextId++, level: level, message: message, actionText: actionText })
            host.actions[host.nextId - 1] = action
            while (toasts.count > 3)
                toasts.remove(0)
        }
    }
    property var actions: ({})

    Column {
        id: column
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: parent.width
        spacing: Theme.s2
        add: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dBase; easing.type: Theme.easeOut }
                NumberAnimation { property: "x"; from: 24; duration: Theme.dSlow; easing.type: Theme.easeOut }
            }
        }
        move: Transition { NumberAnimation { properties: "y"; duration: Theme.dBase; easing.type: Theme.easeOut } }

        Repeater {
            model: toasts
            delegate: Item {
                id: toast
                required property int index
                required property int uid
                required property string level
                required property string message
                required property string actionText
                width: column.width
                height: card.height
                readonly property color tint: level === "error" ? Theme.danger : level === "warning" ? Theme.caution : level === "success" ? Theme.success : Theme.info

                Timer {
                    interval: toast.level === "error" ? 8000 : 4000
                    running: !hover.hovered
                    onTriggered: toasts.remove(toast.index)
                }
                GFloating { anchors.fill: card; level: 2; radius: Theme.rMd + 2 }
                Item {
                    id: card
                    width: parent.width
                    height: Math.max(44, body.implicitHeight + Theme.s3 * 2)
                    Rectangle {
                        x: Theme.s3
                        anchors.verticalCenter: parent.verticalCenter
                        width: 3
                        height: parent.height - Theme.s4
                        radius: 1.5
                        color: toast.tint
                    }
                    GIcon {
                        id: icon
                        x: Theme.s3 + Theme.s3
                        y: Theme.s3
                        name: toast.level === "error" ? "x-circle" : toast.level === "warning" ? "alert" : toast.level === "success" ? "check-circle" : "info"
                        size: Theme.iconMd - 2
                        color: toast.tint
                    }
                    GText {
                        id: body
                        anchors.left: icon.right
                        anchors.leftMargin: Theme.s2 + 2
                        anchors.right: actionButton.visible ? actionButton.left : closeButton.left
                        anchors.rightMargin: Theme.s2
                        y: Theme.s3
                        text: toast.message
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        maximumLineCount: 4
                    }
                    GButton {
                        id: actionButton
                        anchors.right: closeButton.left
                        anchors.verticalCenter: parent.verticalCenter
                        visible: toast.actionText.length > 0
                        text: toast.actionText
                        variant: "ghost"
                        size: "sm"
                        onClicked: {
                            const a = host.actions[toast.uid]
                            if (a) a()
                            toasts.remove(toast.index)
                        }
                    }
                    GIconButton {
                        id: closeButton
                        anchors.right: parent.right
                        anchors.rightMargin: Theme.s2
                        anchors.verticalCenter: parent.verticalCenter
                        size: Theme.controlSm
                        iconName: "close"
                        tooltip: qsTr("Dismiss")
                        focusPolicy: Qt.NoFocus
                        onClicked: toasts.remove(toast.index)
                    }
                    HoverHandler { id: hover }
                }
            }
        }
    }
}
