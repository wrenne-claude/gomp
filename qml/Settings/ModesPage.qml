import QtQuick
import Gomp
import Gomp.Core

// Plan, goal, prewalk and advisor: the settings behind gomp's mode switcher.
GenericPage {
    title: qsTr("Modes")
    subtitle: qsTr("How Plan and Goal behave, and the helpers that run alongside the agent.")
    Component.onCompleted: ids = App.settings.idsWithPrefix(["plan.", "goal.", "prewalk.", "advisor.", "loop."])
    function configure(pageId, query) {
        if (query.length) { App.settings.query = query; return }
        App.settings.query = ""
        App.settings.ids = ids
    }
}
