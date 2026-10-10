import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Plugins (omp plugin) and skills (omp skill), plus the settings that govern discovery.
GenericPage {
    id: page
    title: qsTr("Extensions and skills")
    subtitle: qsTr("Plugins add tools, commands and providers. Skills are reusable instructions the agent loads when a task matches.")
    property var plugins: []
    property var skills: []
    property bool loading: true
    function reload() {
        loading = true
        App.ompJson(["plugin", "list", "--json"], (ok, data, err) => {
            page.plugins = ok ? (data.npm || []).concat(data.marketplace || []) : []
            page.loading = false
        })
        App.ompJson(["skill", "list", "--json"], (ok, data, err) => { page.skills = ok ? (data.skills || []) : [] })
    }
    function configure(pageId, query) {
        if (query.length) { App.settings.query = query; return }
        App.settings.query = ""
        App.settings.ids = App.settings.idsWithPrefix(["extensions", "disabledExtensions", "skills.", "commands."])
    }
    Component.onCompleted: reload()

    headerComponent: Component {
        ColumnLayout {
            width: parent ? parent.width : 0
            spacing: Theme.s3
            RowLayout {
                Layout.fillWidth: true
                GText { text: qsTr("Plugins"); role: "heading"; Layout.fillWidth: true }
                GButton { text: qsTr("Refresh"); iconName: "refresh"; variant: "ghost"; size: "sm"; onClicked: page.reload() }
            }
            Repeater {
                model: page.plugins
                GCard {
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 60
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s3
                        anchors.leftMargin: Theme.s4
                        spacing: Theme.s3
                        GIcon { name: "puzzle"; color: Theme.textSecondary }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                spacing: Theme.s2
                                GText { text: modelData.name || modelData.id || ""; role: "strong" }
                                GText { text: modelData.version || ""; role: "readout"; tone: "tertiary" }
                            }
                            GText { text: modelData.description || ""; role: "caption"; tone: "tertiary"; Layout.fillWidth: true }
                        }
                        GToggle {
                            checked: modelData.enabled !== false
                            onToggled: App.ompRun(["plugin", checked ? "enable" : "disable", modelData.name], (ok, msg) => { Ui.toast(ok ? "info" : "error", msg); page.reload() })
                        }
                    }
                }
            }
            GText {
                visible: !page.loading && page.plugins.length === 0
                text: qsTr("No plugins installed. Install one with omp plugin install <package>, or from the marketplace in omp.")
                tone: "tertiary"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            GText { text: qsTr("Skills"); role: "heading"; Layout.topMargin: Theme.s4 }
            Flow {
                Layout.fillWidth: true
                spacing: Theme.s2
                Repeater {
                    model: page.skills
                    GCard {
                        required property var modelData
                        width: 280
                        height: 64
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.s3
                            spacing: 2
                            RowLayout {
                                spacing: Theme.s2
                                GText { text: modelData.name || ""; role: "strong" }
                                GText { text: modelData.version || ""; role: "readout"; tone: "tertiary" }
                            }
                            GText { text: modelData.description || ""; role: "caption"; tone: "tertiary"; Layout.fillWidth: true }
                        }
                    }
                }
            }
            GText {
                visible: page.skills.length === 0
                text: qsTr("No registry skills installed. Skills in .omp/skills and ~/.omp/agent/skills are discovered automatically.")
                tone: "tertiary"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
            GText { text: qsTr("Discovery settings"); role: "caption"; tone: "tertiary"; Layout.topMargin: Theme.s4 }
        }
    }
}
