import QtQuick
import Gomp

Rectangle {
    property bool vertical: false
    implicitWidth: vertical ? 1 : 0
    implicitHeight: vertical ? 0 : 1
    color: Theme.hairline
}
