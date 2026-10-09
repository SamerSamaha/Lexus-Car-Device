// Verifies: REQ-021

import QtQuick
import QtTest
import LexusHeadUnit

TestCase {
    id: testCase
    name: "DiagnosticsScreen"
    width: 1280
    height: 720
    visible: true
    when: windowShown

    readonly property var viewModel: vehicleData
    readonly property var diagnosticsModel: diagnosticsContext
    readonly property var driver: diagnosticsDriver
    readonly property var powerModel: powerContext
    readonly property var reader: powerReaderContext

    Component {
        id: screensComponent
        Screens {
            width: 1280
            height: 720
            vehicleData: testCase.viewModel
            diagnostics: testCase.diagnosticsModel
            power: testCase.powerModel
        }
    }

    function createScreens() {
        var screens = createTemporaryObject(screensComponent, testCase)
        verify(screens !== null, "screens created")
        return screens
    }

    function openDiagnostics(screens) {
        var home = findChild(screens, "homeScreen")
        var button = findChild(home, "diagnosticsButton")
        verify(button !== null && button.visible, "diagnostics button on Home")
        verify(button.height >= Sizes.touchTarget, "at least 10 mm tall")
        mouseClick(button)
        compare(screens.diagnosticsShown, true)
        var screen = findChild(screens, "diagnosticsScreen")
        verify(screen !== null, "diagnostics screen exists")
        return screen
    }

    function test_opening_the_screen_asks_for_a_read_and_shows_reading() {
        var screens = createScreens()
        var before = driver.requests
        var screen = openDiagnostics(screens)
        compare(driver.requests, before + 1)
        compare(findChild(screen, "summaryText").text, "Reading...")
    }

    function test_codes_with_their_texts_identification_and_power() {
        var screens = createScreens()
        var screen = openDiagnostics(screens)
        reader.setOutput("throttled=0x1\n", true)
        driver.deliver(["P0133", "P0420", "U0100"],
                       ["Oxygen sensor slow response, bank 1 sensor 1",
                        "Catalyst efficiency below threshold, bank 1",
                        "Lost communication with the engine control module"],
                       "DEMO-NOT-A-VIN")
        compare(findChild(screen, "summaryText").text, "3 stored trouble codes")
        compare(findChild(screen, "identificationText").text, "Vehicle identification: DEMO-NOT-A-VIN")
        var list = findChild(screen, "codeList")
        compare(list.count, 3)
        list.forceLayout()
        var row = list.itemAtIndex(2)
        verify(row !== null, "third row exists")
        compare(findChild(row, "codeText").text, "U0100")
        compare(findChild(row, "descriptionText").text, "Lost communication with the engine control module")
        tryCompare(findChild(screen, "powerText"), "text", "Power: Under-voltage", 5000)
        reader.setOutput("throttled=0x0\n", true)
        tryCompare(findChild(screen, "powerText"), "text", "Power: Power OK", 5000)
    }

    function test_no_codes_and_the_read_button() {
        var screens = createScreens()
        var screen = openDiagnostics(screens)
        driver.deliver([], [], "")
        compare(findChild(screen, "summaryText").text, "No stored trouble codes")
        compare(findChild(screen, "identificationText").text, "Vehicle identification: Not read")
        compare(findChild(screen, "codeList").count, 0)
        var button = findChild(screen, "readCodesButton")
        verify(button.width >= Sizes.touchTarget && button.height >= Sizes.touchTarget, "10 mm")
        var before = driver.requests
        mouseClick(button)
        compare(driver.requests, before + 1)
    }

    function test_home_from_the_strip() {
        var screens = createScreens()
        var screen = openDiagnostics(screens)
        mouseClick(findChild(findChild(screen, "statusStrip"), "navigationButton"))
        compare(screens.diagnosticsShown, false)
        compare(findChild(screens, "homeScreen").visible, true)
    }
}
