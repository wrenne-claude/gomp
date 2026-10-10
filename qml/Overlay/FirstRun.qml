pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import Gomp
import Gomp.Core

// First run: find omp (or explain how to install it), then pick a first project. The demo
// path runs everything against the mock backend so the app can be explored without omp.
Rectangle {
    id: first
    color: Theme.hull
    Accessible.role: Accessible.Dialog
    Accessible.name: qsTr("Set up gomp")

    readonly property var omp: App.omp
    readonly property bool found: omp.status === OmpLocator.Found || omp.status === OmpLocator.Untested

    FileDialog {
        id: binaryDialog
        title: qsTr("Locate the omp binary")
        onAccepted: App.omp.useBinary(selectedFile.toString())
    }
    FolderDialog {
        id: projectDialog
        title: qsTr("Choose a project folder")
        onAccepted: {
            App.completeFirstRun()
            App.openProject(selectedFolder.toString())
        }
    }

    // A drag area so the frameless window can still be moved while setting up.
    DragHandler { target: null; onActiveChanged: if (active) first.Window.window.startSystemMove() }

    RowLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.s7 * 2, 980)
        spacing: Theme.s7

        Item {
            Layout.preferredWidth: 220
            Layout.preferredHeight: 220
            Layout.alignment: Qt.AlignVCenter
            visible: first.width > 820
            Corona {
                anchors.centerIn: parent
                size: 200
                workspace: null
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.s4

            GText { text: qsTr("Set up gomp"); role: "display" }
            GText {
                text: qsTr("gomp runs the omp coding agent and shows what it is doing. It needs the omp binary on this machine.")
                tone: "secondary"
                wrapMode: Text.WordWrap
                elide: Text.ElideNone
                Layout.fillWidth: true
                Layout.maximumWidth: 560
            }

            GCard {
                Layout.fillWidth: true
                Layout.maximumWidth: 600
                Layout.topMargin: Theme.s2
                implicitHeight: stepOne.implicitHeight + Theme.s4 * 2
                ColumnLayout {
                    id: stepOne
                    anchors.fill: parent
                    anchors.margins: Theme.s4
                    spacing: Theme.s3
                    RowLayout {
                        spacing: Theme.s3
                        GStatusLight {
                            tint: first.found ? Theme.success : first.omp.status === OmpLocator.Checking ? Theme.caution : Theme.danger
                            pulse: first.omp.status === OmpLocator.Checking
                        }
                        GText {
                            role: "strong"
                            Layout.fillWidth: true
                            text: first.omp.status === OmpLocator.Checking ? qsTr("Looking for omp…")
                                : first.found ? qsTr("Found omp %1").arg(first.omp.version)
                                : first.omp.status === OmpLocator.Broken ? qsTr("omp did not start")
                                : qsTr("omp is not installed")
                        }
                        GText { text: Fmt.shortPath(first.omp.path); role: "code"; tone: "tertiary"; visible: first.found }
                    }
                    GText {
                        visible: first.omp.message.length > 0
                        text: first.omp.message
                        tone: first.omp.status === OmpLocator.Untested ? "caution" : "secondary"
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        visible: !first.found && first.omp.status !== OmpLocator.Checking
                        spacing: Theme.s2
                        Layout.fillWidth: true
                        GText { text: qsTr("Install it with your JavaScript package manager:"); tone: "secondary" }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: Theme.controlLg
                            radius: Theme.rMd
                            color: Theme.codeBg
                            border.width: 1
                            border.color: Theme.hairline
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: Theme.s3
                                anchors.rightMargin: Theme.s1
                                GText { text: first.omp.installCommand; role: "code"; Layout.fillWidth: true }
                                GIconButton {
                                    iconName: "copy"
                                    tooltip: qsTr("Copy command")
                                    onClicked: { App.copyText(first.omp.installCommand); Ui.toast("success", qsTr("Copied. Run it in a terminal, then check again.")) }
                                }
                            }
                        }
                        GText {
                            text: qsTr("Searched: %1").arg(first.omp.searchedPaths.map(p => Fmt.shortPath(p)).join(", "))
                            role: "caption"
                            tone: "tertiary"
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                            Layout.fillWidth: true
                        }
                    }
                    RowLayout {
                        spacing: Theme.s2
                        GButton { text: qsTr("Check again"); iconName: "refresh"; onClicked: App.omp.locate(App.prefs.ompPath) }
                        GButton { text: qsTr("Locate omp…"); iconName: "folder-open"; variant: "ghost"; onClicked: binaryDialog.open() }
                    }
                }
            }

            RowLayout {
                Layout.topMargin: Theme.s2
                spacing: Theme.s2
                GButton {
                    text: qsTr("Choose a project folder")
                    iconName: "folder-open"
                    variant: "primary"
                    size: "lg"
                    enabled: first.found
                    onClicked: projectDialog.open()
                }
                GButton {
                    text: first.found ? qsTr("Continue without a project") : qsTr("Explore the demo")
                    variant: "ghost"
                    size: "lg"
                    onClicked: first.found ? App.completeFirstRun() : App.enterDemoMode()
                }
            }
            GText {
                text: qsTr("The demo uses a scripted agent. Nothing is sent to a model.")
                role: "caption"
                tone: "tertiary"
                visible: !first.found
            }
        }
    }
}
