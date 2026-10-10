pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// Unsaved changes. Rises from the bottom while edits are staged; applying always goes through
// the diff review.
Item {
    id: bar
    readonly property int staged: App.settings.stagedCount
    implicitHeight: staged > 0 ? 64 : 0
    clip: true
    Behavior on implicitHeight { NumberAnimation { duration: Theme.dBase; easing.type: Theme.easeOut } }

    Rectangle {
        anchors.fill: parent
        anchors.margins: Theme.s3
        anchors.leftMargin: Theme.s6
        anchors.rightMargin: Theme.s5
        radius: Theme.rLg
        color: Theme.mix(Theme.surface1, Theme.caution, Theme.dark ? 0.06 : 0.08)
        border.width: 1
        border.color: Theme.alpha(Theme.caution, 0.45)
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.s4
            anchors.rightMargin: Theme.s2
            spacing: Theme.s3
            GIcon { name: "pencil"; color: Theme.caution; size: Theme.iconSm + 2 }
            GText {
                Layout.fillWidth: true
                text: Fmt.count(bar.staged, qsTr("change"), qsTr("changes")) + qsTr(" not saved yet")
                role: "strong"
            }
            GButton { text: qsTr("Discard"); variant: "ghost"; size: "sm"; onClicked: App.settings.discard() }
            GButton { text: qsTr("Review and save"); variant: "primary"; size: "sm"; iconName: "diff"; onClicked: review.open() }
        }
    }
    ReviewSheet { id: review }
    Connections {
        target: Ui
        function onOpenOverlay(name, props) {
            if (name === "reviewSettings") review.open()
            else if (name === "closeReview") review.close()
        }
    }
}
