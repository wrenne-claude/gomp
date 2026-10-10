import QtQuick
import QtQuick.Layouts
import Gomp
import Gomp.Core

// When the agent must ask before using a tool: a default level plus per-tool exceptions.
GenericPage {
    id: page
    title: qsTr("Permissions")
    subtitle: qsTr("Choose what the agent may do without asking. Requests appear in the conversation and as a desktop notification when gomp is in the background.")
    readonly property string mode: { App.settings.stagedCount; App.settings.scope; return String(App.settings.value("tools.approvalMode") || "yolo") }
    readonly property var policies: { App.settings.stagedCount; App.settings.scope; return App.settings.value("tools.approval") || ({}) }
    readonly property var tools: [
        { id: "bash", label: qsTr("Shell commands"), tier: "exec" },
        { id: "eval", label: qsTr("Code evaluation"), tier: "exec" },
        { id: "browser", label: qsTr("Browser control"), tier: "exec" },
        { id: "task", label: qsTr("Subagents"), tier: "exec" },
        { id: "edit", label: qsTr("Edit files"), tier: "write" },
        { id: "write", label: qsTr("Create and overwrite files"), tier: "write" },
        { id: "fetch", label: qsTr("Fetch web pages"), tier: "read" },
        { id: "web_search", label: qsTr("Web search"), tier: "read" },
        { id: "read", label: qsTr("Read files"), tier: "read" }
    ]
    function setPolicy(tool, policy) {
        const next = Object.assign({}, policies)
        if (policy === "inherit") delete next[tool]
        else next[tool] = policy
        App.settings.setValue("tools.approval", next)
    }
    function configure(pageId, query) {
        if (query.length) { App.settings.query = query; return }
        App.settings.query = ""
        App.settings.ids = App.settings.idsWithPrefix(["tools."], ["tools.approvalMode", "tools.approval"])
    }

    headerComponent: Component {
ColumnLayout {
        width: parent ? parent.width : 0
        spacing: Theme.s3
        GridLayout {
            Layout.fillWidth: true
            columns: width > 760 ? 3 : 1
            columnSpacing: Theme.s3
            rowSpacing: Theme.s3
            Repeater {
                model: [
                    { value: "always-ask", label: qsTr("Ask for anything that changes things"), body: qsTr("Reading is free. Edits, writes, commands and subagents wait for you."), icon: "shield" },
                    { value: "write", label: qsTr("Edit freely, ask before running"), body: qsTr("The agent edits files in the project. Commands, browsers and subagents wait for you."), icon: "pencil" },
                    { value: "yolo", label: qsTr("Never ask"), body: qsTr("Everything runs immediately. Per-tool rules below can still ask or block."), icon: "bolt" }
                ]
                GCard {
                    id: modeCard
                    required property var modelData
                    readonly property bool chosen: page.mode === modelData.value
                    Layout.fillWidth: true
                    implicitHeight: 112
                    interactive: true
                    selected: chosen
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData.label
                    Accessible.checked: chosen
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s4
                        spacing: Theme.s2
                        RowLayout {
                            spacing: Theme.s2
                            GIcon { name: modeCard.modelData.icon; color: modeCard.chosen ? Theme.accent : Theme.textSecondary }
                            GText { text: modeCard.modelData.label; role: "strong"; Layout.fillWidth: true }
                        }
                        GText { text: modeCard.modelData.body; tone: "secondary"; role: "caption"; wrapMode: Text.WordWrap; elide: Text.ElideNone; Layout.fillWidth: true }
                    }
                    TapHandler { onTapped: App.settings.setValue("tools.approvalMode", modeCard.modelData.value) }
                }
            }
        }
        GText { text: qsTr("Per-tool rules"); role: "heading"; Layout.topMargin: Theme.s4 }
        GText {
            text: qsTr("Override the level above for single tools. Block stops a tool even in Never ask.")
            tone: "tertiary"
            role: "caption"
        }
        Repeater {
            model: page.tools
            RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Layout.preferredHeight: 44
                spacing: Theme.s3
                GText { text: modelData.label; Layout.preferredWidth: 240 }
                GText { text: modelData.id; font: Theme.fCodeSmall; tone: "tertiary"; Layout.fillWidth: true }
                GSegmented {
                    size: "sm"
                    value: page.policies[modelData.id] || "inherit"
                    options: [
                        { value: "inherit", label: qsTr("Default") },
                        { value: "allow", label: qsTr("Allow") },
                        { value: "prompt", label: qsTr("Ask") },
                        { value: "deny", label: qsTr("Block") }
                    ]
                    onActivated: v => page.setPolicy(modelData.id, v)
                }
            }
        }
        GText { text: qsTr("Other tool settings"); role: "caption"; tone: "tertiary"; Layout.topMargin: Theme.s4 }
    }
        }
}
