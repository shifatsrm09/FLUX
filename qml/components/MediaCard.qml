import QtQuick
import QtQuick.Controls

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
        radius: 8
        color: root.isSelected ? "#1e293b" : (mouseArea.containsMouse ? "#182234" : "#111827")
        border.color: root.isSelected ? "#3b82f6" : (mouseArea.containsMouse ? "#475569" : "#1f2937")
        border.width: root.isSelected ? 2 : 1

        Behavior on color { ColorAnimation { duration: 150 } }
        Behavior on border.color { ColorAnimation { duration: 150 } }

        Column {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 6

            Row {
                width: parent.width
                spacing: 8

                Text {
                    id: titleText
                    text: root.title
                    color: "#f9fafb"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                    width: parent.width - yearBadge.width - 8
                }

                Rectangle {
                    id: yearBadge
                    width: yearText.implicitWidth + 8
                    height: 18
                    radius: 4
                    color: "#1f2937"
                    anchors.verticalCenter: titleText.verticalCenter

                    Text {
                        id: yearText
                        anchors.centerIn: parent
                        text: root.year
                        color: "#9ca3af"
                        font.pixelSize: 11
                        font.bold: true
                    }
                }
            }

            Rectangle {
                width: badgeText.implicitWidth + 10
                height: 20
                radius: 4
                color: "#1e3a8a"
                border.color: "#3b82f6"
                border.width: 1

                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: root.badge
                    color: "#93c5fd"
                    font.pixelSize: 11
                    font.family: "Segoe UI, sans-serif"
                    font.bold: true
                }
            }

            Text {
                text: root.description
                color: "#6b7280"
                font.pixelSize: 11
                font.family: "Consolas, monospace"
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
