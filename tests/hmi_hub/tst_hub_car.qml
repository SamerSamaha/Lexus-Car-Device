// Verifies: REQ-024

import QtQuick
import QtTest
import LexusHub

TestCase {
    id: testCase
    name: "HubCar"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var hub: hubContext
    readonly property var ignitionShutdown: ignitionShutdownContext
    readonly property var network: networkContext
    readonly property var driver: carDriverContext

    Component {
        id: screenComponent
        HubScreen {
            width: 1280
            height: 720
            hub: testCase.hub
            ignitionShutdown: testCase.ignitionShutdown
            network: testCase.network
        }
    }

    function createScreen() {
        driver.reset()
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen !== null, "screen created")
        return screen
    }

    function test_the_countdown_banner_appears_with_a_10_mm_cancel_that_hides_it() {
        var screen = createScreen()
        var executionsBefore = driver.shutdownExecutions()
        var banner = findChild(screen, "countdownBanner")
        compare(banner.visible, false)
        driver.startCountdown()
        tryCompare(banner, "visible", true, 1000)
        compare(findChild(screen, "countdownText").text, "Vehicle off: shutting down in 60 s")
        verify(findChild(screen, "countdownText").font.pixelSize >= Sizes.mm(4.5),
               "countdown readable at a glance")
        var cancel = findChild(screen, "cancelShutdownButton")
        verify(cancel.width >= Sizes.mm(10) && cancel.height >= Sizes.mm(10), "10 mm target")
        mouseClick(cancel)
        tryCompare(banner, "visible", false, 1000)
        compare(driver.shutdownExecutions(), executionsBefore)
    }

    function test_the_address_line_shows_the_model_text_under_the_grid() {
        var screen = createScreen()
        var address = findChild(screen, "addressText")
        compare(address.visible, true)
        compare(address.text, "SSH lexus@172.20.10.2")
        var grid = findChild(screen, "appGrid")
        verify(grid.y + grid.height <= address.y, "the grid ends above the address line")
    }
}
