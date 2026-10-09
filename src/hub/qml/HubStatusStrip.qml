import QtQuick

// The hub's own state: the app in front, or the last exit, or the registry error.
Rectangle {
    id: strip

    required property var hub
    readonly property bool showsError: !hub.appRunning && hub.registryErrorText !== ""

    height: Sizes.statusStripHeight
    color: Sizes.tileBackground

    Text {
        id: titleText
        anchors.left: parent.left
        anchors.leftMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        text: "Lexus Head Unit"
        color: Sizes.liveValue
        font.pixelSize: Sizes.unitPixelSize
        font.bold: true
    }

    Text {
        objectName: "hubStatusText"
        anchors.left: titleText.right
        anchors.right: parent.right
        anchors.leftMargin: Sizes.gutter * 2
        anchors.rightMargin: Sizes.gutter
        anchors.verticalCenter: parent.verticalCenter
        horizontalAlignment: Text.AlignRight
        elide: Text.ElideRight
        text: strip.hub.appRunning ? strip.hub.foregroundAppName + " running"
            : strip.showsError ? strip.hub.registryErrorText
            : strip.hub.lastExitText
        color: strip.showsError ? Sizes.error : Sizes.label
        font.pixelSize: Sizes.labelPixelSize
    }
}
