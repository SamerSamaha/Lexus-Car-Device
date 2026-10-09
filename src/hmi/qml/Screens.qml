import QtQuick

// The screen switch: Home first; Vehicle data or Diagnostics on request; Home again from their
// strips. Diagnostics exists only when a diagnostics view model is given (DN-030).
Item {
    id: screens

    required property var vehicleData
    property var diagnostics: null
    property var power: null
    property bool vehicleDataShown: false
    property bool diagnosticsShown: false

    HomeScreen {
        id: homeScreen
        objectName: "homeScreen"
        anchors.fill: parent
        visible: !screens.vehicleDataShown && !screens.diagnosticsShown
        vehicleData: screens.vehicleData
        diagnosticsAvailable: screens.diagnostics !== null
        onVehicleDataRequested: screens.vehicleDataShown = true
        onDiagnosticsRequested: {
            screens.diagnosticsShown = true
            screens.diagnostics.refresh()
        }
    }

    VehicleDataScreen {
        id: vehicleDataScreen
        objectName: "vehicleDataScreen"
        anchors.fill: parent
        visible: screens.vehicleDataShown
        vehicleData: screens.vehicleData
        onHomeRequested: screens.vehicleDataShown = false
    }

    Loader {
        id: diagnosticsLoader
        objectName: "diagnosticsLoader"
        anchors.fill: parent
        active: screens.diagnostics !== null
        visible: screens.diagnosticsShown
        sourceComponent: DiagnosticsScreen {
            objectName: "diagnosticsScreen"
            vehicleData: screens.vehicleData
            diagnostics: screens.diagnostics
            power: screens.power
            onHomeRequested: screens.diagnosticsShown = false
        }
    }
}
