import QtQuick

// Soft radial glow built from stacked translucent discs (no shader effects
// needed, so it works with the plain Qt Quick runtime).
Item {
    id: root

    property color color: "#E50914"
    property real strength: 0.2
    property int layers: 18

    width: 600
    height: 600

    Repeater {
        model: root.layers

        delegate: Rectangle {
            required property int index

            readonly property real t: index / (root.layers - 1)

            width: root.width * (1.0 - t * 0.94)
            height: width
            radius: width / 2
            anchors.centerIn: parent
            color: root.color
            opacity: root.strength / root.layers * 1.8
        }
    }
}
