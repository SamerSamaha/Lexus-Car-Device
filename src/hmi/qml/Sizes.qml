pragma Singleton
import QtQuick

// Every size on screen is a millimetre figure converted through one constant (D-024).
// The 5-inch Touch Display 2 is 720 x 1280 pixels on 62.1 x 110.4 mm: 11.6 pixels per millimetre.
QtObject {
    readonly property real pixelsPerMillimetre: 11.6

    function mm(millimetres) {
        return Math.round(millimetres * pixelsPerMillimetre)
    }

    // Touch targets are at least 10 mm on each side.
    readonly property int touchTarget: mm(10)
    // A primary value's font: cap height about 4 mm if the font's cap height is 0.7 of the em
    // size (assumption A8, checked by eye on the panel).
    readonly property int primaryValuePixelSize: mm(5.7)
    readonly property int primaryValueMinimumPixelSize: mm(4)
    readonly property int labelPixelSize: mm(2.5)
    readonly property int unitPixelSize: mm(3)
    readonly property int statusStripHeight: mm(8)
    readonly property int gutter: mm(2)
    readonly property int tileRadius: mm(1)

    readonly property color background: "#101418"
    readonly property color tileBackground: "#1b2128"
    readonly property color tileBorder: "#2c3640"
    readonly property color liveValue: "#f4f6f8"
    readonly property color staleValue: "#6b7580"
    readonly property color neverReceivedValue: "#4a525b"
    readonly property color label: "#aab4bf"
    readonly property color staleBadge: "#c9a227"
    readonly property color connected: "#3fb950"
    readonly property color connecting: "#c9a227"
    readonly property color error: "#e5534b"
    readonly property color disconnected: "#6b7580"
}
