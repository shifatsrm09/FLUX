import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Round icon-only button with hover feedback and an optional tooltip.
//   IconButton { iconName: "play"; iconSize: 28; tip: "Play"; onClicked: ... }
Button {
    id: control

    property string iconName: "play"
    property real iconSize: 24
    property color iconColor: "#FFFFFF"
    property string tip: ""
    property bool tipAbove: true
    property bool filled: false
    property color hoverColor: "#26FFFFFF"

    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    padding: 0
    implicitWidth: iconSize + 24
    implicitHeight: iconSize + 24

    background: Rectangle {
        radius: width / 2
        color: control.down ? "#40FFFFFF"
                            : (control.hovered ? control.hoverColor
                                               : (control.filled ? "#26FFFFFF" : "transparent"))

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Item {
        FluxIcon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.iconColor
            opacity: control.enabled ? 1.0 : 0.4
            scale: control.hovered ? 1.08 : 1.0

            Behavior on scale { NumberAnimation { duration: 120 } }
        }
    }

    FluxToolTip {
        visible: control.hovered && control.tip.length > 0
        text: control.tip
        above: control.tipAbove
    }
}
