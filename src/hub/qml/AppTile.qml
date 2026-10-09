import QtQuick

// One app: its icon (or its first letter when it has none) and its name. The whole tile is the
// touch target.
Rectangle {
    id: tile

    property string appName: ""
    property string iconSource: ""
    property int row: -1
    signal activated(int row)

    objectName: "appTile_" + row
    radius: Sizes.tileRadius
    color: tapArea.pressed ? Sizes.tileBorder : Sizes.tileBackground
    border.color: Sizes.tileBorder
    border.width: 2

    Image {
        id: iconImage
        objectName: "appIcon"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: nameText.top
        anchors.bottomMargin: Sizes.gutter
        width: Sizes.mm(12)
        height: width
        fillMode: Image.PreserveAspectFit
        visible: tile.iconSource !== ""
        source: tile.iconSource === "" ? ""
              : tile.iconSource.startsWith("/") ? "file://" + tile.iconSource
              : tile.iconSource
    }

    Text {
        objectName: "appInitial"
        anchors.centerIn: iconImage
        visible: !iconImage.visible
        text: tile.appName.length > 0 ? tile.appName.charAt(0).toUpperCase() : "?"
        color: Sizes.liveValue
        font.pixelSize: Sizes.mm(9)
        font.bold: true
    }

    Text {
        id: nameText
        objectName: "appName"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Sizes.gutter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: tile.appName
        color: Sizes.label
        font.pixelSize: Sizes.unitPixelSize
    }

    MouseArea {
        id: tapArea
        objectName: "tapArea"
        anchors.fill: parent
        onClicked: tile.activated(tile.row)
    }
}
