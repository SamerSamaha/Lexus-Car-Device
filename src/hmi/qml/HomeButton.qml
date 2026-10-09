import QtQuick

// One of the buttons along the bottom of Home: at least 10 mm high, the label centred.
Rectangle {
    id: button

    required property string label
    signal activated()

    height: Sizes.touchTarget
    radius: Sizes.tileRadius
    color: area.pressed ? Sizes.tileBorder : Sizes.tileBackground
    border.color: Sizes.tileBorder
    border.width: 1

    Text {
        anchors.centerIn: parent
        text: button.label
        color: Sizes.liveValue
        font.pixelSize: Sizes.unitPixelSize
        font.bold: true
    }

    MouseArea {
        id: area
        anchors.fill: parent
        onClicked: button.activated()
    }
}
