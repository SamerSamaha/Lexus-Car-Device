import QtQuick
import QtQuick.Window

// The hub window: 1280 x 720 on the panel rotated to landscape (D-045). hubContext is the
// HubViewModel set by the hub executable; connectionContext, powerContext and shutdownContext
// feed the status strip (DN-025); ignitionShutdownContext and networkContext the countdown
// banner and the address line (DN-043).
Window {
    id: window
    width: 1280
    height: 720
    visible: true
    color: Sizes.background
    title: "Lexus Hub"

    HubScreen {
        objectName: "hubScreen"
        anchors.fill: parent
        hub: hubContext
        connection: typeof connectionContext !== "undefined" ? connectionContext : null
        power: typeof powerContext !== "undefined" ? powerContext : null
        shutdown: typeof shutdownContext !== "undefined" ? shutdownContext : null
        ignitionShutdown: typeof ignitionShutdownContext !== "undefined" ? ignitionShutdownContext
                                                                         : null
        network: typeof networkContext !== "undefined" ? networkContext : null
    }
}
