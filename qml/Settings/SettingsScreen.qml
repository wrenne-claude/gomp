pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Settings: curated pages for the things people change most, then every omp settings tab,
// then the raw file. Edits are staged; nothing is written until you review the diff.
Item {
    id: screen
    property string page: Ui.settingsPage
    property string query: ""
    // Pages are plain Items with optional title, subtitle and configure(); read them untyped.
    readonly property var current: pageLoader.item
    readonly property var curated: [
        { id: "providers", label: qsTr("Providers and keys"), icon: "key" },
        { id: "models", label: qsTr("Models and roles"), icon: "chip" },
        { id: "modes", label: qsTr("Modes"), icon: "plan" },
        { id: "permissions", label: qsTr("Permissions"), icon: "shield" },
        { id: "mcp", label: qsTr("MCP servers"), icon: "plug" },
        { id: "extensions", label: qsTr("Extensions and skills"), icon: "puzzle" },
        { id: "appearance", label: qsTr("Appearance"), icon: "palette" },
        { id: "keybindings", label: qsTr("Keyboard"), icon: "keyboard" }
    ]
    readonly property bool scoped: page !== "appearance" && page !== "keybindings" && page !== "providers" && page !== "extensions"

    Connections {
        target: Ui
        function onSettingsPageChanged() {
            if (Ui.settingsPage.startsWith("search:")) {
                navSearch.text = Ui.settingsPage.substring(7)
                navSearch.forceActiveFocus()
            } else {
                screen.page = Ui.settingsPage
            }
        }
    }
    Component.onCompleted: { App.settings.reload(); if (Ui.settingsPage.startsWith("search:")) navSearch.text = Ui.settingsPage.substring(7) }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Navigation ----
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 248
            color: Theme.surface0
            Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.hairline }
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.s3
                anchors.rightMargin: Theme.s3 + 1
                spacing: Theme.s2
                GSearchField {
                    id: navSearch
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search %1 settings").arg(App.settings.sections.reduce((n, s) => n + s.count, 0))
                    onTextChanged: screen.query = text
                }
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: navColumn.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    T.ScrollBar.vertical: GScrollBar {}
                    ColumnLayout {
                        id: navColumn
                        width: parent.width
                        spacing: 2
                        Repeater {
                            model: screen.curated
                            GListRow {
                                id: d72
                                required property var modelData
                                Layout.fillWidth: true
                                dense: true
                                title: d72.modelData.label
                                iconName: d72.modelData.icon
                                selected: screen.query.length === 0 && screen.page === d72.modelData.id
                                onClicked: { navSearch.text = ""; screen.page = d72.modelData.id; Ui.settingsPage = d72.modelData.id }
                                GBadge {
                                    visible: d72.modelData.id === "providers" && App.providers.plaintextKeyCount > 0
                                    text: String(App.providers.plaintextKeyCount)
                                    tone: "caution"
                                }
                                GBadge {
                                    visible: d72.modelData.id === "keybindings" && App.shortcuts.conflictCount > 0
                                    text: String(App.shortcuts.conflictCount)
                                    tone: "danger"
                                }
                            }
                        }
                        GText { text: qsTr("All omp settings"); role: "caption"; tone: "tertiary"; Layout.topMargin: Theme.s3; Layout.leftMargin: Theme.s3; Layout.bottomMargin: 2 }
                        Repeater {
                            model: App.settings.sections
                            GListRow {
                                id: d95
                                required property var modelData
                                Layout.fillWidth: true
                                dense: true
                                title: d95.modelData.title
                                selected: screen.query.length === 0 && screen.page === "tab:" + d95.modelData.id
                                onClicked: { navSearch.text = ""; screen.page = "tab:" + d95.modelData.id; Ui.settingsPage = screen.page }
                                GText { text: String(d95.modelData.count); role: "readout"; tone: "tertiary" }
                            }
                        }
                        GListRow {
                            Layout.fillWidth: true
                            Layout.topMargin: Theme.s3
                            dense: true
                            title: qsTr("Edit config files")
                            iconName: "file-text"
                            selected: screen.query.length === 0 && screen.page === "advanced"
                            onClicked: { navSearch.text = ""; screen.page = "advanced"; Ui.settingsPage = "advanced" }
                        }
                    }
                }
            }
        }

        // ---- Page ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Theme.s6
                Layout.rightMargin: Theme.s5
                Layout.topMargin: Theme.s5
                Layout.bottomMargin: Theme.s3
                spacing: Theme.s4
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    GText {
                        text: screen.query.length ? qsTr("Results for “%1”").arg(screen.query) : screen.current ? screen.current.title : ""
                        role: "title"
                        Layout.fillWidth: true
                    }
                    GText {
                        text: screen.query.length ? "" : screen.current && screen.current.subtitle ? screen.current.subtitle : ""
                        tone: "secondary"
                        visible: text.length > 0
                        wrapMode: Text.WordWrap
                        elide: Text.ElideNone
                        Layout.fillWidth: true
                        Layout.maximumWidth: 760
                    }
                }
                ColumnLayout {
                    visible: screen.scoped || screen.query.length > 0
                    spacing: 2
                    Layout.alignment: Qt.AlignTop
                    GSegmented {
                        size: "sm"
                        value: App.settings.scope
                        options: [
                            { value: "global", label: qsTr("All projects"), icon: "globe" },
                            { value: "project", label: App.settings.hasProject ? App.settings.projectDir.split("/").pop() : qsTr("This project"), icon: "folder" }
                        ]
                        enabled: App.settings.hasProject
                        onActivated: v => App.settings.scope = v
                    }
                    GText {
                        Layout.alignment: Qt.AlignRight
                        text: !App.settings.hasProject ? qsTr("Open a project to override settings for it")
                              : App.settings.scope === "project" ? qsTr("Writes %1/.omp/config.yml").arg(Fmt.shortPath(App.settings.projectDir))
                              : qsTr("Writes ~/.omp/agent/config.yml")
                        role: "caption"
                        tone: "tertiary"
                    }
                }
            }

            Loader {
                id: pageLoader
                Layout.fillWidth: true
                Layout.fillHeight: true
                source: screen.query.length ? "GenericPage.qml"
                      : screen.page.startsWith("tab:") ? "GenericPage.qml"
                      : screen.page === "providers" ? "ProvidersPage.qml"
                      : screen.page === "models" ? "ModelsPage.qml"
                      : screen.page === "modes" ? "ModesPage.qml"
                      : screen.page === "permissions" ? "PermissionsPage.qml"
                      : screen.page === "mcp" ? "McpPage.qml"
                      : screen.page === "extensions" ? "ExtensionsPage.qml"
                      : screen.page === "appearance" ? "AppearancePage.qml"
                      : screen.page === "keybindings" ? "KeybindingsPage.qml"
                      : screen.page === "advanced" ? "AdvancedPage.qml"
                      : "ProvidersPage.qml"
                onLoaded: if (screen.current && screen.current.configure) screen.current.configure(screen.page, screen.query)
            }
            Connections {
                target: screen
                function onQueryChanged() { if (screen.current && screen.current.configure) screen.current.configure(screen.page, screen.query) }
                function onPageChanged() { if (screen.current && screen.current.configure) screen.current.configure(screen.page, screen.query) }
            }

            PendingBar { Layout.fillWidth: true }
        }
    }
}
