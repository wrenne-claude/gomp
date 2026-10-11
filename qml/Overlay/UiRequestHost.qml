pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Questions omp asks through its host UI (select, confirm, input, editor, multi-question ask,
// and OAuth browser hand-offs). The oldest open request is shown; answering pops the next.
Item {
    id: host
    property Workspace workspace: null
    readonly property var request: workspace && workspace.uiRequests.length > 0 ? workspace.uiRequests[0] : null
    readonly property string kind: request ? request.kind : ""
    onRequestChanged: {
        if (request) {
            input.text = request.prefill || ""
            editor.text = request.prefill || ""
            // Start each question on omp's recommended option, so "Send answer" alone accepts it.
            const start = {}
            for (const q of (request.questions || [])) {
                if (q.recommended !== undefined && q.recommended !== null && q.options[q.recommended])
                    start[q.id] = { selected: [q.options[q.recommended].label], custom: "" }
            }
            answers = start
            sheet.open()
        } else {
            sheet.close()
        }
    }
    property var answers: ({})

    function submit(value) {
        if (!host.request) return
        host.workspace.answerUi(host.request.id, value)
    }
    function submitAsk() {
        const qs = host.request.questions
        const out = []
        for (let i = 0; i < qs.length; ++i) {
            const a = host.answers[qs[i].id] || {}
            // omp's AskAnswer: exact option labels plus optional free text.
            out.push({ id: qs[i].id, selectedOptions: a.selected || [], customInput: a.custom || "" })
        }
        submit(out)
    }

    GSheet {
        id: sheet
        title: host.request ? (host.request.title || qsTr("omp needs an answer")) : ""
        subtitle: host.request && host.kind !== "ask" ? (host.request.message || "") : ""
        iconName: host.kind === "confirm" ? "alert" : host.kind === "open_url" ? "external" : "info"
        preferredWidth: host.kind === "editor" || host.kind === "ask" ? 680 : 520
        dismissable: true
        onClosed: if (host.request) host.workspace.dismissUi(host.request.id)

        // select
        Repeater {
            model: host.kind === "select" && host.request ? host.request.options : []
            GListRow {
                id: d52
                required property string modelData
                required property int index
                Layout.fillWidth: true
                title: d52.modelData
                subtitle: host.request.optionDescriptions[index] || ""
                iconName: "chevron-right"
                onClicked: host.submit(d52.modelData)
            }
        }
        // input
        GTextField {
            id: input
            Layout.fillWidth: true
            visible: host.kind === "input"
            placeholderText: host.request ? (host.request.placeholder || "") : ""
            onAccepted: host.submit(text)
        }
        // editor
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 260
            visible: host.kind === "editor"
            radius: Theme.rMd
            color: Theme.codeBg
            border.width: 1
            border.color: editor.activeFocus ? Theme.accent : Theme.hairline
            Flickable {
                id: editorFlick
                anchors.fill: parent
                anchors.margins: Theme.s3
                contentHeight: editor.implicitHeight
                clip: true
                TextEdit {
                    id: editor
                    width: editorFlick.width
                    font: Theme.fCode
                    color: Theme.textPrimary
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    selectionColor: Theme.selection
                }
            }
        }
        // open_url (OAuth and similar)
        GText {
            Layout.fillWidth: true
            visible: host.kind === "open_url"
            text: qsTr("Your browser opens this page. Finish there, then come back.")
            tone: "secondary"
            wrapMode: Text.WordWrap
        }
        GTextField {
            Layout.fillWidth: true
            visible: host.kind === "open_url"
            readOnly: true
            mono: true
            text: host.request ? host.request.url : ""
        }
        // ask: several questions, each with options plus free text
        Repeater {
            model: host.kind === "ask" && host.request ? host.request.questions : []
            ColumnLayout {
                id: q
                required property var modelData
                Layout.fillWidth: true
                spacing: Theme.s2
                GText { text: q.modelData.header || ""; role: "caption"; tone: "tertiary"; visible: text.length > 0 }
                GText { text: q.modelData.question; role: "strong"; wrapMode: Text.WordWrap; Layout.fillWidth: true; elide: Text.ElideNone }
                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.s2
                    Repeater {
                        model: q.modelData.options
                        GButton {
                            id: d126
                            required property var modelData
                            readonly property bool chosen: ((host.answers[q.modelData.id] || {}).selected || []).indexOf(d126.modelData.label) >= 0
                            text: d126.modelData.label
                            size: "sm"
                            variant: chosen ? "primary" : "secondary"
                            onClicked: {
                                const a = Object.assign({ selected: [], custom: "" }, host.answers[q.modelData.id] || {})
                                const sel = a.selected.slice()
                                const at = sel.indexOf(d126.modelData.label)
                                if (q.modelData.multi) { if (at >= 0) sel.splice(at, 1); else sel.push(d126.modelData.label) }
                                else sel.splice(0, sel.length, d126.modelData.label)
                                a.selected = sel
                                const next = Object.assign({}, host.answers); next[q.modelData.id] = a; host.answers = next
                            }
                            GToolTip { visible: d126.hovered && (d126.modelData.description || "").length > 0; text: d126.modelData.description || "" }
                        }
                    }
                }
                GTextField {
                    Layout.fillWidth: true
                    placeholderText: qsTr("Or write your own answer")
                    onTextChanged: {
                        const a = Object.assign({ selected: [], custom: "" }, host.answers[q.modelData.id] || {})
                        a.custom = text
                        const next = Object.assign({}, host.answers); next[q.modelData.id] = a; host.answers = next
                    }
                }
            }
        }

        footer: [
            Item { Layout.fillWidth: true },
            GButton {
                text: host.kind === "confirm" ? qsTr("No") : host.kind === "open_url" ? qsTr("Copy link") : qsTr("Skip")
                variant: "ghost"
                onClicked: {
                    if (host.kind === "confirm") host.submit(false)
                    else if (host.kind === "open_url") App.copyText(host.request.url)
                    else sheet.close()
                }
            },
            GButton {
                visible: host.kind !== "select"
                text: host.kind === "confirm" ? qsTr("Yes") : host.kind === "open_url" ? qsTr("Open again") : qsTr("Send answer")
                variant: "primary"
                onClicked: {
                    if (host.kind === "confirm") host.submit(true)
                    else if (host.kind === "open_url") App.openUrl(host.request.url)
                    else if (host.kind === "input") host.submit(input.text)
                    else if (host.kind === "editor") host.submit(editor.text)
                    else if (host.kind === "ask") host.submitAsk()
                }
            }
        ]
    }
}
