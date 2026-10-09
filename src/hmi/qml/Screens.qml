import QtQuick

// The screen switch: Home first, Vehicle data on request, Home again from its strip.
Item {
    id: screens

    required property var vehicleData
    property bool vehicleDataShown: false

    HomeScreen {
        id: homeScreen
        objectName: "homeScreen"
        anchors.fill: parent
        visible: !screens.vehicleDataShown
        vehicleData: screens.vehicleData
        onVehicleDataRequested: screens.vehicleDataShown = true
    }

    VehicleDataScreen {
        id: vehicleDataScreen
        objectName: "vehicleDataScreen"
        anchors.fill: parent
        visible: screens.vehicleDataShown
        vehicleData: screens.vehicleData
        onHomeRequested: screens.vehicleDataShown = false
    }
}
