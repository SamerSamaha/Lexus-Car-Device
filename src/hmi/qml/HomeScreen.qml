import QtQuick

// Home: the status strip, two primary values and the buttons to the other screens.
Item {
    id: home

    required property var vehicleData
    property bool diagnosticsAvailable: false
    // Car mode (DN-043): a last button that leaves the app for the hub.
    property bool hubButtonShown: false
    signal vehicleDataRequested()
    signal hubRequested()
    signal tripRequested()
    signal diagnosticsRequested()

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
        anchors.bottom: buttonRow.top
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

    // Positions are bindings, not a Row: a Row places its children only on the next polish, so a
    // tap in the first frame could land on a button still stacked at x = 0.
    Item {
        id: buttonRow
        objectName: "buttonRow"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        height: Sizes.touchTarget

        readonly property int buttonCount: 2 + (home.diagnosticsAvailable ? 1 : 0)
                                           + (home.hubButtonShown ? 1 : 0)
        readonly property real buttonWidth:
            (width - (buttonCount - 1) * Sizes.gutter) / buttonCount
        readonly property real step: buttonWidth + Sizes.gutter

        HomeButton {
            id: vehicleDataButton
            objectName: "vehicleDataButton"
            x: 0
            width: buttonRow.buttonWidth
            label: "Vehicle data"
            onActivated: home.vehicleDataRequested()
        }

        HomeButton {
            id: tripButton
            objectName: "tripButton"
            x: buttonRow.step
            width: buttonRow.buttonWidth
            label: "Trip"
            onActivated: home.tripRequested()
        }

        HomeButton {
            id: diagnosticsButton
            objectName: "diagnosticsButton"
            visible: home.diagnosticsAvailable
            x: 2 * buttonRow.step
            width: buttonRow.buttonWidth
            label: "Diagnostics"
            onActivated: home.diagnosticsRequested()
        }

        HomeButton {
            id: hubButton
            objectName: "hubButton"
            visible: home.hubButtonShown
            x: (buttonRow.buttonCount - 1) * buttonRow.step
            width: buttonRow.buttonWidth
            label: "Hub"
            onActivated: home.hubRequested()
        }
    }
}
