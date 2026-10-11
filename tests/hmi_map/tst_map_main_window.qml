// Verifies: REQ-030

import QtQuick
import QtTest
import LexusMap

// The map window as lexus-map creates it: the model set as a context property must reach the
// screen through Main.qml (lesson of LHU-047), and recording near home must cover the map there.
TestCase {
    id: testCase
    name: "MapMainWindow"
    visible: true
    when: windowShown

    Component {
        id: mainComponent
        Main {}
    }

    function test_the_model_reaches_the_screen_and_the_mask_works_in_the_window() {
        mapDriverContext.setRecording(false)
        var window = createTemporaryObject(mainComponent, testCase)
        verify(window !== null)
        var screen = findChild(window.contentItem, "mapScreen")
        verify(screen !== null)
        compare(screen.map, mapContext)
        mapDriverContext.deliver(43.5, -87.0, 5, true)
        mapDriverContext.setRecording(true)
        compare(findChild(screen, "mapCover").visible, true)
        compare(findChild(screen, "coverText").text, "Position hidden near home")
        mapDriverContext.setRecording(false)
    }
}
