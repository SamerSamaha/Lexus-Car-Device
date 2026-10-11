import QtQuick

// The map screen (DN-054): a strip with the position status and a Hub button (10 mm), then the
// OpenStreetMap view with the phone's position. The map itself is in MapView.qml, loaded by a
// Loader so that the screen still works, with a message, when the QtLocation module is missing.
// Before the first position, and whenever the position is near home while recording, the map
// is not drawn at all: no tile of the area around home reaches the screen.
Rectangle {
    id: screen

    required property var map
    property string cacheDirectory: ""
    signal hubRequested()

    readonly property bool mapShown: map.hasPosition && !map.masked

    color: Sizes.background

    Rectangle {
        id: strip
        objectName: "mapStrip"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Sizes.statusStripHeight
        color: Sizes.tileBackground

        Rectangle {
            id: liveDot
            objectName: "liveDot"
            anchors.left: parent.left
            anchors.leftMargin: Sizes.gutter
            anchors.verticalCenter: parent.verticalCenter
            width: Sizes.mm(3)
            height: width
            radius: width / 2
            color: screen.map.positionLive && !screen.map.masked ? Sizes.connected
                 : screen.map.hasPosition ? Sizes.connecting
                 : Sizes.disconnected
        }

        Text {
            objectName: "statusText"
            anchors.left: liveDot.right
            anchors.right: hubButton.left
            anchors.leftMargin: Sizes.gutter
            anchors.rightMargin: Sizes.gutter
            anchors.verticalCenter: parent.verticalCenter
            elide: Text.ElideRight
            text: screen.map.statusText
            color: Sizes.liveValue
            font.pixelSize: Sizes.detailPixelSize
            font.bold: true
        }

        Rectangle {
            id: hubButton
            objectName: "hubButton"
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.rightMargin: Sizes.gutter / 2
            width: Math.max(Sizes.touchTarget * 2, hubLabel.implicitWidth + Sizes.gutter * 2)
            radius: Sizes.tileRadius
            color: hubArea.pressed ? Sizes.tileBorder : Sizes.background
            border.color: Sizes.tileBorder

            Text {
                id: hubLabel
                anchors.centerIn: parent
                text: "Hub"
                color: Sizes.liveValue
                font.pixelSize: Sizes.unitPixelSize
                font.bold: true
            }

            MouseArea {
                id: hubArea
                anchors.fill: parent
                onClicked: screen.hubRequested()
            }
        }
    }

    Item {
        id: mapArea
        objectName: "mapArea"
        anchors.top: strip.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        Loader {
            id: mapLoader
            objectName: "mapLoader"
            anchors.fill: parent
            // Created with the first position, so no tile is fetched before there is one.
            active: screen.map.hasPosition
            visible: screen.mapShown
            source: Qt.resolvedUrl("MapView.qml")
        }

        Binding {
            target: mapLoader.item
            when: mapLoader.status === Loader.Ready
            property: "latitude"
            value: screen.map.latitude
        }
        Binding {
            target: mapLoader.item
            when: mapLoader.status === Loader.Ready
            property: "longitude"
            value: screen.map.longitude
        }
        Binding {
            target: mapLoader.item
            when: mapLoader.status === Loader.Ready
            property: "accuracyMetres"
            value: screen.map.accuracyMetres
        }
        Binding {
            target: mapLoader.item
            when: mapLoader.status === Loader.Ready
            property: "live"
            value: screen.map.positionLive
        }
        Binding {
            target: mapLoader.item
            when: mapLoader.status === Loader.Ready
            property: "cacheDirectory"
            value: screen.cacheDirectory
        }

        // Covers the whole map area whenever the map is not shown.
        Rectangle {
            id: cover
            objectName: "mapCover"
            anchors.fill: parent
            visible: !screen.mapShown || mapLoader.status !== Loader.Ready
            color: Sizes.background

            Text {
                objectName: "coverText"
                anchors.centerIn: parent
                width: parent.width - Sizes.gutter * 4
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: screen.map.masked ? "Position hidden near home"
                    : mapLoader.status === Loader.Error ? "The map module is not installed (qml6-module-qtlocation)"
                    : !screen.map.hasPosition ? "Open the phone link in Safari on the iPhone and tap Share position"
                    : ""
                color: Sizes.label
                font.pixelSize: Sizes.detailPixelSize
            }
        }
    }
}
