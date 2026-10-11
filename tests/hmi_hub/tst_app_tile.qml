// Verifies: REQ-016, REQ-028

import QtQuick
import QtTest
import LexusHub

// App names on a hub tile of the car registry's size are never cut (LHU-051: the car showed
// "Replay last dr..."): they wrap to two lines and shrink before anything is lost.
TestCase {
    id: testCase
    name: "AppTile"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    // The longest names of deploy/car/hub_car.conf and two longer ones.
    readonly property var names: ["Replay last drive", "Screen recording", "Pair adapter",
                                  "Vehicle data", "Phone screen mirroring",
                                  "Bluetooth audio to the car"]

    Component {
        id: tileComponent
        AppTile {
            // A cell of the 4 x 2 grid under the strip and the address line, less the gutter.
            width: 290
            height: 205
        }
    }

    function test_names_wrap_and_are_never_cut() {
        for (var index = 0; index < names.length; ++index) {
            var tile = createTemporaryObject(tileComponent, testCase, {appName: names[index]})
            verify(tile !== null)
            var name = findChild(tile, "appName")
            waitForRendering(tile)
            compare(name.truncated, false, names[index] + " is not cut")
            verify(name.lineCount <= 2, names[index] + " takes at most two lines")
            var icon = findChild(tile, "appIcon")
            verify(icon.y >= 0, names[index] + ": the icon stays inside the tile")
            verify(icon.width >= Sizes.mm(6), "the icon keeps at least 6 mm")
        }
    }

    function test_the_whole_tile_stays_a_10_mm_target() {
        var tile = createTemporaryObject(tileComponent, testCase, {appName: "Replay last drive"})
        verify(tile.width >= Sizes.mm(10) && tile.height >= Sizes.mm(10))
    }
}
