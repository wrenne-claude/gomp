pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T
import Qt.labs.qmlmodels
import Gomp
import Gomp.Core

// The transcript. Follows the stream while you are at the bottom; scrolling up pauses that and
// offers a jump back. Rows are virtualised and reused.
Item {
    id: root
    property Workspace workspace: null
    readonly property int columnWidth: Math.min(width - Theme.s6 * 2, Theme.proseMaxWidth + 160)

    ListView {
        id: list
        // The list is the reading column itself, centred; the scrollbar and wheel cover the full width.
        width: root.columnWidth
        height: parent.height
        anchors.horizontalCenter: parent.horizontalCenter
        clip: true
        model: root.workspace ? root.workspace.conversation : null
        spacing: Theme.compact ? Theme.s3 : Theme.s4
        topMargin: Theme.s5
        bottomMargin: Theme.s5
        reuseItems: true
        cacheBuffer: 1200
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 4000
        T.ScrollBar.vertical: vbar

        property bool follow: true
        onMovementStarted: follow = false
        onMovementEnded: follow = atYEnd
        onContentHeightChanged: if (follow) Qt.callLater(positionViewAtEnd)
        onCountChanged: if (follow) Qt.callLater(positionViewAtEnd)
        Connections {
            target: root
            function onWorkspaceChanged() { list.follow = true; Qt.callLater(list.positionViewAtEnd) }
        }

        // New rows rise in; history loads (model resets) appear without animation.
        add: Transition {
            enabled: !Theme.reduceMotion
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.dBase; easing.type: Theme.easeOut }
        }

        delegate: DelegateChooser {
            role: "kind"
            DelegateChoice { roleValue: ConversationModel.User; delegate: UserMessage { width: root.columnWidth; workspace: root.workspace } }
            DelegateChoice { roleValue: ConversationModel.Assistant; delegate: AssistantMessage { width: root.columnWidth } }
            DelegateChoice { roleValue: ConversationModel.Thinking; delegate: ThinkingBlock { width: root.columnWidth } }
            DelegateChoice { roleValue: ConversationModel.Tool; delegate: ToolCard { width: root.columnWidth; workspace: root.workspace } }
            DelegateChoice { roleValue: ConversationModel.Notice; delegate: NoticeRow { width: root.columnWidth } }
            DelegateChoice { roleValue: ConversationModel.Error; delegate: ErrorRow { width: root.columnWidth; workspace: root.workspace } }
            DelegateChoice { roleValue: ConversationModel.Compaction; delegate: CompactionRow { width: root.columnWidth } }
            DelegateChoice { roleValue: ConversationModel.CommandOutput; delegate: CommandOutputRow { width: root.columnWidth } }
        }

        footer: Item {
            width: root.columnWidth
            height: root.workspace && root.workspace.busy ? 40 : 0
            WorkingLine {
                width: root.columnWidth
                anchors.verticalCenter: parent.verticalCenter
                workspace: root.workspace
                visible: parent.height > 0
            }
        }
    }

    GScrollBar {
        id: vbar
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
    }
    // Scrolling over the margins beside the column moves the transcript too.
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: event => {
            list.follow = false
            list.contentY = Math.max(list.originY - list.topMargin,
                                     Math.min(list.contentY - event.angleDelta.y,
                                              list.originY + list.contentHeight + list.bottomMargin - list.height))
            if (list.atYEnd) list.follow = true
        }
    }

    ConversationEmpty {
        anchors.centerIn: parent
        visible: root.workspace !== null && list.count === 0
        workspace: root.workspace
    }

    GButton {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.s3
        visible: !list.follow && !list.atYEnd && list.count > 0
        text: root.workspace && root.workspace.busy ? qsTr("Follow live output") : qsTr("Jump to latest")
        iconName: "chevron-down"
        size: "sm"
        onClicked: { list.follow = true; list.positionViewAtEnd() }
    }
}
