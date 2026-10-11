// Verifies: REQ-024, REQ-026

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
    readonly property var recording: recordingContext
    readonly property var recordingDriver: recordingDriverContext

    Component {
        id: screenComponent
        HubScreen {
            width: 1280
            height: 720
            hub: testCase.hub
            ignitionShutdown: testCase.ignitionShutdown
            network: testCase.network
            recording: testCase.recording
        }
    }

    function createScreen() {
        driver.reset()
        recordingDriver.stop()
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

    function test_while_recording_the_strip_shows_rec_and_the_address_is_hidden() {
        var screen = createScreen()
        var indicator = findChild(screen, "recordingIndicator")
        var address = findChild(screen, "addressText")
        compare(indicator.visible, false)
        compare(address.visible, true)
        recordingDriver.start("screen-0001-2026-10-11T10-00-00.mkv")
        compare(recording.recording, true)
        compare(recording.fileName, "screen-0001-2026-10-11T10-00-00.mkv")
        tryCompare(indicator, "visible", true, 2000)
        compare(address.visible, false)
        verify(indicator.width > 0, "the indicator takes space on the strip")
        recordingDriver.stop()
        tryCompare(indicator, "visible", false, 2000)
        compare(address.visible, true)
    }
}
