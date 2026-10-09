import QtQuick
import QtQuick.Window

// The vehicle-data application window. The panel is 720 x 1280 rotated to landscape by the
// compositor (D-045), so the window is 1280 x 720. vehicleDataContext is the context property
// set by the application.
Window {
    id: window
    width: 1280
    height: 720
    visible: true
    color: Sizes.background
    title: "Lexus Head Unit"

    property var vehicleData: typeof vehicleDataContext !== "undefined" ? vehicleDataContext : null

    Screens {
        id: screens
        objectName: "screens"
        anchors.fill: parent
        vehicleData: window.vehicleData
    }
}
