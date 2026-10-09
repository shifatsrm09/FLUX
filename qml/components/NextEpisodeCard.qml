import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// "Up next" card shown near the end of an episode.
//   countdownSeconds < 0  -> no auto-play countdown (just a Play now button)
Rectangle {
    id: root

    property string title: ""
    property string subtitle: ""
    property int countdownSeconds: -1
    property real countdownFraction: 0

    signal playNow()
    signal dismissed()

    width: 360
    implicitHeight: content.implicitHeight + 36
    radius: 16
    color: "#F2101015"
    border.width: 1
    border.color: "#2EFFFFFF"

    ColumnLayout {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: 18
        anchors.rightMargin: 18
        spacing: 6

        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "UP NEXT"
                color: Theme.accentHover
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 2
                Layout.fillWidth: true
            }

            IconButton {
                iconName: "close"
                iconSize: 12
                iconColor: Theme.textDim
                implicitWidth: 26
                implicitHeight: 26
                tip: "Dismiss"
                tipAbove: false
                Layout.alignment: Qt.AlignVCenter

                onClicked: root.dismissed()
            }
        }

        Text {
            Layout.fillWidth: true
            text: root.title
            color: "#FFFFFF"
            font.pixelSize: 16
            font.weight: Font.Bold
            wrapMode: Text.Wrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Text {
            Layout.fillWidth: true
            visible: root.subtitle.length > 0
            text: root.subtitle
            color: Theme.textDim
            font.pixelSize: 12
            elide: Text.ElideRight
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 8
            spacing: 12

            FluxButton {
                text: "Play now"
                implicitHeight: 38
                onClicked: root.playNow()
            }

            Text {
                visible: root.countdownSeconds >= 0
                text: "Starting in " + root.countdownSeconds + "s"
                color: Theme.textDim
                font.pixelSize: 12
                Layout.alignment: Qt.AlignVCenter
            }
        }

        // Countdown progress
        Rectangle {
            visible: root.countdownSeconds >= 0
            Layout.fillWidth: true
            Layout.topMargin: 4
            implicitHeight: 3
            radius: 1.5
            color: "#33FFFFFF"

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.countdownFraction))
                height: parent.height
                radius: 1.5
                color: Theme.accent

                Behavior on width { NumberAnimation { duration: 950 } }
            }
        }
    }
}
