pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import Gomp
import Gomp.Core

// A list of omp settings: one tab, an explicit id list, or search results, grouped by section.
Item {
    id: page
    property string title: ""
    property string subtitle: ""
    // Custom content above the list (curated pages): a Component, loaded into the list header.
    property Component headerComponent: null
    property var ids: []

    function configure(pageId, query) {
        if (query.length) {
            App.settings.query = query
            return
        }
        App.settings.query = ""
        if (page.ids.length) {
            App.settings.ids = page.ids
        } else if (pageId.startsWith("tab:")) {
            const tab = pageId.substring(4)
            App.settings.section = tab
            const s = App.settings.sections.find(x => x.id === tab)
            page.title = s ? s.title : tab
            page.subtitle = qsTr("omp's own settings, with the labels and descriptions omp ships.")
        }
    }

    ListView {
        id: list
        anchors.fill: parent
        model: App.settings
        clip: true
        reuseItems: true
        leftMargin: Theme.s5
        rightMargin: Theme.s5
        bottomMargin: Theme.s5
        boundsBehavior: Flickable.StopAtBounds
        T.ScrollBar.vertical: GScrollBar {}
        header: Loader {
            width: list.width - list.leftMargin - list.rightMargin
            sourceComponent: page.headerComponent
            height: item ? (item as Item).implicitHeight + Theme.s4 : 0
        }
        section.property: "group"
        section.criteria: ViewSection.FullString
        section.delegate: Item {
            id: d51
            required property string section
            width: list.width - list.leftMargin - list.rightMargin
            height: 44
            GText { x: Theme.s4; anchors.bottom: parent.bottom; anchors.bottomMargin: Theme.s1; text: d51.section; role: "caption"; tone: "tertiary" }
            GDivider { anchors.bottom: parent.bottom; x: Theme.s4; width: parent.width - Theme.s4 * 2; opacity: 0.6 }
        }
        delegate: SettingRow { width: list.width - list.leftMargin - list.rightMargin }
        GEmptyState {
            anchors.centerIn: parent
            visible: list.count === 0
            iconName: App.settings.loaded ? "search" : "sliders"
            title: App.settings.loaded ? qsTr("No settings match") : qsTr("Reading omp's settings")
            body: App.settings.loaded ? qsTr("Try a different word, or a setting key such as tools.approvalMode.") : ""
        }
    }
}
