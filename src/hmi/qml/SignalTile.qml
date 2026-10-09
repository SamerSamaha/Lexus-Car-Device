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
        text: tile.model.valueText
        color: tile.valueColor
        font.pixelSize: tile.valuePixelSize
        font.bold: true
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        spacing: Sizes.gutter

        Rectangle {
            id: staleBadge
            objectName: "staleBadge"
            visible: tile.model.isStale
            width: staleBadgeText.implicitWidth + Sizes.gutter
            height: staleBadgeText.implicitHeight + Sizes.gutter / 2
            radius: Sizes.tileRadius
            color: Sizes.staleBadge
            anchors.verticalCenter: parent.verticalCenter

            Text {
                id: staleBadgeText
                anchors.centerIn: parent
                text: "STALE"
                color: Sizes.background
                font.pixelSize: Sizes.labelPixelSize
                font.bold: true
            }
        }

        Text {
            id: unitText
            objectName: "unitText"
            text: tile.model.unitText
            color: Sizes.label
            font.pixelSize: Sizes.unitPixelSize
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
