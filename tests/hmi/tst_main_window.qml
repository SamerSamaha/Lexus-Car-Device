// Verifies: REQ-024

import QtQuick
import QtTest
import LexusHeadUnit

// The application window as the app creates it, with the context properties of
// lexus-head-unit --hub-button: the Hub button must reach the Home screen (LHU-047).
TestCase {
    id: testCase
    name: "MainWindow"
    visible: true
    when: windowShown

    Component {
        id: mainComponent
        Main {}
    }

    function test_the_hub_button_option_reaches_the_home_screen() {
        var window = createTemporaryObject(mainComponent, testCase)
        verify(window !== null)
        var home = findChild(window.contentItem, "homeScreen")
        verify(home !== null)
        compare(home.hubButtonShown, true)
        compare(findChild(home, "hubButton").visible, true)
    }
}
