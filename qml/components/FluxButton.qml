import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Pill button.  variant: "primary" (red) | "secondary" (frosted) | "ghost"
//   FluxButton { text: "Search"; iconName: "search"; onClicked: ... }
Button {
    id: control

    property string iconName: ""
    property string variant: "primary"
    property real iconSize: 20

    hoverEnabled: true
    focusPolicy: Qt.NoFocus
    implicitHeight: 44
    implicitWidth: Math.max(implicitHeight, buttonRow.implicitWidth + leftPadding + rightPadding)
    leftPadding: control.text.length > 0 ? 22 : 12
    rightPadding: control.text.length > 0 ? 22 : 12
    opacity: control.enabled ? 1.0 : 0.45
    scale: control.down ? 0.97 : 1.0

    Behavior on scale { NumberAnimation { duration: 90; easing.type: Easing.OutQuad } }

    background: Rectangle {
        radius: height / 2
        color: {
            if (control.variant === "primary") {
                return control.down ? Theme.accentPressed
                                    : (control.hovered ? Theme.accentHover : Theme.accent)
            }
            if (control.variant === "secondary") {
                return control.down ? "#40FFFFFF" : (control.hovered ? "#33FFFFFF" : "#24FFFFFF")
            }
            return control.hovered ? "#1FFFFFFF" : "transparent"
        }

        Behavior on color { ColorAnimation { duration: 120 } }
    }

    contentItem: Item {
        implicitWidth: buttonRow.implicitWidth
        implicitHeight: buttonRow.implicitHeight

        RowLayout {
            id: buttonRow
            anchors.centerIn: parent
            spacing: 8

            FluxIcon {
                visible: control.iconName.length > 0
                name: control.iconName
                size: control.iconSize
                color: "#FFFFFF"
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                visible: control.text.length > 0
                text: control.text
                color: "#FFFFFF"
                font.pixelSize: 14
                font.weight: Font.DemiBold
                Layout.alignment: Qt.AlignVCenter
            }
        }
    }
}
