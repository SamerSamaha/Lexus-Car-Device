// Verifies: REQ-016

import QtQuick
import QtTest
import LexusHub

TestCase {
    id: testCase
    name: "HubScreen"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var hub: hubContext

    Component {
        id: screenComponent
        HubScreen {
            width: 1280
            height: 720
            hub: testCase.hub
        }
    }

    function init() {
        if (hub.appRunning) {
            hub.requestReturn()
            tryCompare(hub, "appRunning", false, 1000)
        }
    }

    function createScreen() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen !== null, "screen created")
        var grid = findChild(screen, "appGrid")
        tryCompare(grid, "count", 8)
        return screen
    }

    function tileAt(screen, index) {
        var grid = findChild(screen, "appGrid")
        grid.forceLayout()
        var item = grid.itemAtIndex(index)
        verify(item !== null, "tile " + index + " exists")
        return item
    }

    function test_eight_apps_render_eight_tiles_of_at_least_10_mm() {
        var screen = createScreen()
        for (var index = 0; index < 8; ++index) {
            var item = tileAt(screen, index)
            compare(findChild(item, "appName").text, "App " + (index + 1))
            compare(findChild(item, "appInitial").text, "A")
            verify(item.width >= Sizes.touchTarget, "tile " + index + " at least 10 mm wide")
            verify(item.height >= Sizes.touchTarget, "tile " + index + " at least 10 mm tall")
            verify(item.visible, "tile " + index + " visible")
        }
        compare(Sizes.touchTarget, Sizes.mm(10))
    }

    function test_a_tap_launches_that_app_and_disables_the_grid() {
        var screen = createScreen()
        var grid = findChild(screen, "appGrid")
        verify(grid.enabled, "grid enabled while idle")
        mouseClick(tileAt(screen, 3))
        compare(hub.appRunning, true)
        compare(hub.foregroundAppName, "App 4")
        compare(grid.enabled, false)
        compare(findChild(screen, "hubStatusText").text, "App 4 running")
        mouseClick(tileAt(screen, 5))
        compare(hub.foregroundAppName, "App 4")
    }

    function test_return_brings_the_grid_back_and_shows_the_exit() {
        var screen = createScreen()
        mouseClick(tileAt(screen, 0))
        compare(hub.appRunning, true)
        verify(hub.requestReturn(), "stop requested")
        tryCompare(hub, "appRunning", false, 1000)
        compare(findChild(screen, "appGrid").enabled, true)
        compare(findChild(screen, "hubStatusText").text, "App 1 stopped")
    }
}
