pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// MCP servers that give the agent extra tools. Global ones load everywhere; project ones live in
// the project's .omp/mcp.json and travel with the repository.
Item {
    id: page
    readonly property string title: qsTr("MCP servers")
    readonly property string subtitle: qsTr("Model Context Protocol servers add tools the agent can call. omp picks up changes on the next session start.")

    Flickable {
        id: flick
        anchors.fill: parent
        contentHeight: col.implicitHeight + Theme.s6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: GScrollBar {}
        ColumnLayout {
            id: col
            x: Theme.s6
            width: Math.min(flick.width - Theme.s6 * 2, 900)
            spacing: Theme.s3
            RowLayout {
                Layout.fillWidth: true
                GText { text: Fmt.count(App.mcp.servers.length, qsTr("server"), qsTr("servers")); tone: "secondary"; Layout.fillWidth: true }
                GButton { text: qsTr("Add server"); iconName: "plus"; variant: "primary"; size: "sm"; onClicked: { editor.load(null); editor.open() } }
            }
            Repeater {
                model: App.mcp.servers
                GCard {
                    id: card
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: 72
                    opacity: card.modelData.enabled ? 1 : 0.7
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s3
                        anchors.leftMargin: Theme.s4
                        spacing: Theme.s3
                        GIcon { name: card.modelData.type === "stdio" ? "terminal" : "globe"; color: Theme.textSecondary }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            RowLayout {
                                spacing: Theme.s2
                                GText { text: card.modelData.name; role: "strong" }
                                GBadge { text: card.modelData.type }
                                GBadge { text: card.modelData.scope === "project" ? qsTr("Project") : qsTr("All projects"); tone: card.modelData.scope === "project" ? "plan" : "neutral" }
                                GBadge { visible: card.modelData.auth; text: qsTr("Signs in"); iconName: "lock" }
                            }
                            GText {
                                Layout.fillWidth: true
                                text: card.modelData.type === "stdio" ? [card.modelData.command].concat(card.modelData.args).join(" ") : card.modelData.url
                                font: Theme.fCodeSmall
                                tone: "tertiary"
                                elide: Text.ElideMiddle
                            }
                        }
                        GToggle {
                            checked: card.modelData.enabled
                            Accessible.name: qsTr("Enable %1").arg(card.modelData.name)
                            onToggled: {
                                const err = App.mcp.setEnabled(card.modelData.scope, card.modelData.name, checked)
                                if (err.length) Ui.toast("error", err)
                            }
                        }
                        GIconButton {
                            iconName: "more"
                            tooltip: qsTr("More")
                            onClicked: menu.popup()
                            GMenu {
                                id: menu
                                GMenuItem { text: qsTr("Edit…"); iconName: "pencil"; onTriggered: { editor.load(card.modelData); editor.open() } }
                                GMenuSeparator {}
                                GMenuItem {
                                    text: qsTr("Remove"); iconName: "trash"; destructive: true
                                    onTriggered: { const err = App.mcp.removeServer(card.modelData.scope, card.modelData.name); Ui.toast(err.length ? "error" : "info", err.length ? err : qsTr("Removed %1. A backup was kept.").arg(card.modelData.name)) }
                                }
                            }
                        }
                    }
                }
            }
            GEmptyState {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Theme.s6
                visible: App.mcp.servers.length === 0
                iconName: "plug"
                title: qsTr("No MCP servers")
                body: qsTr("Add a local command (stdio) or a remote endpoint (http). Popular choices: documentation search, issue trackers, databases.")
            }
        }
    }

    GSheet {
        id: editor
        property string originalName
        property string originalScope
        title: originalName.length ? qsTr("Edit %1").arg(originalName) : qsTr("Add an MCP server")
        iconName: "plug"
        preferredWidth: 620
        property string error
        function load(s) {
            error = ""
            originalName = s ? s.name : ""
            originalScope = s ? s.scope : "global"
            nameF.text = s ? s.name : ""
            scopeF.value = s ? s.scope : (App.settings.hasProject ? "project" : "global")
            typeF.value = s ? s.type : "stdio"
            commandF.text = s ? s.command : ""
            argsF.text = s ? s.args.join(" ") : ""
            urlF.text = s ? s.url : ""
            envF.text = s ? Object.keys(s.env).map(k => k + "=" + s.env[k]).join("\n") : ""
            headersF.text = s ? Object.keys(s.headers).map(k => k + ": " + s.headers[k]).join("\n") : ""
        }
        function pairs(text, sep) {
            const out = {}
            for (const line of text.split("\n")) {
                const i = line.indexOf(sep)
                if (i > 0) out[line.substring(0, i).trim()] = line.substring(i + 1).trim()
            }
            return out
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.s3
            GField { Layout.fillWidth: true; label: qsTr("Name"); GTextField { id: nameF; width: parent.width; placeholderText: "context7"; mono: true } }
            GField {
                label: qsTr("Where")
                GSegmented {
                    id: scopeF
                    size: "sm"
                    value: "global"
                    options: [{ value: "global", label: qsTr("All projects") }].concat(App.settings.hasProject ? [{ value: "project", label: qsTr("This project") }] : [])
                    onActivated: v => value = v
                }
            }
        }
        GField {
            Layout.fillWidth: true
            label: qsTr("Transport")
            GSegmented {
                id: typeF
                size: "sm"
                value: "stdio"
                options: [{ value: "stdio", label: qsTr("Local command") }, { value: "http", label: qsTr("HTTP") }, { value: "sse", label: qsTr("SSE (legacy)") }]
                onActivated: v => value = v
            }
        }
        GField {
            Layout.fillWidth: true
            visible: typeF.value === "stdio"
            label: qsTr("Command")
            description: qsTr("The executable and its arguments, for example npx -y @upstash/context7-mcp.")
            RowLayout {
                width: parent.width
                GTextField { id: commandF; Layout.preferredWidth: 200; mono: true; placeholderText: "npx" }
                GTextField { id: argsF; Layout.fillWidth: true; mono: true; placeholderText: "-y @upstash/context7-mcp" }
            }
        }
        GField {
            Layout.fillWidth: true
            visible: typeF.value !== "stdio"
            label: qsTr("URL")
            GTextField { id: urlF; width: parent.width; mono: true; placeholderText: "https://mcp.example.com/mcp" }
        }
        GField {
            Layout.fillWidth: true
            label: typeF.value === "stdio" ? qsTr("Environment variables") : qsTr("Headers")
            description: typeF.value === "stdio" ? qsTr("One per line, NAME=value.") : qsTr("One per line, Name: value.")
            Rectangle {
                width: parent.width
                height: 84
                radius: Theme.rMd
                color: Theme.surface1
                border.width: 1
                border.color: Theme.hairline
                TextEdit {
                    id: envF
                    anchors.fill: parent
                    anchors.margins: Theme.s2
                    visible: typeF.value === "stdio"
                    font: Theme.fCodeSmall
                    color: Theme.textPrimary
                    selectByMouse: true
                }
                TextEdit {
                    id: headersF
                    anchors.fill: parent
                    anchors.margins: Theme.s2
                    visible: typeF.value !== "stdio"
                    font: Theme.fCodeSmall
                    color: Theme.textPrimary
                    selectByMouse: true
                }
            }
        }
        GText { visible: editor.error.length > 0; text: editor.error; tone: "danger"; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        footer: [
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: editor.close() },
            GButton {
                text: editor.originalName.length ? qsTr("Save") : qsTr("Add server")
                variant: "primary"
                onClicked: {
                    const server = { name: nameF.text.trim(), type: typeF.value }
                    if (typeF.value === "stdio") {
                        server.command = commandF.text.trim()
                        server.args = argsF.text.trim().length ? argsF.text.trim().split(/\s+/) : []
                        server.env = editor.pairs(envF.text, "=")
                    } else {
                        server.url = urlF.text.trim()
                        server.headers = editor.pairs(headersF.text, ":")
                    }
                    if (editor.originalName.length && editor.originalScope !== scopeF.value)
                        App.mcp.removeServer(editor.originalScope, editor.originalName)
                    editor.error = App.mcp.saveServer(scopeF.value, editor.originalScope === scopeF.value ? editor.originalName : "", server)
                    if (!editor.error.length) { editor.close(); Ui.toast("success", qsTr("Saved %1. New sessions load it.").arg(server.name)) }
                }
            }
        ]
    }
}
