pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import Gomp
import Gomp.Core

// Custom titlebar: logo, workspace tabs, activity Corona, palette launcher, window controls. Dragging moves the
// window through the compositor (startSystemMove), so Wayland snapping and tiling keep working.
Rectangle {
    id: bar
    property bool frameless: true
    implicitHeight: Theme.titlebarHeight
    color: Theme.surface0
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.hairline }

    // Empty titlebar space moves the window; double-click toggles maximise.
    DragHandler {
        target: null
        enabled: bar.frameless
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        onActiveChanged: if (active) bar.Window.window.startSystemMove()
    }
    TapHandler {
        enabled: bar.frameless
        gesturePolicy: TapHandler.WithinBounds
        onDoubleTapped: bar.toggleMaximized()
    }
    function toggleMaximized() {
        const w = bar.Window.window
        if (w.visibility === Window.Maximized)
            w.showNormal()
        else
            w.showMaximized()
    }

    FolderDialog {
        id: folderDialog
        title: qsTr("Open a project")
        currentFolder: "file://" + App.homePath()
        onAccepted: App.openProject(selectedFolder.toString())
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.s3
        anchors.rightMargin: bar.frameless ? Theme.s1 : Theme.s3
        spacing: Theme.s2

        Item {
            implicitWidth: Theme.railWidth - Theme.s3 * 2 + 4
            implicitHeight: parent.height
            GLogo {
                anchors.centerIn: parent
                size: 20
            }
        }

        WorkspaceTabs {
            Layout.fillHeight: true
            Layout.maximumWidth: bar.width * 0.62
            onOpenRequested: folderDialog.open()
        }

        Item { Layout.fillWidth: true; Layout.fillHeight: true }

        // The current workspace's activity, always visible whatever screen is open.
        Corona {
            Layout.rightMargin: Theme.s2
            size: 20
            visible: App.workspaces.current !== null
            workspace: App.workspaces.current
        }

        // Palette launcher: looks like a field because it opens one.
        Rectangle {
            id: launcher
            Layout.preferredWidth: Math.min(320, bar.width * 0.24)
            Layout.preferredHeight: Theme.controlSm
            radius: Theme.rMd
            color: launcherHover.hovered ? Theme.surface2 : Theme.surface1
            border.width: 1
            border.color: Theme.hairline
            visible: bar.width > 900
            Accessible.role: Accessible.Button
            Accessible.name: qsTr("Search or run a command")
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.s2 + 2
                anchors.rightMargin: Theme.s1 + 2
                spacing: Theme.s2
                GIcon { name: "search"; size: Theme.iconSm; color: Theme.textTertiary }
                GText { text: qsTr("Search or run a command"); tone: "tertiary"; Layout.fillWidth: true; lineHeight: 1.0 }
                GKbd { sequence: App.shortcuts.map["palette.open"] || "" }
            }
            HoverHandler { id: launcherHover; cursorShape: Qt.PointingHandCursor }
            TapHandler { onTapped: Ui.paletteOpen = true }
        }
        GIconButton {
            visible: !launcher.visible
            iconName: "search"
            tooltip: qsTr("Search or run a command")
            shortcutText: App.shortcuts.map["palette.open"] || ""
            onClicked: Ui.paletteOpen = true
        }

        Row {
            visible: bar.frameless
            spacing: 2
            Layout.leftMargin: Theme.s2
            GIconButton {
                iconName: "minimize"; tooltip: qsTr("Minimise"); size: Theme.controlSm + 4
                onClicked: bar.Window.window.showMinimized()
            }
            GIconButton {
                iconName: bar.Window.window && bar.Window.window.visibility === Window.Maximized ? "restore" : "maximize"
                tooltip: bar.Window.window && bar.Window.window.visibility === Window.Maximized ? qsTr("Restore") : qsTr("Maximise")
                size: Theme.controlSm + 4
                onClicked: bar.toggleMaximized()
            }
            GIconButton {
                id: closeButton
                iconName: "close"; tooltip: qsTr("Close"); size: Theme.controlSm + 4
                iconColor: hovered ? Theme.onAccent : Theme.textSecondary
                onClicked: bar.Window.window.close()
                background: Rectangle {
                    radius: Theme.rMd
                    color: closeButton.hovered ? Theme.danger : "transparent"
                }
            }
        }
    }
}
