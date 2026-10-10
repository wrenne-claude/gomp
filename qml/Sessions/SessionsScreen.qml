pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// Every omp session on this machine: search, filter by project, sort; details, branches and
// actions on the right. Narrow windows show one pane at a time.
Item {
    id: screen
    readonly property bool twoPane: width >= 1000
    property string selectedPath: ""
    readonly property var selected: selectedPath.length ? App.sessions.get(selectedPath) : ({})

    Connections {
        target: Ui
        function onSelectSessionRequested(path) { screen.selectedPath = path }
    }
    SessionFilterModel {
        id: filtered
        sourceModel: App.sessions
    }
    onVisibleChanged: if (visible) App.sessions.rescan()
    Component.onCompleted: App.sessions.rescan()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- List pane ----
        ColumnLayout {
            Layout.fillHeight: true
            Layout.preferredWidth: screen.twoPane ? Math.min(480, screen.width * 0.4) : screen.width
            Layout.fillWidth: !screen.twoPane // nested layouts default to filling; keep this pane fixed
            visible: screen.twoPane || screen.selectedPath.length === 0
            spacing: 0

            ColumnLayout {
                Layout.fillWidth: true
                Layout.margins: Theme.s4
                Layout.bottomMargin: Theme.s3
                spacing: Theme.s3
                RowLayout {
                    Layout.fillWidth: true
                    GText { text: qsTr("Sessions"); role: "title"; Layout.fillWidth: true }
                    GText {
                        text: App.sessions.scanning ? qsTr("Scanning…") : Fmt.count(filtered.count, qsTr("session"), qsTr("sessions"))
                        role: "caption"
                        tone: "tertiary"
                    }
                    GIconButton { iconName: "refresh"; tooltip: qsTr("Rescan"); onClicked: App.sessions.rescan() }
                }
                GSearchField {
                    id: search
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search titles, prompts, replies and models")
                    onTextChanged: filtered.query = text
                    Keys.onDownPressed: list.forceActiveFocus()
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.s2
                    GDropdown {
                        Layout.fillWidth: true
                        Layout.maximumWidth: 220
                        value: filtered.project
                        placeholder: qsTr("All projects")
                        options: [{ value: "", label: qsTr("All projects") }].concat(App.sessions.projects.map(p => ({ value: p, label: p })))
                        onPicked: v => filtered.project = v
                    }
                    Item { Layout.fillWidth: true }
                    GSegmented {
                        size: "sm"
                        value: filtered.sortKey
                        options: [
                            { value: "modified", label: qsTr("Recent") },
                            { value: "created", label: qsTr("Created") },
                            { value: "messages", label: qsTr("Longest") },
                            { value: "title", label: qsTr("A–Z") }
                        ]
                        onActivated: v => filtered.sortKey = v
                    }
                }
            }
            GDivider { Layout.fillWidth: true }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                model: filtered
                clip: true
                reuseItems: true
                spacing: 2
                topMargin: Theme.s2
                bottomMargin: Theme.s2
                leftMargin: Theme.s2
                rightMargin: Theme.s2
                keyNavigationEnabled: true
                highlightMoveDuration: 0
                boundsBehavior: Flickable.StopAtBounds
                T.ScrollBar.vertical: GScrollBar {}
                onCurrentIndexChanged: if (activeFocus && currentIndex >= 0) screen.selectedPath = filtered.get(currentIndex).path
                Keys.onReturnPressed: if (currentIndex >= 0) screen.resume(filtered.get(currentIndex))
                delegate: SessionRow {
                    width: list.width - list.leftMargin - list.rightMargin
                    selected: screen.selectedPath === path
                    open: App.workspaces.indexOfSession(path) >= 0
                    onClicked: { list.currentIndex = index; screen.selectedPath = path }
                    onDoubleClicked: screen.resume(filtered.get(index))
                }
                // Skeleton rows while the first scan runs.
                Column {
                    anchors.fill: parent
                    anchors.margins: Theme.s3
                    spacing: Theme.s3
                    visible: App.sessions.scanning && list.count === 0
                    Repeater {
                        model: 6
                        Column {
                            spacing: Theme.s2
                            GSkeleton { width: 220; height: 12 }
                            GSkeleton { width: list.width * 0.7; height: 10 }
                        }
                    }
                }
                GEmptyState {
                    anchors.centerIn: parent
                    visible: !App.sessions.scanning && list.count === 0
                    iconName: search.text.length ? "search" : "history"
                    title: search.text.length || filtered.project.length ? qsTr("No sessions match") : qsTr("No sessions yet")
                    body: search.text.length || filtered.project.length ? qsTr("Try fewer words, or show all projects.")
                                                                         : qsTr("Sessions appear here as soon as you talk to omp in any project.")
                    GButton {
                        visible: search.text.length > 0 || filtered.project.length > 0
                        text: qsTr("Clear filters")
                        onClicked: { search.text = ""; filtered.project = "" }
                    }
                }
            }
        }
        GDivider { vertical: true; Layout.fillHeight: true; visible: screen.twoPane }

        // ---- Detail pane ----
        SessionDetail {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: screen.twoPane || screen.selectedPath.length > 0
            session: screen.selected
            showBack: !screen.twoPane
            onBack: screen.selectedPath = ""
            onResumeRequested: screen.resume(screen.selected)
            onDeleted: screen.selectedPath = ""
        }
    }

    function resume(s) {
        if (!s || !s.path) return
        App.openSession(s.path, s.cwd)
        Ui.go("agent")
    }
}
