// Verifies: REQ-012

import QtQuick
import QtTest
import LexusHeadUnit

TestCase {
    id: testCase
    name: "Screens"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var viewModel: vehicleData

    Component {
        id: screensComponent
        Screens {
            width: 1280
            height: 720
            vehicleData: testCase.viewModel
        }
    }

    function test_home_first_then_vehicle_data_then_home_again() {
        var screens = createTemporaryObject(screensComponent, testCase)
        verify(screens !== null)
        var home = findChild(screens, "homeScreen")
        var vehicle = findChild(screens, "vehicleDataScreen")
        compare(home.visible, true)
        compare(vehicle.visible, false)

        mouseClick(findChild(home, "vehicleDataButton"))
        compare(screens.vehicleDataShown, true)
        compare(home.visible, false)
        compare(vehicle.visible, true)

        mouseClick(findChild(findChild(vehicle, "statusStrip"), "navigationButton"))
        compare(screens.vehicleDataShown, false)
        compare(home.visible, true)
        compare(vehicle.visible, false)
    }
}
