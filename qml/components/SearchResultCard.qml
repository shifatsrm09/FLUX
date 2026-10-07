import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "MediaFormatter.js" as Formatter

Rectangle {
    id: root

    property string title: ""
    property string parentPath: ""
    property bool isFolder: false
    property string formattedSize: ""
    property string extension: ""
    property string playUrl: ""
    property string libraryName: ""
    property bool isPlaying: false

    signal playRequested(string url, string title)

    // Formatted presentation values
    readonly property var parsed: Formatter.formatMedia(root.title, root.isFolder, root.formattedSize)

    implicitHeight: 58
    radius: 6
    color: {
        if (root.isPlaying) return "#151B28"
        if (rowMouse.containsMouse && !root.isFolder) return "#10131B"
        if (rowMouse.containsMouse && root.isFolder) return "#0D0F14"
        return "transparent"
    }

    Behavior on color { ColorAnimation { duration: 120 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 16

        // Title and Parsed Metadata
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            Text {
                Layout.fillWidth: true
                text: root.parsed.title
                color: root.isPlaying ? "#38BDF8" : "#F5F5F5"
                font.pixelSize: 15
                font.weight: Font.Medium
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.libraryName.length > 0
                      ? (root.parsed.subtitle.length > 0 ? root.libraryName + "  •  " + root.parsed.subtitle : root.libraryName)
                      : root.parsed.subtitle
                color: "#8F96A3"
                font.pixelSize: 12
                font.family: "Segoe UI, -apple-system, sans-serif"
                elide: Text.ElideRight
            }
        }

        // Hover Play Affordance or Folder Tag
        Item {
            implicitWidth: 32
            implicitHeight: 32

            // Play Icon (reveals on hover or while playing)
            Text {
                anchors.centerIn: parent
                visible: !root.isFolder
                text: root.isPlaying ? "❚❚" : "▶"
                font.pixelSize: root.isPlaying ? 11 : 12
                color: root.isPlaying ? "#38BDF8" : (rowMouse.containsMouse ? "#FFFFFF" : "transparent")
                opacity: (rowMouse.containsMouse || root.isPlaying) ? 1.0 : 0.0

                Behavior on opacity { NumberAnimation { duration: 120 } }
            }

            // Folder Label
            Text {
                anchors.centerIn: parent
                visible: root.isFolder
                text: "Folder"
                color: "#4B5363"
                font.pixelSize: 11
            }
        }
    }

    // Subtle bottom divider
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        height: 1
        color: "#141721"
        visible: !rowMouse.containsMouse && !root.isPlaying
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.isFolder ? Qt.ArrowCursor : Qt.PointingHandCursor
        onClicked: {
            if (!root.isFolder && root.playUrl.length > 0) {
                root.playRequested(root.playUrl, root.parsed.title)
            }
        }
    }

    ToolTip.visible: rowMouse.containsMouse && root.title.length > 30
    ToolTip.delay: 600
    ToolTip.text: root.title + (root.parentPath.length > 0 ? ("\n" + root.parentPath) : "")
}
