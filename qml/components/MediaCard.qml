import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

Item {
    id: root

    property string title: ""
    property string year: ""
    property string badge: ""
    property string description: ""
    property string url: ""
    property bool isSelected: false

    signal clicked()

    implicitWidth: 320
    implicitHeight: 96

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 12
        color: root.isSelected ? Theme.surfaceHi : (mouseArea.containsMouse ? Theme.surfaceHi : Theme.surface)
        border.width: root.isSelected ? 2 : 1
        border.color: root.isSelected ? Theme.accent : (mouseArea.containsMouse ? Theme.borderHi : Theme.border)

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Column {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 8

            Row {
                width: parent.width
                spacing: 8

                Text {
                    id: titleText
                    text: root.title
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                    width: parent.width - yearBadge.width - 8
                }

                Rectangle {
                    id: yearBadge
                    width: yearText.implicitWidth + 12
                    height: 20
                    radius: 5
                    color: Theme.surfaceTop
                    anchors.verticalCenter: titleText.verticalCenter

                    Text {
                        id: yearText
                        anchors.centerIn: parent
                        text: root.year
                        color: Theme.textDim
                        font.pixelSize: 11
                        font.weight: Font.Bold
                    }
                }
            }

            Rectangle {
                width: badgeText.implicitWidth + 14
                height: 22
                radius: 6
                color: Theme.accentSoft
                border.width: 1
                border.color: Theme.accentRing

                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: root.badge
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }

            Text {
                text: root.description
                color: Theme.textMute
                font.pixelSize: 11
                font.family: Theme.monoFamily
                elide: Text.ElideRight
                width: parent.width
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.clicked()
        }
    }
}
