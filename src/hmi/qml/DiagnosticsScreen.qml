import QtQuick

// Diagnostics (DN-030, REQ-021): the stored trouble codes with their texts in a scrolling list,
// the vehicle identification, the power flags, the connection state on the strip, and a
// 10 mm Read codes button. Codes are only read; nothing here can clear them.
// power may be null (no power reader in this process); its line is then hidden.
Item {
    id: screen

    required property var vehicleData
    required property var diagnostics
    property var power: null
    signal homeRequested()

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

    Column {
        id: details
        anchors.top: strip.bottom
        anchors.left: parent.left
        anchors.right: readButton.left
        anchors.margins: Sizes.gutter
        spacing: Sizes.gutter

        Text {
            objectName: "summaryText"
            text: screen.diagnostics.summaryText
            color: Sizes.liveValue
            font.pixelSize: Sizes.unitPixelSize
            font.bold: true
        }

        Text {
            objectName: "identificationText"
            text: "Vehicle identification: " + screen.diagnostics.identificationText
            color: Sizes.label
            font.pixelSize: Sizes.labelPixelSize
        }

        Text {
            objectName: "powerText"
            visible: screen.power !== null
            text: screen.power === null ? "" : "Power: " + screen.power.flagsText
            color: screen.power !== null && screen.power.available && screen.power.flagsText !== "Power OK"
                   ? Sizes.error : Sizes.label
            font.pixelSize: Sizes.labelPixelSize
        }

        Text {
            objectName: "readTimeText"
            text: screen.diagnostics.readTimeText
            color: Sizes.label
            font.pixelSize: Sizes.labelPixelSize
        }
    }

    ListView {
        id: codeList
        objectName: "codeList"
        anchors.top: details.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        clip: true
        spacing: Sizes.gutter / 2
        model: screen.diagnostics.codeCount

        delegate: Rectangle {
            required property int index
            objectName: "codeRow" + index
            width: codeList.width
            height: Sizes.touchTarget
            radius: Sizes.tileRadius
            color: Sizes.tileBackground
            border.color: Sizes.tileBorder
            border.width: 1

            Text {
                id: codeText
                objectName: "codeText"
                anchors.left: parent.left
                anchors.leftMargin: Sizes.gutter
                anchors.verticalCenter: parent.verticalCenter
                width: Sizes.mm(20)
                text: screen.diagnostics.codes[parent.index]
                color: Sizes.liveValue
                font.pixelSize: Sizes.unitPixelSize
                font.bold: true
            }

            Text {
                objectName: "descriptionText"
                anchors.left: codeText.right
                anchors.right: parent.right
                anchors.rightMargin: Sizes.gutter
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: screen.diagnostics.descriptions[parent.index]
                color: Sizes.label
                font.pixelSize: Sizes.labelPixelSize
            }
        }
    }

    Rectangle {
        id: readButton
        objectName: "readCodesButton"
        anchors.top: strip.bottom
        anchors.right: parent.right
        anchors.margins: Sizes.gutter
        width: Sizes.mm(30)
        height: Sizes.touchTarget
        radius: Sizes.tileRadius
        color: readArea.pressed ? Sizes.tileBorder : Sizes.tileBackground
        border.color: Sizes.tileBorder
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "Read codes"
            color: Sizes.liveValue
            font.pixelSize: Sizes.unitPixelSize
            font.bold: true
        }

        MouseArea {
            id: readArea
            anchors.fill: parent
            onClicked: screen.diagnostics.refresh()
        }
    }
}
