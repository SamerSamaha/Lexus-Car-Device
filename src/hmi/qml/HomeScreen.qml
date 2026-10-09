import QtQuick

// Home: the status strip, two primary values and the button to the vehicle-data screen.
Item {
    id: home

    required property var vehicleData
    signal vehicleDataRequested()

    StatusStrip {
        id: strip
        objectName: "statusStrip"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        connection: home.vehicleData.connection
    }

    Row {
        id: primaryValues
        anchors.top: strip.bottom
        anchors.bottom: vehicleDataButton.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        spacing: Sizes.gutter

        SignalTile {
            id: speedTile
            objectName: "speedTile"
            width: (primaryValues.width - primaryValues.spacing) / 2
            height: primaryValues.height
            model: home.vehicleData.vehicleSpeed
        }

        SignalTile {
            id: rpmTile
            objectName: "rpmTile"
            width: (primaryValues.width - primaryValues.spacing) / 2
            height: primaryValues.height
            model: home.vehicleData.engineRpm
        }
    }

    Rectangle {
        id: vehicleDataButton
        objectName: "vehicleDataButton"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        height: Sizes.touchTarget
        radius: Sizes.tileRadius
        color: buttonArea.pressed ? Sizes.tileBorder : Sizes.tileBackground
        border.color: Sizes.tileBorder
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "Vehicle data"
            color: Sizes.liveValue
            font.pixelSize: Sizes.unitPixelSize
            font.bold: true
        }

        MouseArea {
            id: buttonArea
            anchors.fill: parent
            onClicked: home.vehicleDataRequested()
        }
    }
}
