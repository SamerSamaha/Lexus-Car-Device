import QtQuick
import QtLocation
import QtPositioning

// The OpenStreetMap view (DN-054): standard tiles from tile.openstreetmap.org through the Qt
// osm plugin, a dot at the phone's position (grey when it is not live) and the accuracy circle.
// The map follows the dot; there is no route and no turn-by-turn (D-042). The tile usage policy
// asks for an application user agent and the attribution, both below.
Item {
    id: view

    property real latitude: 0
    property real longitude: 0
    property real accuracyMetres: 0
    property bool live: false
    property string cacheDirectory: ""

    readonly property var position: QtPositioning.coordinate(latitude, longitude)

    Plugin {
        id: tiles
        name: "osm"
        PluginParameter {
            name: "osm.useragent"
            value: "LexusHeadUnit/1.0 (portfolio project; github.com/SamerSamaha/Lexus-Car-Device)"
        }
        PluginParameter { name: "osm.mapping.custom.host"; value: "https://tile.openstreetmap.org/" }
        PluginParameter { name: "osm.mapping.providersrepository.disabled"; value: true }
        PluginParameter { name: "osm.mapping.cache.directory"; value: view.cacheDirectory }
    }

    Map {
        id: map
        objectName: "map"
        anchors.fill: parent
        plugin: tiles
        center: view.position
        zoomLevel: 16
        copyrightsVisible: false

        Component.onCompleted: {
            for (var index = 0; index < supportedMapTypes.length; ++index) {
                if (supportedMapTypes[index].style === MapType.CustomMap) {
                    activeMapType = supportedMapTypes[index]
                }
            }
        }

        MapCircle {
            center: view.position
            radius: view.accuracyMetres
            color: "#3358a6ff"
            border.width: 0
        }

        MapQuickItem {
            coordinate: view.position
            anchorPoint.x: dot.width / 2
            anchorPoint.y: dot.height / 2
            sourceItem: Rectangle {
                id: dot
                width: Sizes.mm(5)
                height: width
                radius: width / 2
                color: view.live ? "#58a6ff" : Sizes.staleValue
                border.color: "white"
                border.width: 3
            }
        }
    }

    Text {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 4
        text: "© OpenStreetMap contributors"
        color: "#202020"
        style: Text.Outline
        styleColor: "white"
        font.pixelSize: Sizes.labelPixelSize
    }
}
