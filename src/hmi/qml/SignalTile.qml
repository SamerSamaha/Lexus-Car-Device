import QtQuick

// One signal: name, value, unit and status. Bindings only; the model decides the text.
Rectangle {
    id: tile

    required property var model
    property int valuePixelSize: Sizes.primaryValuePixelSize

    // The live colour is used only for a Valid value (REQ-006, REQ-012).
    readonly property color valueColor: model.isValid ? Sizes.liveValue
                                      : model.isStale ? Sizes.staleValue
                                      : Sizes.neverReceivedValue

    color: Sizes.tileBackground
    border.color: Sizes.tileBorder
    border.width: 1
    radius: Sizes.tileRadius

    Text {
        id: nameText
        objectName: "nameText"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Sizes.gutter
        text: tile.model.name
        color: Sizes.label
        font.pixelSize: Sizes.labelPixelSize
    }

    Text {
        id: valueText
        objectName: "valueText"
        anchors.centerIn: parent
        width: parent.width - 2 * Sizes.gutter
        horizontalAlignment: Text.AlignHCenter
        text: tile.model.valueText
        color: tile.valueColor
        font.pixelSize: tile.valuePixelSize
        minimumPixelSize: Sizes.primaryValueMinimumPixelSize
        fontSizeMode: Text.HorizontalFit
        font.bold: true
    }

    Text {
        id: unitText
        objectName: "unitText"
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        text: tile.model.unitText
        color: Sizes.label
        font.pixelSize: Sizes.unitPixelSize
    }

    Rectangle {
        id: staleBadge
        objectName: "staleBadge"
        visible: tile.model.isStale
        anchors.right: unitText.left
        anchors.rightMargin: Sizes.gutter
        anchors.verticalCenter: unitText.verticalCenter
        width: staleBadgeText.implicitWidth + Sizes.gutter
        height: staleBadgeText.implicitHeight + Sizes.gutter / 2
        radius: Sizes.tileRadius
        color: Sizes.staleBadge

        Text {
            id: staleBadgeText
            anchors.centerIn: parent
            text: "STALE"
            color: Sizes.background
            font.pixelSize: Sizes.labelPixelSize
            font.bold: true
        }
    }
}
