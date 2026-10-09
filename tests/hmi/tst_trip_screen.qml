// Verifies: REQ-022

import QtQuick
import QtTest
import LexusHeadUnit

TestCase {
    id: testCase
    name: "TripScreen"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var viewModel: vehicleData
    // SignalId numbers of the eight derived signals, in the order of the trip grid.
    readonly property var tripSignalIndexes: [9, 10, 11, 16, 12, 13, 14, 15]
    readonly property var expectedNames: ["Fuel economy", "Trip fuel economy", "Trip distance",
                                          "Coolant warm-up time", "Time below 1000 rpm",
                                          "Time 1000 to 2499 rpm", "Time 2500 to 3999 rpm",
                                          "Time from 4000 rpm"]
    readonly property var expectedUnits: ["L/100 km", "L/100 km", "km", "min", "min", "min",
                                          "min", "min"]

    Component {
        id: screensComponent
        Screens {
            width: 1280
            height: 720
            vehicleData: testCase.viewModel
        }
    }

    function openTrip() {
        var screens = createTemporaryObject(screensComponent, testCase)
        verify(screens !== null, "screens created")
        var button = findChild(findChild(screens, "homeScreen"), "tripButton")
        verify(button !== null && button.visible, "Trip button on Home")
        verify(button.height >= Sizes.mm(10) && button.width >= Sizes.mm(10), "10 mm target")
        mouseClick(button)
        compare(screens.tripShown, true)
        var trip = findChild(screens, "tripScreen")
        compare(trip.visible, true)
        compare(findChild(screens, "homeScreen").visible, false)
        return { screens: screens, trip: trip }
    }

    function test_eight_trip_tiles_with_names_and_units() {
        var opened = openTrip()
        for (var index = 0; index < 8; ++index) {
            var item = findChild(opened.trip, "tile" + index)
            verify(item !== null, "tile" + index)
            compare(findChild(item, "nameText").text, expectedNames[index])
            compare(findChild(item, "unitText").text, expectedUnits[index])
        }
        verify(findChild(opened.trip, "tile8") === null, "no ninth tile")
    }

    function test_values_show_one_decimal() {
        var opened = openTrip()
        vehicleData.simulateValid(tripSignalIndexes[0], 8.04)
        vehicleData.simulateValid(tripSignalIndexes[2], 12.345)
        vehicleData.simulateValid(tripSignalIndexes[3], 6.0)
        compare(findChild(findChild(opened.trip, "tile0"), "valueText").text, "8.0")
        compare(findChild(findChild(opened.trip, "tile2"), "valueText").text, "12.3")
        compare(findChild(findChild(opened.trip, "tile3"), "valueText").text, "6.0")
    }

    function test_home_from_the_trip_strip() {
        var opened = openTrip()
        mouseClick(findChild(findChild(opened.trip, "statusStrip"), "navigationButton"))
        compare(opened.screens.tripShown, false)
        compare(findChild(opened.screens, "homeScreen").visible, true)
    }
}
