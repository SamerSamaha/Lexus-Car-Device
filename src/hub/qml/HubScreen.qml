import QtQuick

// The status strip and the app grid: four columns, two rows per page, each tile a touch target
// of at least 10 mm (D-024). The grid is disabled while an app covers the hub.
Rectangle {
    id: screen

    required property var hub
    readonly property int columns: 4
    readonly property int rows: 2

    color: Sizes.background

    HubStatusStrip {
        id: strip
        objectName: "hubStatusStrip"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        hub: screen.hub
    }

    GridView {
        id: grid
        objectName: "appGrid"
        anchors.top: strip.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Sizes.gutter
        model: screen.hub.apps
        cellWidth: Math.floor(width / screen.columns)
        cellHeight: Math.floor(height / screen.rows)
        interactive: count > screen.columns * screen.rows
        clip: true
        enabled: !screen.hub.appRunning
        opacity: enabled ? 1.0 : 0.5

        delegate: AppTile {
            required property int index
            required property string name
            required property string icon

            width: grid.cellWidth - Sizes.gutter
            height: grid.cellHeight - Sizes.gutter
            appName: name
            iconSource: icon
            row: index
            onActivated: function(activatedRow) {
                screen.hub.launch(activatedRow)
            }
        }
    }
}
