// Verifies: REQ-024, REQ-026

import QtQuick
import QtTest
import LexusHub

// The hub window as lexus-hub creates it, with every context property the executable sets: the
// recording status and the address must reach the screen through Main.qml (lesson of LHU-047).
TestCase {
    id: testCase
    name: "HubMainWindow"
    visible: true
    when: windowShown

    Component {
        id: mainComponent
        Main {}
    }

    function test_the_recording_status_reaches_the_strip_and_hides_the_address() {
        recordingDriverContext.stop()
        var window = createTemporaryObject(mainComponent, testCase)
        verify(window !== null)
        var screen = findChild(window.contentItem, "hubScreen")
        verify(screen !== null)
        verify(screen.recording !== null, "recordingContext reached HubScreen")
        var address = findChild(screen, "addressText")
        var indicator = findChild(screen, "recordingIndicator")
        compare(address.visible, true)
        compare(indicator.visible, false)
        recordingDriverContext.start("screen-0002-2026-10-11T11-00-00.mkv")
        tryCompare(indicator, "visible", true, 2000)
        compare(address.visible, false)
        recordingDriverContext.stop()
        tryCompare(address, "visible", true, 2000)
    }
}
