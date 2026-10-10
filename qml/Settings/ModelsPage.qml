import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Which model omp uses for each job. Roles are omp's (modelRoles); each picks a model and,
// optionally, an effort suffix (":high").
Item {
    id: page
    readonly property string title: qsTr("Models and roles")
    readonly property string subtitle: qsTr("omp uses different models for different jobs. Set each role once; switch a single session's model from the Agent screen.")
    readonly property var roleInfo: [
        { id: "default", label: qsTr("Default"), help: qsTr("Conversations and edits") },
        { id: "smol", label: qsTr("Small"), help: qsTr("Quick, cheap steps such as titles and summaries") },
        { id: "slow", label: qsTr("Slow"), help: qsTr("Hard problems where thoroughness beats speed") },
        { id: "plan", label: qsTr("Plan"), help: qsTr("Plan mode: exploring and writing the plan") },
        { id: "task", label: qsTr("Task"), help: qsTr("Subagents spawned by the task tool") },
        { id: "vision", label: qsTr("Vision"), help: qsTr("Reading screenshots and images") },
        { id: "commit", label: qsTr("Commit"), help: qsTr("Commit messages and changelogs") },
        { id: "tiny", label: qsTr("Tiny"), help: qsTr("Very small helper calls") },
        { id: "memory", label: qsTr("Memory"), help: qsTr("Summarising and recalling long-term memory") },
        { id: "advisor", label: qsTr("Advisor"), help: qsTr("The advisor that reviews each turn") },
        { id: "judge", label: qsTr("Judge"), help: qsTr("Evaluations and automatic checks") },
        { id: "web", label: qsTr("Web"), help: qsTr("Web search backend") }
    ]
    readonly property var roles: { App.settings.stagedCount; App.settings.scope; return App.settings.value("modelRoles") || ({}) }
    property string pickingRole: ""

    function configure() { App.settings.ids = ["defaultThinkingLevel", "cycleOrder", "enabledModels", "modelProviderOrder", "modelRoleStorage"] }
    function assign(role, selector) {
        const next = Object.assign({}, roles)
        const effort = (roles[role] || "").split(":")[1]
        next[role] = effort ? selector + ":" + effort : selector
        App.settings.setValue("modelRoles", next)
    }
    function setEffort(role, effort) {
        const next = Object.assign({}, roles)
        const base = (roles[role] || "").split(":")[0]
        if (!base.length) return
        next[role] = effort.length ? base + ":" + effort : base
        App.settings.setValue("modelRoles", next)
    }
    Component.onCompleted: if (App.models.total === 0 && !App.mockMode) App.models.refresh()

    GenericPage {
        anchors.fill: parent
        ids: ["defaultThinkingLevel", "cycleOrder", "enabledModels", "modelProviderOrder", "modelRoleStorage"]
        Component.onCompleted: configure("", "")
        headerComponent: Component {
ColumnLayout {
            width: parent ? parent.width : 0
            spacing: Theme.s2
            Repeater {
                model: page.roleInfo
                GCard {
                    id: roleCard
                    required property var modelData
                    readonly property string current: page.roles[modelData.id] || ""
                    readonly property var info: App.models.get(current.split(":")[0])
                    Layout.fillWidth: true
                    implicitHeight: 64
                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: Theme.s3
                        anchors.leftMargin: Theme.s4
                        spacing: Theme.s4
                        ColumnLayout {
                            Layout.preferredWidth: 220
                            spacing: 2
                            GText { text: roleCard.modelData.label; role: "strong" }
                            GText { text: roleCard.modelData.help; role: "caption"; tone: "tertiary"; Layout.fillWidth: true }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            GText {
                                text: roleCard.info.name || (roleCard.current.length ? roleCard.current.split(":")[0] : qsTr("Uses the default model"))
                                color: roleCard.current.length ? Theme.textPrimary : Theme.textTertiary
                                Layout.fillWidth: true
                            }
                            GText {
                                visible: roleCard.info.contextWindow !== undefined
                                text: roleCard.info.contextWindow ? Fmt.tokens(roleCard.info.contextWindow) + qsTr(" context") + (roleCard.info.pricingKnown ? qsTr(", %1 in / %2 out per 1M tokens").arg(Fmt.pricePerMTok(roleCard.info.costIn)).arg(Fmt.pricePerMTok(roleCard.info.costOut)) : "") : ""
                                role: "caption"
                                tone: "tertiary"
                                Layout.fillWidth: true
                            }
                        }
                        GDropdown {
                            implicitWidth: 120
                            visible: roleCard.current.length > 0 && (roleCard.info.efforts || []).length > 0
                            value: roleCard.current.split(":")[1] || ""
                            placeholder: qsTr("Effort")
                            options: [{ value: "", label: qsTr("Model default") }].concat((roleCard.info.efforts || []).map(e => ({ value: e, label: e })))
                            onPicked: v => page.setEffort(roleCard.modelData.id, v)
                        }
                        GButton {
                            text: qsTr("Change")
                            size: "sm"
                            onClicked: { page.pickingRole = roleCard.modelData.id; chooser.open() }
                        }
                    }
                }
            }
            GText { text: qsTr("Model selection"); role: "caption"; tone: "tertiary"; Layout.topMargin: Theme.s3 }
        }
        }
    }

    GSheet {
        id: chooser
        title: qsTr("Model for %1").arg(page.roleInfo.find(r => r.id === page.pickingRole) ? page.roleInfo.find(r => r.id === page.pickingRole).label : "")
        iconName: "chip"
        preferredWidth: 640
        onOpened: { roleSearch.text = ""; roleSearch.forceActiveFocus() }
        onClosed: App.models.query = ""
        GSearchField {
            id: roleSearch
            Layout.fillWidth: true
            placeholderText: qsTr("Search models")
            onTextChanged: App.models.query = text
        }
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: 360
            model: App.models
            clip: true
            T.ScrollBar.vertical: GScrollBar {}
            delegate: GListRow {
                required property string selector
                required property string name
                required property double contextWindow
                width: ListView.view.width
                title: name
                subtitle: selector + (contextWindow > 0 ? "  " + Fmt.tokens(contextWindow) + qsTr(" context") : "")
                selected: (page.roles[page.pickingRole] || "").split(":")[0] === selector
                onClicked: { page.assign(page.pickingRole, selector); chooser.close() }
            }
        }
        footer: [
            GButton {
                text: qsTr("Use the default model")
                variant: "ghost"
                onClicked: {
                    const next = Object.assign({}, page.roles)
                    delete next[page.pickingRole]
                    App.settings.setValue("modelRoles", next)
                    chooser.close()
                }
            },
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: chooser.close() }
        ]
    }
}
