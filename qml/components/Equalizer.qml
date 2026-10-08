import QtQuick
import "Theme.js" as Theme

// Tiny animated "now playing" equalizer bars.
Item {
    id: root

    property bool running: true
    property color color: Theme.accent
    property int barCount: 3

    implicitWidth: barCount * 3 + (barCount - 1) * 2
    implicitHeight: 14
    width: implicitWidth
    height: implicitHeight

    Repeater {
        model: root.barCount

        delegate: Rectangle {
            required property int index

            x: index * 5
            y: root.height - height
            width: 3
            height: 4
            radius: 1.5
            color: root.color

            SequentialAnimation on height {
                running: root.running
                loops: Animation.Infinite

                NumberAnimation { to: 14; duration: 380 + index * 90; easing.type: Easing.InOutSine }
                NumberAnimation { to: 4;  duration: 420 + index * 70; easing.type: Easing.InOutSine }
            }
        }
    }
}
