import QtQuick

// The link detail in large text (DN-042), then the connection state and its last cause, across
// the top of every screen, with an optional navigation button (a 10 mm touch target) at the right.
Rectangle {
    id: strip

    required property var connection
    property string navigationText: ""
    signal navigationRequested()

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
        id: detailText
        objectName: "detailText"
        anchors.left: stateDot.right
        anchors.leftMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        text: strip.connection.detailText
        color: Sizes.detailColour(strip.connection.detailName)
        font.pixelSize: Sizes.detailPixelSize
        font.bold: true
    }

    Text {
        id: stateText
        objectName: "stateText"
        anchors.left: detailText.right
        anchors.leftMargin: Sizes.gutter * 2
        anchors.verticalCenter: parent.verticalCenter
        text: strip.connection.stateText
        color: Sizes.label
        font.pixelSize: Sizes.labelPixelSize
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

    Rectangle {
        id: navigationButton
        objectName: "navigationButton"
        visible: strip.navigationText !== ""
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.rightMargin: Sizes.gutter / 2
        width: Math.max(Sizes.touchTarget, navigationLabel.implicitWidth + Sizes.gutter * 2)
        radius: Sizes.tileRadius
        color: navigationArea.pressed ? Sizes.tileBorder : Sizes.background
        border.color: Sizes.tileBorder
        border.width: 1

        Text {
            id: navigationLabel
            anchors.centerIn: parent
            text: strip.navigationText
            color: Sizes.liveValue
            font.pixelSize: Sizes.unitPixelSize
            font.bold: true
        }

        MouseArea {
            id: navigationArea
            anchors.fill: parent
            onClicked: strip.navigationRequested()
        }
    }
}
