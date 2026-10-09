import QtQuick

// The hub's status strip (DN-021, DN-025): the vehicle connection from the vehicle-data
// service, the hub's own state (shutdown prompt, the app in front, the last exit, a registry
// error), the firmware power flags, and the shutdown button (a 10 mm touch target).
// connection, power and shutdown may be null; their parts are then hidden.
Rectangle {
    id: strip

    required property var hub
    property var connection: null
    property var power: null
    property var shutdown: null
    readonly property bool showsError: !hub.appRunning && hub.registryErrorText !== ""
    readonly property bool powerProblem: power !== null && (!power.available || power.underVoltage
                                         || power.frequencyCapped || power.throttled
                                         || power.softTemperatureLimit)

    height: Sizes.statusStripHeight
    color: Sizes.tileBackground

    Rectangle {
        id: connectionDot
        objectName: "connectionDot"
        visible: strip.connection !== null
        anchors.left: parent.left
        anchors.leftMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        width: Sizes.mm(3)
        height: width
        radius: width / 2
        color: strip.connection === null ? Sizes.disconnected
             : strip.connection.isConnected ? Sizes.connected
             : strip.connection.isError ? Sizes.error
             : strip.connection.stateText === "Connecting" ? Sizes.connecting
             : Sizes.disconnected
    }

    Text {
        id: connectionText
        objectName: "connectionText"
        visible: strip.connection !== null
        anchors.left: connectionDot.right
        anchors.leftMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        text: strip.connection === null ? "" : strip.connection.stateText
        color: Sizes.liveValue
        font.pixelSize: Sizes.labelPixelSize
        font.bold: true
    }

    Text {
        id: titleText
        anchors.left: strip.connection === null ? parent.left : connectionText.right
        anchors.leftMargin: Sizes.gutter * 2
        anchors.verticalCenter: parent.verticalCenter
        text: "Lexus Head Unit"
        color: Sizes.liveValue
        font.pixelSize: Sizes.unitPixelSize
        font.bold: true
    }

    Text {
        objectName: "hubStatusText"
        anchors.left: titleText.right
        anchors.right: powerDot.left
        anchors.leftMargin: Sizes.gutter * 2
        anchors.rightMargin: Sizes.gutter * 2
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        text: strip.shutdown !== null && strip.shutdown.lastResultText !== ""
              ? strip.shutdown.lastResultText
            : strip.hub.appRunning ? strip.hub.foregroundAppName + " running"
            : strip.showsError ? strip.hub.registryErrorText
            : strip.hub.lastExitText
        color: strip.showsError ? Sizes.error : Sizes.label
        font.pixelSize: Sizes.labelPixelSize
    }

    // Amber when any flag has been set since boot, even if it has cleared.
    Rectangle {
        id: powerDot
        objectName: "powerOccurredDot"
        visible: strip.power !== null && strip.power.occurredSinceBoot
        anchors.right: powerText.left
        anchors.rightMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        width: visible ? Sizes.mm(2) : 0
        height: Sizes.mm(2)
        radius: height / 2
        color: Sizes.connecting
    }

    Text {
        id: powerText
        objectName: "powerText"
        visible: strip.power !== null
        anchors.right: shutdownButton.visible ? shutdownButton.left : parent.right
        anchors.rightMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        text: strip.power === null ? "" : strip.power.flagsText
        color: strip.powerProblem ? Sizes.error : Sizes.label
        font.pixelSize: Sizes.labelPixelSize
        font.bold: strip.powerProblem
    }

    Rectangle {
        id: shutdownButton
        objectName: "shutdownButton"
        visible: strip.shutdown !== null
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Sizes.touchTarget
        height: Sizes.touchTarget
        radius: Sizes.tileRadius
        color: strip.shutdown !== null && strip.shutdown.armed ? Sizes.error
             : shutdownArea.pressed ? Sizes.tileBorder
             : Sizes.background

        Text {
            anchors.centerIn: parent
            text: "⏻"
            color: Sizes.liveValue
            font.pixelSize: Sizes.unitPixelSize
        }

        MouseArea {
            id: shutdownArea
            anchors.fill: parent
            onClicked: strip.shutdown.press()
        }
    }
}
