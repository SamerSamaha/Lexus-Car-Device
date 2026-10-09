// Verifies: REQ-012, REQ-006

import QtQuick
import QtTest
import LexusHeadUnit

TestCase {
    id: testCase
    name: "VehicleDataScreen"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var viewModel: vehicleData
    readonly property var expectedNames: ["Vehicle speed", "Engine speed", "Coolant temperature",
                                          "Engine load", "Throttle position",
                                          "Intake air temperature", "Control module voltage",
                                          "Fuel level"]
    readonly property var expectedUnits: ["km/h", "rpm", "degC", "%", "%", "degC", "V", "%"]

    Component {
        id: screenComponent
        VehicleDataScreen {
            width: 1280
            height: 720
            vehicleData: testCase.viewModel
        }
    }

    function createScreen() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen !== null, "screen created")
        return screen
    }

    function tile(screen, index) {
        var item = findChild(screen, "tile" + index)
        verify(item !== null, "tile" + index + " exists")
        return item
    }

    function test_eight_tiles_in_id_order_with_names_and_units() {
        var screen = createScreen()
        for (var index = 0; index < 8; ++index) {
            var item = tile(screen, index)
            compare(findChild(item, "nameText").text, expectedNames[index])
            compare(findChild(item, "unitText").text, expectedUnits[index])
        }
        verify(findChild(screen, "tile8") === null, "no ninth tile")
    }

    function test_every_signal_shows_valid_stale_and_never_received() {
        var screen = createScreen()
        for (var index = 0; index < 8; ++index) {
            var item = tile(screen, index)
            var valueText = findChild(item, "valueText")
            var badge = findChild(item, "staleBadge")

            vehicleData.simulateValid(index, 10 + index)
            compare(valueText.text, index === 6 ? "16.0" : String(10 + index))
            compare(valueText.color, Sizes.liveValue)
            compare(badge.visible, false)

            vehicleData.simulateStale(index, 10 + index)
            compare(valueText.text, index === 6 ? "16.0" : String(10 + index))
            verify(valueText.color !== Sizes.liveValue, "stale tile " + index + " is not live")
            compare(badge.visible, true)

            vehicleData.simulateNeverReceived(index)
            compare(valueText.text, "--")
            verify(valueText.color !== Sizes.liveValue, "never received tile " + index + " is not live")
            compare(badge.visible, false)
        }
    }

    function test_sizes_follow_the_millimetre_rules() {
        var screen = createScreen()
        for (var index = 0; index < 8; ++index) {
            var item = tile(screen, index)
            verify(item.width >= Sizes.mm(22), "tile " + index + " at least 22 mm wide: " + item.width)
            verify(item.height >= Sizes.mm(20), "tile " + index + " at least 20 mm tall: " + item.height)
            verify(findChild(item, "valueText").font.pixelSize >= Sizes.mm(4), "value at least 4 mm")
        }
        var button = findChild(findChild(screen, "statusStrip"), "navigationButton")
        verify(button.visible, "home button visible")
        verify(button.width >= Sizes.mm(10), "home button at least 10 mm wide")
        verify(button.height >= Sizes.mm(10), "home button at least 10 mm tall")
    }

    function test_home_button_emits_the_request() {
        var screen = createScreen()
        var requested = 0
        screen.homeRequested.connect(function() { requested += 1 })
        mouseClick(findChild(findChild(screen, "statusStrip"), "navigationButton"))
        compare(requested, 1)
    }
}
