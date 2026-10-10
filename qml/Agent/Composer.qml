pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Dialogs
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Where prompts are written. Enter sends (or queues a follow-up while the agent works),
// Shift+Enter adds a line, Alt+Enter steers the running turn. "/" opens commands, "@" opens files.
Item {
    id: composer
    property Workspace workspace: null
    readonly property bool busy: workspace && workspace.busy
    readonly property bool ready: workspace && workspace.connection === Workspace.Ready
    property var attachments: []   // local file paths
    property var history: []
    property int historyIndex: -1
    implicitHeight: col.implicitHeight + Theme.s3 + (Theme.compact ? Theme.s2 : Theme.s4)

    // Slash and @ completion state, derived from the text before the cursor.
    readonly property string beforeCursor: input.text.substring(0, input.cursorPosition)
    readonly property bool slashMode: /^\/[\w:-]*$/.test(beforeCursor)
    readonly property var atMatch: /(^|\s)@([\w.\/-]*)$/.exec(beforeCursor)
    readonly property bool fileMode: atMatch !== null && !slashMode

    FileIndex {
        id: files
        root: composer.fileMode && composer.workspace ? composer.workspace.cwd : ""
        query: composer.fileMode ? composer.atMatch[2] : ""
        highlightColor: Theme.syntax.accent
    }
    Binding {
        target: composer.workspace ? composer.workspace.commands : null
        property: "query"
        value: composer.slashMode ? composer.beforeCursor.substring(1) : ""
        when: composer.workspace !== null
    }

    FileDialog {
        id: imageDialog
        title: qsTr("Attach images")
        fileMode: FileDialog.OpenFiles
        nameFilters: [qsTr("Images (*.png *.jpg *.jpeg *.webp *.gif)")]
        onAccepted: composer.attachments = composer.attachments.concat(selectedFiles.map(u => App.toLocalPath(u.toString())))
    }

    function send(steerNow) {
        if (!workspace) return
        const text = input.text.trim()
        if (!text.length && !attachments.length) return
        if (steerNow && busy) workspace.steer(text)
        else workspace.send(text, attachments)
        if (text.length) { history.unshift(text); if (history.length > 50) history.pop() }
        historyIndex = -1
        input.text = ""
        attachments = []
    }
    function completeCommand(name) {
        input.text = "/" + name + " " + input.text.substring(input.cursorPosition)
        input.cursorPosition = name.length + 2
    }
    function completeFile(path) {
        const start = input.cursorPosition - atMatch[2].length - 1
        input.remove(start, input.cursorPosition)
        input.insert(start, "@" + path + " ")
    }

    Connections {
        target: composer.workspace
        function onBranchTextReady(text) { input.text = text; input.cursorPosition = text.length; input.forceActiveFocus() }
    }
    Connections {
        target: Ui
        function onFocusComposer() { input.forceActiveFocus() }
        function onOpenOverlay(name, props) {
            if (name === "composerText") {
                input.text = props.text
                input.cursorPosition = input.text.length
                input.forceActiveFocus()
            }
        }
    }

    ColumnLayout {
        id: col
        x: Theme.s5
        width: parent.width - Theme.s5 * 2
        y: Theme.s3
        spacing: Theme.s2

        // Queued follow-ups the agent will read after this turn.
        Flow {
            Layout.fillWidth: true
            spacing: Theme.s2
            visible: composer.workspace && composer.workspace.queuedMessages.length > 0
            Repeater {
                model: composer.workspace ? composer.workspace.queuedMessages : []
                GBadge { required property string modelData; iconName: "clock"; text: modelData.length > 60 ? modelData.substring(0, 59) + "…" : modelData }
            }
        }

        Rectangle {
            id: frame
            Layout.fillWidth: true
            implicitHeight: inner.implicitHeight + Theme.s2 * 2
            radius: Theme.rLg
            color: Theme.surface1
            border.width: input.activeFocus ? 2 : 1
            border.color: input.activeFocus ? Theme.alpha(Theme.modeColor(composer.workspace ? composer.workspace.mode : "default"), 0.8) : Theme.hairlineStrong
            Behavior on border.color { ColorAnimation { duration: Theme.dBase } }

            DropArea {
                anchors.fill: parent
                onDropped: drop => {
                    if (!drop.hasUrls) return
                    for (const u of drop.urls) {
                        const p = App.toLocalPath(u.toString())
                        if (/\.(png|jpe?g|webp|gif)$/i.test(p)) composer.attachments = composer.attachments.concat([p])
                        else input.insert(input.cursorPosition, "@" + p + " ")
                    }
                }
            }

            ColumnLayout {
                id: inner
                x: Theme.s3
                y: Theme.s2
                width: parent.width - Theme.s3 * 2
                spacing: Theme.s2

                Flow {
                    Layout.fillWidth: true
                    spacing: Theme.s2
                    visible: composer.attachments.length > 0
                    Repeater {
                        model: composer.attachments
                        Rectangle {
                            required property string modelData
                            required property int index
                            width: 64
                            height: 48
                            radius: Theme.rSm + 2
                            color: Theme.surface2
                            border.width: 1
                            border.color: Theme.hairline
                            clip: true
                            Image { anchors.fill: parent; anchors.margins: 1; source: "file://" + parent.modelData; fillMode: Image.PreserveAspectCrop; asynchronous: true; sourceSize: Qt.size(128, 96) }
                            GIconButton {
                                anchors.top: parent.top
                                anchors.right: parent.right
                                size: 20
                                subtle: false
                                iconName: "close"
                                tooltip: qsTr("Remove")
                                onClicked: { const a = composer.attachments.slice(); a.splice(parent.index, 1); composer.attachments = a }
                            }
                        }
                    }
                }

                Flickable {
                    id: flick
                    Layout.fillWidth: true
                    readonly property int lineHeight: Math.ceil(Theme.fBody.pixelSize * Theme.lhBody)
                    Layout.minimumHeight: lineHeight + 8
                    Layout.preferredHeight: Math.max(lineHeight + 8, Math.min(input.implicitHeight, lineHeight * (Theme.compact ? 6 : 10) + 8))
                    contentHeight: input.implicitHeight
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    T.ScrollBar.vertical: GScrollBar {}
                    function ensureVisible(r) {
                        if (contentY >= r.y) contentY = r.y
                        else if (contentY + height <= r.y + r.height) contentY = r.y + r.height - height
                    }
                    T.TextArea {
                        id: input
                        width: flick.width
                        // Templates controls have no implicit size; without this the edit stays one
                        // short line tall, clips descenders and never grows.
                        implicitHeight: contentHeight + topPadding + bottomPadding
                        wrapMode: TextEdit.Wrap
                        font: Theme.fBody
                        color: Theme.textPrimary
                        selectionColor: Theme.selection
                        selectedTextColor: Theme.textPrimary
                        placeholderTextColor: Theme.textTertiary
                        placeholderText: !composer.ready ? qsTr("Waiting for omp…")
                                         : composer.busy ? qsTr("Add a follow-up, or Alt+Enter to steer the current turn")
                                         : composer.workspace && composer.workspace.mode === "plan" ? qsTr("Describe what to plan")
                                         : qsTr("Ask, or describe a change. / for commands, @ for files")
                        topPadding: 4
                        bottomPadding: 4
                        leftPadding: 0
                        rightPadding: 0
                        selectByMouse: true
                        persistentSelection: true
                        enabled: composer.workspace !== null
                        focus: true
                        Accessible.role: Accessible.EditableText
                        Accessible.name: qsTr("Message to the agent")
                        onCursorRectangleChanged: flick.ensureVisible(cursorRectangle)
                        GText {
                            anchors.fill: parent
                            topPadding: parent.topPadding
                            text: input.placeholderText
                            color: input.placeholderTextColor
                            lineHeight: 1.0
                            visible: input.length === 0 && !input.preeditText
                            elide: Text.ElideRight
                        }

                        Keys.onPressed: event => {
                            const menu = composer.slashMode ? slashMenu : composer.fileMode ? fileMenu : null
                            if (menu && menu.count > 0) {
                                if (event.key === Qt.Key_Down) { menu.move(1); event.accepted = true; return }
                                if (event.key === Qt.Key_Up) { menu.move(-1); event.accepted = true; return }
                                if (event.key === Qt.Key_Tab || ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && !(event.modifiers & Qt.ShiftModifier))) {
                                    menu.accept(); event.accepted = true; return
                                }
                            }
                            if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                if (event.modifiers & Qt.ShiftModifier) return
                                composer.send((event.modifiers & Qt.AltModifier) !== 0)
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape) {
                                if (composer.busy && input.length === 0) { composer.workspace.stop(); event.accepted = true }
                            } else if (event.key === Qt.Key_Up && input.cursorPosition === 0 && composer.history.length > 0) {
                                composer.historyIndex = Math.min(composer.history.length - 1, composer.historyIndex + 1)
                                input.text = composer.history[composer.historyIndex]
                                event.accepted = true
                            } else if (event.key === Qt.Key_V && (event.modifiers & Qt.ControlModifier)) {
                                const pasted = App.pasteImage()
                                if (pasted.length) { composer.attachments = composer.attachments.concat([pasted]); event.accepted = true }
                            }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s1
                    GIconButton {
                        size: Theme.controlSm
                        iconName: "paperclip"
                        tooltip: composer.workspace && !composer.workspace.modelAcceptsImages && !composer.workspace.mock ? qsTr("This model does not accept images") : qsTr("Attach images")
                        enabled: composer.workspace && (composer.workspace.modelAcceptsImages || composer.workspace.mock)
                        onClicked: imageDialog.open()
                    }
                    GIconButton {
                        size: Theme.controlSm
                        iconName: "at"
                        tooltip: qsTr("Mention a file")
                        onClicked: { input.insert(input.cursorPosition, "@"); input.forceActiveFocus() }
                    }
                    GIconButton {
                        size: Theme.controlSm
                        iconName: "slash"
                        tooltip: qsTr("Run a command")
                        onClicked: { input.text = "/"; input.cursorPosition = 1; input.forceActiveFocus() }
                    }
                    Item { Layout.fillWidth: true }
                    Row {
                        spacing: Theme.s1 + 2
                        visible: frame.width > 520 && input.activeFocus
                        GKbd { sequence: "Return"; anchors.verticalCenter: parent.verticalCenter }
                        GText { text: composer.busy ? qsTr("queue") : qsTr("send"); role: "caption"; tone: "tertiary"; anchors.verticalCenter: parent.verticalCenter }
                        GKbd { sequence: composer.busy ? "Alt+Return" : "Shift+Return"; anchors.verticalCenter: parent.verticalCenter; leftPadding: Theme.s2 }
                        GText { text: composer.busy ? qsTr("steer") : qsTr("new line"); role: "caption"; tone: "tertiary"; anchors.verticalCenter: parent.verticalCenter }
                    }
                    GButton {
                        visible: composer.busy
                        text: qsTr("Stop")
                        iconName: "stop"
                        size: "sm"
                        variant: "danger"
                        Layout.leftMargin: Theme.s2
                        onClicked: composer.workspace.stop()
                        GToolTip { visible: (parent as T.AbstractButton).hovered; text: qsTr("Stop the agent"); shortcut: App.shortcuts.map["agent.stop"] || "" }
                    }
                    GButton {
                        text: composer.busy ? qsTr("Queue") : qsTr("Send")
                        iconName: composer.busy ? "clock" : "arrow-up"
                        size: "sm"
                        variant: composer.busy ? "secondary" : "primary"
                        enabled: composer.ready && (input.length > 0 || composer.attachments.length > 0)
                        onClicked: composer.send(false)
                    }
                }
            }
        }
    }

    CompletionMenu {
        id: slashMenu
        anchors.bottom: col.top
        anchors.bottomMargin: -Theme.s1
        x: col.x
        width: Math.min(520, col.width)
        visible: composer.slashMode && count > 0 && input.activeFocus
        model: composer.workspace ? composer.workspace.commands : null
        titleRole: "name"
        prefix: "/"
        emptyText: qsTr("No matching commands")
        onChosen: row => composer.completeCommand(composer.workspace.commands.nameAt(row))
    }
    CompletionMenu {
        id: fileMenu
        anchors.bottom: col.top
        anchors.bottomMargin: -Theme.s1
        x: col.x
        width: Math.min(560, col.width)
        visible: composer.fileMode && input.activeFocus && (count > 0 || files.indexing)
        model: files
        titleRole: "highlighted"
        richTitles: true
        loading: files.indexing
        emptyText: qsTr("No matching files")
        onChosen: row => composer.completeFile(files.pathAt(row))
    }
}
