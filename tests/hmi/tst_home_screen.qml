// Verifies: REQ-012, REQ-006, REQ-007, REQ-023, REQ-024

import QtQuick
import QtTest
import LexusHeadUnit

TestCase {
    id: testCase
    name: "HomeScreen"
    width: 1280
    height: 720
    // TestCase is invisible by default; the screen under test must be shown for visibility
    // and mouse events to mean anything.
    visible: true
    when: windowShown

    readonly property int speedIndex: 0
    readonly property int rpmIndex: 1
    // The context property set by the test runner, under a name the screen does not shadow.
    readonly property var viewModel: vehicleData

    Component {
        id: homeComponent
        HomeScreen {
            width: 1280
            height: 720
            vehicleData: testCase.viewModel
        }
    }

    function createHome() {
        var home = createTemporaryObject(homeComponent, testCase)
        verify(home !== null, "home screen created")
        return home
    }

    function tileOf(home, objectName) {
        var tile = findChild(home, objectName)
        verify(tile !== null, objectName + " exists")
        return tile
    }

    function test_valid_values_show_text_unit_and_live_colour() {
        var home = createHome()
        vehicleData.simulateValid(speedIndex, 63.4)
        vehicleData.simulateValid(rpmIndex, 1726)
        var speed = tileOf(home, "speedTile")
        var rpm = tileOf(home, "rpmTile")
        compare(findChild(speed, "valueText").text, "63")
        compare(findChild(speed, "unitText").text, "km/h")
        compare(findChild(speed, "nameText").text, "Vehicle speed")
        compare(findChild(rpm, "valueText").text, "1726")
        compare(findChild(rpm, "unitText").text, "rpm")
        compare(findChild(speed, "valueText").color, Sizes.liveValue)
        compare(findChild(speed, "staleBadge").visible, false)
        compare(vehicleData.vehicleSpeed.statusText, "Valid")
    }

    function test_stale_values_keep_the_number_show_the_badge_and_never_use_the_live_colour() {
        var home = createHome()
        vehicleData.simulateValid(speedIndex, 63)
        vehicleData.simulateStale(speedIndex, 63)
        var speed = tileOf(home, "speedTile")
        compare(findChild(speed, "valueText").text, "63")
        compare(findChild(speed, "staleBadge").visible, true)
        verify(findChild(speed, "valueText").color !== Sizes.liveValue, "stale is not drawn live")
        compare(findChild(speed, "valueText").color, Sizes.staleValue)
        compare(vehicleData.vehicleSpeed.statusText, "Stale")
    }

    function test_never_received_shows_dashes_without_a_badge() {
        var home = createHome()
        vehicleData.simulateNeverReceived(speedIndex)
        vehicleData.simulateNeverReceived(rpmIndex)
        var speed = tileOf(home, "speedTile")
        compare(findChild(speed, "valueText").text, "--")
        compare(findChild(speed, "staleBadge").visible, false)
        verify(findChild(speed, "valueText").color !== Sizes.liveValue, "never received is not live")
        compare(findChild(tileOf(home, "rpmTile"), "valueText").text, "--")
    }

    function test_every_status_transition_updates_the_tile() {
        var home = createHome()
        var speed = tileOf(home, "speedTile")
        var valueText = findChild(speed, "valueText")
        vehicleData.simulateValid(speedIndex, 10)
        compare(valueText.text, "10")
        vehicleData.simulateStale(speedIndex, 10)
        compare(findChild(speed, "staleBadge").visible, true)
        vehicleData.simulateValid(speedIndex, 11)
        compare(valueText.text, "11")
        compare(findChild(speed, "staleBadge").visible, false)
        compare(valueText.color, Sizes.liveValue)
    }

    function test_connection_state_is_shown_within_500_ms() {
        var home = createHome()
        var strip = tileOf(home, "statusStrip")
        var stateText = findChild(strip, "stateText")
        vehicleData.simulateConnection("Connecting", "StartRequested")
        tryCompare(stateText, "text", "Connecting", 500)
        vehicleData.simulateConnection("Connected", "HandshakeSucceeded")
        tryCompare(stateText, "text", "Connected", 500)
        compare(findChild(strip, "causeText").text, "HandshakeSucceeded")
        compare(findChild(strip, "stateDot").color, Sizes.connected)
        vehicleData.simulateConnection("Error", "LinkLost")
        tryCompare(stateText, "text", "Error", 500)
        compare(findChild(strip, "stateDot").color, Sizes.error)
    }

    function test_each_link_detail_is_shown_large_within_500_ms() {
        var home = createHome()
        var strip = tileOf(home, "statusStrip")
        var detailText = findChild(strip, "detailText")
        verify(detailText !== null, "detail text exists")
        var cases = [["SearchingForAdapter", "Searching for adapter", Sizes.connecting],
                     ["AdapterWithoutVehicle", "Adapter found, no vehicle", Sizes.connecting],
                     ["Live", "Live", Sizes.connected],
                     ["LinkLostRetrying", "Link lost, retrying", Sizes.error]]
        for (var index = 0; index < cases.length; ++index) {
            vehicleData.simulateLinkDetail(cases[index][0])
            tryCompare(detailText, "text", cases[index][1], 500)
            compare(detailText.color, cases[index][2])
        }
        verify(detailText.font.pixelSize >= Sizes.mm(4.5), "detail at least 4.5 mm em size")
        verify(detailText.font.pixelSize > findChild(strip, "stateText").font.pixelSize,
               "detail larger than the state text")
        // The longest text fits on the strip next to the state and the cause.
        vehicleData.simulateLinkDetail("AdapterWithoutVehicle")
        var causeText = findChild(strip, "causeText")
        verify(causeText.x + causeText.width <= strip.width, "strip content fits")
    }

    function test_the_hub_button_appears_only_when_asked_and_requests_the_exit() {
        var home = createHome()
        compare(tileOf(home, "hubButton").visible, false)
        home.hubButtonShown = true
        var button = tileOf(home, "hubButton")
        compare(button.visible, true)
        verify(button.width >= Sizes.mm(10) && button.height >= Sizes.mm(10), "10 mm target")
        var requested = 0
        home.hubRequested.connect(function() { requested += 1 })
        mouseClick(button)
        compare(requested, 1)
        var last = tileOf(home, "diagnosticsButton").visible ? tileOf(home, "diagnosticsButton")
                                                             : tileOf(home, "tripButton")
        verify(button.x >= last.x + last.width, "the Hub button is the last one")
    }

    function test_sizes_follow_the_millimetre_rules() {
        var home = createHome()
        var speed = tileOf(home, "speedTile")
        verify(findChild(speed, "valueText").font.pixelSize >= Sizes.mm(4), "primary value at least 4 mm")
        var button = tileOf(home, "vehicleDataButton")
        verify(button.height >= Sizes.mm(10), "touch target at least 10 mm high")
        verify(button.width >= Sizes.mm(10), "touch target at least 10 mm wide")
        compare(Sizes.pixelsPerMillimetre, 11.6)
        compare(Sizes.mm(10), 116)
    }

    function test_vehicle_data_button_emits_the_request() {
        var home = createHome()
        var requested = 0
        home.vehicleDataRequested.connect(function() { requested += 1 })
        var button = tileOf(home, "vehicleDataButton")
        mouseClick(button)
        compare(requested, 1)
    }
}
