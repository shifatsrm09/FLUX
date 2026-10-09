import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Folder navigation bar:  [<]  Results  >  Share  >  Folder  >  Subfolder
// The last crumb is the current location; earlier crumbs are clickable.
Rectangle {
    id: root

    property var crumbs: []
    property string rootLabel: "Results"

    signal backClicked()
    signal rootClicked()
    signal crumbClicked(int index)

    implicitHeight: 50
    radius: 25
    color: Theme.surface
    border.width: 1
    border.color: Theme.border

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 8
        anchors.rightMargin: 20
        spacing: 6

        IconButton {
            iconName: "back"
            iconSize: 18
            implicitWidth: 36
            implicitHeight: 36
            tip: "Up one level"
            tipAbove: false
            Layout.alignment: Qt.AlignVCenter

            onClicked: root.backClicked()
        }

        // First crumb: back to the search results (or home)
        Item {
            implicitWidth: rootText.implicitWidth + 12
            implicitHeight: 32
            Layout.alignment: Qt.AlignVCenter

            Text {
                id: rootText
                anchors.centerIn: parent
                text: root.rootLabel
                color: rootMouse.containsMouse ? Theme.text : Theme.textDim
                font.pixelSize: 13
                font.weight: Font.Medium

                Behavior on color { ColorAnimation { duration: 120 } }
            }

            MouseArea {
                id: rootMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.rootClicked()
            }
        }

        ListView {
            id: list

            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: ListView.Horizontal
            clip: true
            interactive: list.contentWidth > list.width
            boundsBehavior: Flickable.StopAtBounds
            model: root.crumbs

            // Keep the current (right-most) location in view
            onContentWidthChanged: list.positionViewAtEnd()

            delegate: Item {
                id: crumb

                required property var modelData
                required property int index

                readonly property bool isLast: crumb.index === list.count - 1

                height: list.height
                width: sep.implicitWidth + nameText.implicitWidth + 24

                Text {
                    id: sep
                    anchors.left: parent.left
                    anchors.leftMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    text: "\u203A"
                    color: Theme.textMute
                    font.pixelSize: 18
                }

                Text {
                    id: nameText
                    anchors.left: sep.right
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: crumb.modelData.name
                    color: (crumb.isLast || crumbMouse.containsMouse) ? Theme.text : Theme.textDim
                    font.pixelSize: 13
                    font.weight: crumb.isLast ? Font.Bold : Font.Medium

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                MouseArea {
                    id: crumbMouse
                    anchors.fill: nameText
                    anchors.margins: -6
                    enabled: !crumb.isLast
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.crumbClicked(crumb.index)
                }
            }
        }
    }
}
