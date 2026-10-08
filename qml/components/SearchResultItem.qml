import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Rectangle {
    id: root

    property string title: ""
    property string parentPath: ""
    property bool isFolder: false
    property string formattedSize: ""
    property string extension: ""
    property string playUrl: ""
    property bool isPlaying: false

    signal playRequested(string url)

    implicitWidth: 380
    implicitHeight: 68
    radius: 12
    color: (mouseArea.containsMouse || root.isPlaying) && !root.isFolder ? Theme.surfaceHi : Theme.surface
    border.width: root.isPlaying ? 2 : 1
    border.color: root.isPlaying ? Theme.accent : (mouseArea.containsMouse && !root.isFolder ? Theme.borderHi : Theme.border)

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 12

        // Type icon
        Rectangle {
            implicitWidth: 40
            implicitHeight: 40
            radius: 10
            color: root.isFolder ? Theme.surfaceTop : (root.isPlaying ? Theme.accent : Theme.accentSoft)

            FluxIcon {
                anchors.centerIn: parent
                name: root.isFolder ? "folder" : "play"
                size: 20
                color: root.isFolder ? Theme.textDim : "#FFFFFF"
            }
        }

        // Title & parent path
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: root.title
                    color: root.isPlaying ? Theme.accentHover : (root.isFolder ? Theme.textDim : Theme.text)
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }

                Rectangle {
                    visible: root.extension.length > 0
                    implicitWidth: extText.implicitWidth + 12
                    implicitHeight: 18
                    radius: 4
                    color: Theme.surfaceTop

                    Text {
                        id: extText
                        anchors.centerIn: parent
                        text: root.extension
                        color: Theme.textDim
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        font.family: Theme.monoFamily
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: root.parentPath
                    color: Theme.textMute
                    font.pixelSize: 10
                    font.family: Theme.monoFamily
                    elide: Text.ElideLeft
                }

                Text {
                    text: root.formattedSize
                    color: root.isFolder ? Theme.textMute : Theme.success
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    font.family: Theme.monoFamily
                }
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.isFolder ? Qt.ArrowCursor : Qt.PointingHandCursor
        onClicked: {
            if (!root.isFolder && root.playUrl.length > 0) {
                root.playRequested(root.playUrl)
            }
        }
    }

    FluxToolTip {
        visible: mouseArea.containsMouse && root.isFolder
        delay: 400
        text: "Directory: " + root.title + " (cannot be played directly)"
    }
}
