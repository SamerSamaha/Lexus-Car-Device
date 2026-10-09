import QtQuick

// Vehicle data: the status strip with a Home button and all eight signals as a 4 x 2 grid.
Item {
    id: screen

    required property var vehicleData
    signal homeRequested()

    readonly property int columns: 4
    readonly property int rows: 2

    StatusStrip {
        id: strip
        objectName: "statusStrip"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        connection: screen.vehicleData.connection
        navigationText: "Home"
        onNavigationRequested: screen.homeRequested()
    }

    Grid {
        id: grid
        objectName: "grid"
        anchors.top: strip.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        columns: screen.columns
        rows: screen.rows
        columnSpacing: Sizes.gutter
        rowSpacing: Sizes.gutter

        readonly property int tileWidth: (width - (columns - 1) * columnSpacing) / columns
        readonly property int tileHeight: (height - (rows - 1) * rowSpacing) / rows

        Repeater {
            model: screen.vehicleData.tiles

            SignalTile {
                required property int index
                required property var modelData
                objectName: "tile" + index
                width: grid.tileWidth
                height: grid.tileHeight
                model: modelData
                valuePixelSize: Sizes.gridValuePixelSize
            }
        }
    }
}
