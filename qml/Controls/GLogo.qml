pragma ComponentBehavior: Bound
import QtQuick
import Gomp

// The gomp logo. It is 10x10 pixel art, so it is scaled with nearest-neighbour sampling; sizes
// that are multiples of 10 keep every pixel the same size.
Image {
    property int size: 20
    width: size
    height: size
    source: "qrc:/qt/qml/Gomp/brand/gomp.png"
    sourceSize: Qt.size(10, 10)
    fillMode: Image.PreserveAspectFit
    smooth: false
    mipmap: false
    Accessible.role: Accessible.Graphic
    Accessible.name: "gomp"
}
