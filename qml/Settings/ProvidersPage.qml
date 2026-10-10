pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Model providers and their credentials. Keys gomp manages live in the system keyring
// (KWallet or GNOME Keyring through the Secret Service API), never in plain files.
Item {
    id: page
    readonly property string title: qsTr("Providers and keys")
    readonly property string subtitle: qsTr("API keys you enter here go to the system keyring. omp reads them at start; nothing is written to disk in plain text.")
    property string editingEnv: ""
    property bool showAll: false
    Component.onCompleted: {
        if (App.mockMode) App.providers.loadDemoOAuth()
        else App.providers.refreshOAuth()
    }
    readonly property bool loginIdle: ["idle", "done", "failed"].indexOf(App.providers.loginState) >= 0

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
            spacing: Theme.s5

            // Plain-text keys warning
            Rectangle {
                Layout.fillWidth: true
                visible: App.providers.plaintextKeyCount > 0
                implicitHeight: warnRow.implicitHeight + Theme.s4 * 2
                radius: Theme.rLg
                color: Theme.alpha(Theme.caution, Theme.dark ? 0.08 : 0.10)
                border.width: 1
                border.color: Theme.alpha(Theme.caution, 0.45)
                RowLayout {
                    id: warnRow
                    anchors.fill: parent
                    anchors.margins: Theme.s4
                    spacing: Theme.s3
                    GIcon { name: "alert"; color: Theme.caution; Layout.alignment: Qt.AlignTop }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        GText { text: Fmt.count(App.providers.plaintextKeyCount, qsTr("API key is"), qsTr("API keys are")) + qsTr(" stored in plain text in models.yml"); role: "strong" }
                        GText {
                            Layout.fillWidth: true
                            text: qsTr("Anyone who can read your home folder can read them. Move each to the keyring; models.yml then asks the keyring at start.")
                            tone: "secondary"
                            wrapMode: Text.WordWrap
                            elide: Text.ElideNone
                        }
                    }
                }
            }

            // Custom providers (models.yml)
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                RowLayout {
                    Layout.fillWidth: true
                    GText { text: qsTr("Custom providers"); role: "heading"; Layout.fillWidth: true }
                    GButton { text: qsTr("Add provider"); iconName: "plus"; size: "sm"; onClicked: addSheet.open() }
                }
                GText {
                    text: qsTr("OpenAI-compatible endpoints defined in ~/.omp/agent/models.yml.")
                    tone: "tertiary"
                    role: "caption"
                }
                Repeater {
                    model: App.providers.customProviders
                    GCard {
                        id: d77
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: 64
                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.s3
                            anchors.leftMargin: Theme.s4
                            spacing: Theme.s3
                            GIcon { name: "globe"; color: Theme.textSecondary }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                GText { text: d77.modelData.name; role: "strong" }
                                GText { text: d77.modelData.baseUrl + "  " + d77.modelData.api; role: "caption"; tone: "tertiary"; Layout.fillWidth: true }
                            }
                            GBadge {
                                text: d77.modelData.keySource === "keyring" ? qsTr("Key in keyring")
                                    : d77.modelData.keySource === "plaintext" ? qsTr("Plain-text key")
                                    : d77.modelData.keySource === "env" ? qsTr("From %1").arg(d77.modelData.envVar)
                                    : d77.modelData.keySource === "command" ? qsTr("Key from a command") : qsTr("No key")
                                tone: d77.modelData.keySource === "keyring" ? "success" : d77.modelData.keySource === "plaintext" ? "caution" : "neutral"
                                iconName: d77.modelData.keySource === "keyring" ? "lock" : d77.modelData.keySource === "plaintext" ? "alert" : ""
                            }
                            GButton {
                                visible: d77.modelData.keySource === "plaintext"
                                text: qsTr("Move to keyring")
                                size: "sm"
                                variant: "primary"
                                enabled: App.providers.keyringAvailable
                                onClicked: App.providers.moveKeyToKeyring(d77.modelData.name)
                            }
                            GIconButton {
                                iconName: "more"
                                tooltip: qsTr("More")
                                onClicked: provMenu.popup()
                                GMenu {
                                    id: provMenu
                                    GMenuItem { text: qsTr("Set a new key…"); iconName: "key"; onTriggered: { keySheet.target = d77.modelData.name; keySheet.envVar = ""; keySheet.open() } }
                                    GMenuSeparator {}
                                    GMenuItem { text: qsTr("Remove provider"); iconName: "trash"; destructive: true; onTriggered: App.providers.removeCustomProvider(d77.modelData.name) }
                                }
                            }
                        }
                    }
                }
                GText {
                    visible: App.providers.customProviders.length === 0
                    text: qsTr("None yet. Add one to use a self-hosted or third-party OpenAI-compatible API.")
                    tone: "tertiary"
                }
            }

            // OAuth
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                visible: App.providers.oauthProviders.length > 0
                GText { text: qsTr("Sign in with an account"); role: "heading" }
                GText { text: qsTr("Subscriptions and hosted APIs. Hover a logo for the name; omp stores the tokens it receives."); tone: "tertiary"; role: "caption" }
                // Connected accounts first, then omp's own order. Hover a tile for the provider's name.
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.s2
                    Repeater {
                        model: App.providers.oauthProviders.slice().sort((x, y) => (y.authenticated ? 1 : 0) - (x.authenticated ? 1 : 0))
                        ProviderTile {
                            id: d142
                            required property var modelData
                            provider: d142.modelData
                            enabled: d142.modelData.available && page.loginIdle
                            busy: App.providers.loginProvider === d142.modelData.id
                                  && (App.providers.loginState === "starting" || App.providers.loginState === "browser")
                            onClicked: App.providers.login(d142.modelData.id)
                        }
                    }
                }
            }

            // Env providers
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Theme.s2
                RowLayout {
                    Layout.fillWidth: true
                    GText { text: qsTr("Built-in providers"); role: "heading"; Layout.fillWidth: true }
                    GToggle { text: qsTr("Show all"); checked: page.showAll; onToggled: page.showAll = checked }
                }
                GText {
                    text: qsTr("Providers omp knows by environment variable. Keys set here are injected into omp from the keyring.")
                    tone: "tertiary"
                    role: "caption"
                }
                Repeater {
                    model: App.providers.envProviders.filter(p => page.showAll || p.source !== "none" || ["ANTHROPIC_API_KEY", "OPENAI_API_KEY", "GEMINI_API_KEY", "OPENROUTER_API_KEY", "XAI_API_KEY", "MISTRAL_API_KEY", "GROQ_API_KEY"].indexOf(p.envVar) >= 0)
                    Rectangle {
                        id: d171
                        required property var modelData
                        Layout.fillWidth: true
                        implicitHeight: page.editingEnv === d171.modelData.envVar ? 96 : 52
                        radius: Theme.rMd
                        color: envHover.hovered ? Theme.alpha(Theme.surface2, 0.6) : "transparent"
                        HoverHandler { id: envHover }
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.s3
                            spacing: Theme.s2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Theme.s3
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    GText { text: d171.modelData.description; role: "strong"; Layout.fillWidth: true }
                                    GText { text: d171.modelData.envVar; font: Theme.fCodeSmall; tone: "tertiary" }
                                }
                                GBadge {
                                    text: d171.modelData.source === "keyring" ? qsTr("In keyring") : d171.modelData.source === "environment" ? qsTr("From your shell") : qsTr("Not set")
                                    tone: d171.modelData.source === "keyring" ? "success" : d171.modelData.source === "environment" ? "info" : "neutral"
                                    iconName: d171.modelData.source === "keyring" ? "lock" : ""
                                }
                                GButton {
                                    text: d171.modelData.source === "keyring" ? qsTr("Replace") : qsTr("Set key")
                                    size: "sm"
                                    visible: page.editingEnv !== d171.modelData.envVar
                                    onClicked: { page.editingEnv = d171.modelData.envVar; keyField.text = ""; keyField.forceActiveFocus() }
                                }
                                GIconButton {
                                    visible: d171.modelData.source === "keyring" && page.editingEnv !== d171.modelData.envVar
                                    size: Theme.controlSm
                                    iconName: "trash"
                                    tooltip: qsTr("Remove from keyring")
                                    onClicked: App.providers.clearEnvKey(d171.modelData.envVar)
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: page.editingEnv === d171.modelData.envVar
                                spacing: Theme.s2
                                GTextField {
                                    id: keyField
                                    Layout.fillWidth: true
                                    echoMode: TextInput.Password
                                    mono: true
                                    placeholderText: qsTr("Paste the key")
                                    onAccepted: if (text.length) { App.providers.setEnvKey(d171.modelData.envVar, text); text = ""; page.editingEnv = "" }
                                    Keys.onEscapePressed: page.editingEnv = ""
                                }
                                GButton { text: qsTr("Cancel"); variant: "ghost"; size: "sm"; onClicked: page.editingEnv = "" }
                                GButton { text: qsTr("Save to keyring"); variant: "primary"; size: "sm"; iconName: "lock"; enabled: keyField.text.length > 0; onClicked: keyField.accepted() }
                            }
                        }
                    }
                }
                GText {
                    visible: !App.providers.keyringAvailable
                    text: qsTr("secret-tool was not found, so keys cannot be stored. Install libsecret (Fedora: sudo dnf install libsecret).")
                    tone: "danger"
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
        }
    }

    GSheet {
        id: addSheet
        title: qsTr("Add a provider")
        subtitle: qsTr("Any OpenAI-compatible API. omp discovers its models from the endpoint's model list.")
        iconName: "globe"
        onOpened: { nameField.text = ""; urlField.text = ""; apiField.value = "openai-completions"; secretField.text = ""; nameField.forceActiveFocus() }
        GField {
            Layout.fillWidth: true
            label: qsTr("Name")
            description: qsTr("Used in model selectors, for example myprovider/model-id.")
            GTextField { id: nameField; width: parent.width; placeholderText: qsTr("myprovider") }
        }
        GField {
            Layout.fillWidth: true
            label: qsTr("Base URL")
            GTextField { id: urlField; width: parent.width; placeholderText: "https://api.example.com/v1"; mono: true }
        }
        GField {
            Layout.fillWidth: true
            label: qsTr("API style")
            GDropdown {
                id: apiField
                width: parent.width
                value: "openai-completions"
                options: [
                    { value: "openai-completions", label: qsTr("OpenAI chat completions"), description: "/v1/chat/completions" },
                    { value: "openai-responses", label: qsTr("OpenAI responses"), description: "/v1/responses" },
                    { value: "anthropic-messages", label: qsTr("Anthropic messages"), description: "/v1/messages" }
                ]
                onPicked: v => value = v
            }
        }
        GField {
            Layout.fillWidth: true
            label: qsTr("API key")
            description: qsTr("Optional. Stored in the system keyring.")
            GTextField { id: secretField; width: parent.width; echoMode: TextInput.Password; mono: true }
        }
        footer: [
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: addSheet.close() },
            GButton {
                text: qsTr("Add provider")
                variant: "primary"
                enabled: nameField.text.length > 0 && urlField.text.length > 0
                onClicked: { App.providers.addCustomProvider(nameField.text, urlField.text, apiField.value, secretField.text); addSheet.close() }
            }
        ]
    }
    GSheet {
        id: keySheet
        property string target
        property string envVar
        title: qsTr("New key for %1").arg(target)
        subtitle: qsTr("Replaces the stored key. models.yml will read it from the keyring.")
        iconName: "key"
        onOpened: { newKey.text = ""; newKey.forceActiveFocus() }
        GTextField { id: newKey; Layout.fillWidth: true; echoMode: TextInput.Password; mono: true; placeholderText: qsTr("Paste the key") }
        footer: [
            Item { Layout.fillWidth: true },
            GButton { text: qsTr("Cancel"); variant: "ghost"; onClicked: keySheet.close() },
            GButton { text: qsTr("Save to keyring"); variant: "primary"; enabled: newKey.text.length > 0; onClicked: { App.providers.setCustomKey(keySheet.target, newKey.text); keySheet.close() } }
        ]
    }
}
