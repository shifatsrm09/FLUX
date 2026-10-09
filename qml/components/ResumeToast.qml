import QtQuick
import QtQuick.Layouts
import "Theme.js" as Theme

// "Resumed from 32:10  |  Start over" pill shown briefly after resuming playback.
Rectangle {
    id: root

    property string text: ""

    signal startOver()

    implicitHeight: 46
    implicitWidth: toastRow.implicitWidth + 36
    radius: 23
    color: "#F2101015"
    border.width: 1
    border.color: "#2EFFFFFF"

    RowLayout {
        id: toastRow
        anchors.centerIn: parent
        spacing: 14

        Rectangle {
            implicitWidth: 8
            implicitHeight: 8
            radius: 4
            color: Theme.accent
            Layout.alignment: Qt.AlignVCenter
        }

        Text {
            text: root.text
            color: Theme.text
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        Rectangle {
            implicitWidth: 1
            implicitHeight: 18
            color: "#33FFFFFF"
        }

        Item {
            implicitWidth: startOverText.implicitWidth + 8
            implicitHeight: 30
            Layout.alignment: Qt.AlignVCenter

            Text {
                id: startOverText
                anchors.centerIn: parent
                text: "Start over"
                color: startMouse.containsMouse ? Theme.accentHover : Theme.text
                font.pixelSize: 13
                font.weight: Font.Bold

                Behavior on color { ColorAnimation { duration: 120 } }
            }

            MouseArea {
                id: startMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.startOver()
            }
        }
    }
}
