import QtQuick
import QtQuick.Window

// The map app's window, 1280 x 720 on the rotated panel (D-045). mapContext is the MapViewModel
// set by lexus-map, mapCacheDirectoryContext the tile cache folder.
Window {
    id: window
    width: 1280
    height: 720
    visible: true
    color: Sizes.background
    title: "Lexus Map"

    MapScreen {
        objectName: "mapScreen"
        anchors.fill: parent
        map: mapContext
        cacheDirectory: typeof mapCacheDirectoryContext !== "undefined" ? mapCacheDirectoryContext : ""
        // A clean exit: the hub restarts an app only on failure, so it is in front again.
        onHubRequested: Qt.quit()
    }
}
