import QtQuick
import QtQuick.Window

// The hub window: 1280 x 720 on the panel rotated to landscape (D-045). hubContext is the
// HubViewModel set by the hub executable.
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
    }
}
