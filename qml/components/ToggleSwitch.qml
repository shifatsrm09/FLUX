import QtQuick
import "Theme.js" as Theme

// Small on/off switch.  The bound `checked` value is never overwritten locally; the
// owner reacts to toggled(value) and updates its own state (so bindings stay intact).
Item {
    id: root

    property bool checked: false
    signal toggled(bool value)

    implicitWidth: 44
    implicitHeight: 24

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.checked ? Theme.accent : "#38FFFFFF"

        Behavior on color { ColorAnimation { duration: 180 } }

        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 3
            x: root.checked ? parent.width - width - 3 : 3
            color: "#FFFFFF"

            Behavior on x { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
