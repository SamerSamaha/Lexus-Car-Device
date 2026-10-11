// Verifies: REQ-027

import QtQuick
import QtTest
import LexusHeadUnit

// The vehicle-data and trip grids with a view model built for US units, as lexus-head-unit
// builds it in car mode: unit texts and converted values on screen (DN-050).
TestCase {
    id: testCase
    name: "UsUnits"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var expectedGridUnits: ["mph", "rpm", "°F", "%", "%", "°F", "V"]
    readonly property var expectedTripUnits: ["mpg", "mpg", "mi", "min", "min", "min", "min", "min"]

    Component {
        id: gridComponent
        VehicleDataScreen {
            width: 1280
            height: 720
            vehicleData: vehicleDataUs
        }
    }

    Component {
        id: tripComponent
        VehicleDataScreen {
            width: 1280
            height: 720
            vehicleData: vehicleDataUs
            tiles: vehicleDataUs.tripTiles
        }
    }

    function test_the_grid_shows_us_units_and_converted_values() {
        var screen = createTemporaryObject(gridComponent, testCase)
        verify(screen !== null)
        compare(vehicleDataUs.usUnits, true)
        for (var index = 0; index < expectedGridUnits.length; ++index) {
            compare(findChild(findChild(screen, "tile" + index), "unitText").text,
                    expectedGridUnits[index], "unit of tile " + index)
        }
        vehicleDataUs.simulateValid(0, 100.0)
        vehicleDataUs.simulateValid(2, 90.0)
        tryCompare(findChild(findChild(screen, "tile0"), "valueText"), "text", "62", 500)
        tryCompare(findChild(findChild(screen, "tile2"), "valueText"), "text", "194", 500)
    }

    function test_the_trip_grid_shows_miles_and_miles_per_gallon() {
        var screen = createTemporaryObject(tripComponent, testCase)
        verify(screen !== null)
        for (var index = 0; index < expectedTripUnits.length; ++index) {
            compare(findChild(findChild(screen, "tile" + index), "unitText").text,
                    expectedTripUnits[index], "unit of trip tile " + index)
        }
    }
}
