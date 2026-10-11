// Verifies: REQ-030

import QtQuick
import QtTest
import LexusMap

// The map screen with the real MapViewModel (DN-054). The QtLocation map itself is loaded only
// where the module is installed (the Pi); everywhere the cover hides the map area until there is
// a position, and whenever the position is near home while recording.
TestCase {
    id: testCase
    name: "MapScreen"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var map: mapContext
    readonly property var driver: mapDriverContext
    // The invented home of the test set-up and a place 2 km north of it.
    readonly property real homeLatitude: 43.5
    readonly property real homeLongitude: -87.0
    readonly property real awayLatitude: 43.52

    Component {
        id: screenComponent
        MapScreen {
            width: 1280
            height: 720
            map: testCase.map
        }
    }

    function createScreen() {
        driver.setRecording(false)
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen !== null)
        return screen
    }

    function test_a_position_away_from_home_is_shown_with_its_accuracy() {
        var screen = createScreen()
        driver.deliver(awayLatitude, homeLongitude, 7, true)
        compare(map.hasPosition, true)
        compare(findChild(screen, "statusText").text, "Phone position, 7 m")
        compare(screen.mapShown, true)
        var loader = findChild(screen, "mapLoader")
        tryVerify(function() { return loader.status === Loader.Ready || loader.status === Loader.Error })
        // Without QtLocation (the desk) the cover says so; with it the map is drawn.
        var cover = findChild(screen, "mapCover")
        compare(cover.visible, loader.status !== Loader.Ready)
        if (loader.status === Loader.Error) {
            compare(findChild(screen, "coverText").text,
                    "The map module is not installed (qml6-module-qtlocation)")
        }
    }

    function test_near_home_while_recording_the_map_is_covered_and_says_why() {
        var screen = createScreen()
        driver.deliver(homeLatitude + 0.001, homeLongitude, 5, true)
        compare(screen.mapShown, true)
        driver.setRecording(true)
        compare(map.masked, true)
        compare(screen.mapShown, false)
        compare(findChild(screen, "mapLoader").visible, false)
        compare(findChild(screen, "mapCover").visible, true)
        compare(findChild(screen, "coverText").text, "Position hidden near home")
        compare(findChild(screen, "statusText").text, "Position hidden near home")
        driver.setRecording(false)
        compare(screen.mapShown, true)
    }

    function test_a_lost_position_turns_the_dot_amber_and_says_so() {
        var screen = createScreen()
        driver.deliver(awayLatitude, homeLongitude, 5, true)
        compare(findChild(screen, "liveDot").color, Sizes.connected)
        driver.deliver(awayLatitude, homeLongitude, 5, false)
        compare(findChild(screen, "liveDot").color, Sizes.connecting)
        compare(findChild(screen, "statusText").text,
                "Phone position lost: keep the page open on the phone")
    }

    function test_the_hub_button_is_a_10_mm_target_that_asks_to_leave() {
        var screen = createScreen()
        var button = findChild(screen, "hubButton")
        verify(button.width >= Sizes.mm(10) && button.height >= Sizes.mm(10))
        var requested = 0
        screen.hubRequested.connect(function() { requested += 1 })
        mouseClick(button)
        compare(requested, 1)
    }
}
