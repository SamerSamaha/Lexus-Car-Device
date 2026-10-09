// Verifies: REQ-020

import QtQuick
import QtTest
import LexusHub

TestCase {
    id: testCase
    name: "HubPower"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var hub: hubContext
    readonly property var power: powerContext
    readonly property var shutdown: shutdownContext
    readonly property var reader: powerReaderContext
    readonly property var connection: connectionContext
    readonly property var flagCases: [["throttled=0x1\n", "Under-voltage"],
                                      ["throttled=0x2\n", "Frequency capped"],
                                      ["throttled=0x4\n", "Throttled"],
                                      ["throttled=0x8\n", "Soft temperature limit"]]

    Component {
        id: screenComponent
        HubScreen {
            width: 1280
            height: 720
            hub: testCase.hub
            power: testCase.power
            shutdown: testCase.shutdown
            connection: testCase.connection
        }
    }

    function createScreen() {
        var screen = createTemporaryObject(screenComponent, testCase)
        verify(screen !== null, "screen created")
        return screen
    }

    function test_each_current_flag_is_shown_within_5_s_and_cleared() {
        var screen = createScreen()
        var powerText = findChild(screen, "powerText")
        verify(powerText !== null, "power text exists")
        for (var index = 0; index < flagCases.length; ++index) {
            reader.setOutput(flagCases[index][0], true)
            tryCompare(powerText, "text", flagCases[index][1], 5000)
            compare(powerText.color, Sizes.error)
            reader.setOutput("throttled=0x0\n", true)
            tryCompare(powerText, "text", "Power OK", 5000)
        }
    }

    function test_unavailable_power_status_is_said_not_hidden() {
        var screen = createScreen()
        reader.setOutput("", false)
        tryCompare(findChild(screen, "powerText"), "text", "Power status unavailable", 5000)
        reader.setOutput("throttled=0x0\n", true)
        tryCompare(findChild(screen, "powerText"), "text", "Power OK", 5000)
    }

    function test_shutdown_button_is_a_touch_target_and_needs_two_taps() {
        var screen = createScreen()
        var button = findChild(screen, "shutdownButton")
        verify(button !== null && button.visible, "shutdown button shown")
        verify(button.width >= Sizes.touchTarget, "at least 10 mm wide")
        verify(button.height >= Sizes.touchTarget, "at least 10 mm tall")
        var before = shutdown.executions
        mouseClick(button)
        compare(shutdown.armed, true)
        compare(shutdown.executions, before)
        compare(findChild(screen, "hubStatusText").text, "Tap again to shut down")
        mouseClick(button)
        compare(shutdown.armed, false)
        compare(shutdown.executions, before + 1)
    }

    function test_connection_state_is_on_the_strip() {
        var screen = createScreen()
        var state = findChild(screen, "connectionText")
        verify(state !== null, "connection text exists")
        compare(state.text, connection.stateText)
    }
}
