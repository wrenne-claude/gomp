pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import Gomp

// A tinted stroke icon from resources/icons, rasterised at the screen's pixel density.
Image {
    id: icon
    property string name
    property color color: Theme.textSecondary
    property int size: Theme.iconMd

    width: size
    height: size
    visible: name.length > 0
    source: name.length > 0 ? "image://icons/" + name + "/" + color.toString() : ""
    sourceSize: Qt.size(Math.ceil(size * Screen.devicePixelRatio), Math.ceil(size * Screen.devicePixelRatio))
    fillMode: Image.PreserveAspectFit
    smooth: true
    asynchronous: false
    cache: true
    Accessible.ignored: true
}
