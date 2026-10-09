import QtQuick

// Connection state and its last cause, across the top of every screen.
Rectangle {
    id: strip

    required property var connection

    readonly property color stateColor: connection.isConnected ? Sizes.connected
                                      : connection.isError ? Sizes.error
                                      : connection.stateText === "Connecting" ? Sizes.connecting
                                      : Sizes.disconnected

    height: Sizes.statusStripHeight
    color: Sizes.tileBackground

    Rectangle {
        id: stateDot
        objectName: "stateDot"
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Sizes.gutter
        width: Sizes.mm(3)
        height: width
        radius: width / 2
        color: strip.stateColor
    }

    Text {
        id: stateText
        objectName: "stateText"
        anchors.left: stateDot.right
        anchors.leftMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        text: strip.connection.stateText
        color: Sizes.liveValue
        font.pixelSize: Sizes.unitPixelSize
        font.bold: true
    }

    Text {
        id: causeText
        objectName: "causeText"
        anchors.left: stateText.right
        anchors.leftMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        text: strip.connection.lastCauseText
        color: Sizes.label
        font.pixelSize: Sizes.labelPixelSize
    }

    Text {
        id: titleText
        anchors.right: parent.right
        anchors.rightMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        text: "Lexus Head Unit"
        color: Sizes.label
        font.pixelSize: Sizes.labelPixelSize
    }
}
