pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Pick the session's model: fuzzy search across providers with context window, output limit,
// price and modalities; reasoning effort below. Changes apply to this session only.
GPopover {
    id: picker
    property Workspace workspace: null
    width: 520
    height: Math.min(560, (parent && parent.Window.window ? parent.Window.window.height - 140 : 560))
    padding: 0
    onOpened: { search.text = ""; search.forceActiveFocus(); if (App.models.total === 0) App.models.refresh() }
    onClosed: App.models.query = ""

    contentItem: ColumnLayout {
        spacing: 0
        GSearchField {
            id: search
            Layout.fillWidth: true
            Layout.margins: Theme.s3
            placeholderText: qsTr("Search %1").arg(Fmt.count(App.models.total, qsTr("model"), qsTr("models")))
            onTextChanged: { App.models.query = text; list.currentIndex = 0 }
            Keys.onDownPressed: list.currentIndex = Math.min(list.count - 1, list.currentIndex + 1)
            Keys.onUpPressed: list.currentIndex = Math.max(0, list.currentIndex - 1)
            Keys.onReturnPressed: list.pick(list.currentIndex)
        }
        GDivider { Layout.fillWidth: true }
        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: App.models
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            T.ScrollBar.vertical: GScrollBar {}
            section.property: App.models.query.length ? "" : "provider"
            section.delegate: Item {
                id: d42
                required property string section
                width: list.width
                height: 30
                GText { x: Theme.s4; anchors.bottom: parent.bottom; anchors.bottomMargin: 4; text: d42.section; role: "caption"; tone: "tertiary" }
            }
            function pick(row) {
                const m = App.models.at(row)
                if (m.selector && picker.workspace) picker.workspace.setModel(m.selector)
                picker.close()
            }
            delegate: Item {
                id: entry
                required property int index
                required property string selector
                required property string highlighted
                required property string provider
                required property double contextWindow
                required property double maxTokens
                required property bool reasoning
                required property bool images
                required property double costIn
                required property double costOut
                required property bool pricingKnown
                width: list.width
                height: 52
                readonly property bool active: picker.workspace && picker.workspace.modelSelector === selector
                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s2
                    anchors.rightMargin: Theme.s2
                    radius: Theme.rMd
                    color: entry.ListView.isCurrentItem ? Theme.accentSoft : eh.hovered ? Theme.surface2 : "transparent"
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.s4
                    anchors.rightMargin: Theme.s4
                    spacing: Theme.s3
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        RowLayout {
                            spacing: Theme.s2
                            Text { text: entry.highlighted; textFormat: Text.StyledText; font: Theme.fBodyStrong; color: Theme.textPrimary; elide: Text.ElideRight; Layout.maximumWidth: 260 }
                            GIcon { name: "check"; size: Theme.iconSm; color: Theme.accent; visible: entry.active }
                        }
                        GText { text: entry.selector; role: "caption"; tone: "tertiary"; Layout.fillWidth: true }
                    }
                    Row {
                        spacing: Theme.s1 + 2
                        GBadge { visible: entry.reasoning; iconName: "orbit"; text: qsTr("reasoning") }
                        GBadge { visible: entry.images; iconName: "image"; text: qsTr("images") }
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.preferredWidth: 96
                        GText { Layout.alignment: Qt.AlignRight; text: entry.contextWindow > 0 ? Fmt.tokens(entry.contextWindow) + qsTr(" ctx") : "—"; role: "readout"; color: Theme.textSecondary }
                        GText {
                            Layout.alignment: Qt.AlignRight
                            text: entry.pricingKnown ? Fmt.pricePerMTok(entry.costIn) + " / " + Fmt.pricePerMTok(entry.costOut) : qsTr("price unknown")
                            role: "readout"
                            tone: "tertiary"
                            font.pixelSize: 11
                        }
                    }
                }
                HoverHandler { id: eh }
                TapHandler { onTapped: list.pick(entry.index) }
            }
            GEmptyState {
                anchors.centerIn: parent
                visible: list.count === 0
                iconName: App.models.error.length ? "alert" : "chip"
                tone: App.models.error.length ? "danger" : "neutral"
                title: App.models.loading ? qsTr("Loading models") : App.models.error.length ? qsTr("Could not list models") : qsTr("No models match")
                body: App.models.error.length ? App.models.error : ""
                GButton { visible: App.models.error.length > 0; text: qsTr("Try again"); onClicked: App.models.refresh() }
            }
        }
        GDivider { Layout.fillWidth: true }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Theme.s3
            spacing: Theme.s3
            GText { text: qsTr("Effort"); role: "strong" }
            GSegmented {
                Layout.fillWidth: true
                size: "sm"
                value: picker.workspace ? picker.workspace.thinkingLevel : ""
                options: picker.workspace ? picker.workspace.thinkingLevels.map(l => ({ value: l, label: l })) : []
                onActivated: v => picker.workspace.setThinkingLevel(v)
            }
        }
        GText {
            Layout.fillWidth: true
            Layout.leftMargin: Theme.s3
            Layout.bottomMargin: Theme.s3
            text: qsTr("Applies to this session. Set defaults per role in Settings, Models.")
            role: "caption"
            tone: "tertiary"
        }
    }
}
