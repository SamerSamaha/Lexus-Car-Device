import QtQuick

// The status strip and the app grid: four columns, two rows per page, each tile a touch target
// of at least 10 mm (D-024). The grid is disabled while an app covers the hub. In car mode
// (DN-043) the address line sits under the grid and the shutdown countdown covers it.
Rectangle {
    id: screen

    required property var hub
    property var connection: null
    property var power: null
    property var shutdown: null
    property var ignitionShutdown: null
    property var network: null
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
        connection: screen.connection
        power: screen.power
        shutdown: screen.shutdown
    }

    GridView {
        id: grid
        objectName: "appGrid"
        anchors.top: strip.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: addressText.visible ? addressText.top : parent.bottom
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

    Text {
        id: addressText
        objectName: "addressText"
        visible: screen.network !== null
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Sizes.gutter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: screen.network === null ? "" : screen.network.addressText
        color: Sizes.label
        font.pixelSize: Sizes.labelPixelSize
    }

    // The countdown after the vehicle went quiet (DN-043): large, over the grid, with Cancel.
    Rectangle {
        id: countdownBanner
        objectName: "countdownBanner"
        visible: screen.ignitionShutdown !== null && screen.ignitionShutdown.countdownActive
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: grid.verticalCenter
        anchors.margins: Sizes.gutter
        height: Sizes.touchTarget * 2
        radius: Sizes.tileRadius
        color: Sizes.tileBackground
        border.color: Sizes.connecting
        border.width: 2

        Text {
            objectName: "countdownText"
            anchors.left: parent.left
            anchors.right: cancelButton.left
            anchors.leftMargin: Sizes.gutter * 2
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            text: screen.ignitionShutdown === null ? "" : screen.ignitionShutdown.countdownText
            color: Sizes.liveValue
            font.pixelSize: Sizes.detailPixelSize
            font.bold: true
        }

        Rectangle {
            id: cancelButton
            objectName: "cancelShutdownButton"
            anchors.right: parent.right
            anchors.rightMargin: Sizes.gutter
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(Sizes.touchTarget * 2, cancelLabel.implicitWidth + Sizes.gutter * 2)
            height: Sizes.touchTarget
            radius: Sizes.tileRadius
            color: cancelArea.pressed ? Sizes.tileBorder : Sizes.background
            border.color: Sizes.tileBorder

            Text {
                id: cancelLabel
                anchors.centerIn: parent
                text: "Cancel"
                color: Sizes.liveValue
                font.pixelSize: Sizes.unitPixelSize
                font.bold: true
            }

            MouseArea {
                id: cancelArea
                anchors.fill: parent
                onClicked: screen.ignitionShutdown.cancel()
            }
        }
    }
}
