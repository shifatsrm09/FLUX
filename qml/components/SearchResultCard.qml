import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    implicitHeight: 62
    radius: 8
    color: {
        if (root.isPlaying) return "#13233f"
        if (cardMouse.containsMouse && !root.isFolder) return "#111b2e"
        if (cardMouse.containsMouse && root.isFolder) return "#0d1424"
        return "#090f1d"
    }
    border.color: {
        if (root.isPlaying) return "#3b82f6"
        if (cardMouse.containsMouse && !root.isFolder) return "#2563eb"
        return "#162035"
    }
    border.width: root.isPlaying ? 2 : 1

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 14

        // Type Badge
        Rectangle {
            implicitWidth: 38
            implicitHeight: 38
            radius: 6
            color: root.isFolder ? "#1e293b" : (root.isPlaying ? "#1d4ed8" : "#14223d")

            Text {
                anchors.centerIn: parent
                text: root.isFolder ? "DIR" : (root.extension.length > 0 ? root.extension : "FILE")
                color: root.isFolder ? "#94a3b8" : (root.isPlaying ? "#ffffff" : "#60a5fa")
                font.pixelSize: 10
                font.bold: true
                font.family: "Consolas, monospace"
            }
        }

        // Title and Metadata Info
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: root.title
                color: root.isPlaying ? "#93c5fd" : (root.isFolder ? "#cbd5e1" : "#f8fafc")
                font.pixelSize: 13
                font.bold: true
                elide: Text.ElideRight
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: root.libraryName.length > 0 ? root.libraryName : "Media Library"
                    color: "#64748b"
                    font.pixelSize: 11
                }

                Text {
                    text: "•"
                    color: "#334155"
                    font.pixelSize: 10
                }

                Text {
                    text: root.isFolder ? "Directory" : root.formattedSize
                    color: root.isFolder ? "#64748b" : "#38bdf8"
                    font.pixelSize: 11
                    font.family: "Consolas, monospace"
                }

                Text {
                    visible: root.parentPath.length > 0 && root.parentPath !== "/"
                    text: "•"
                    color: "#334155"
                    font.pixelSize: 10
                }

                Text {
                    Layout.fillWidth: true
                    visible: root.parentPath.length > 0 && root.parentPath !== "/"
                    text: root.parentPath
                    color: "#475569"
                    font.pixelSize: 10
                    font.family: "Consolas, monospace"
                    elide: Text.ElideLeft
                }
            }
        }

        // Action Indicator
        Rectangle {
            implicitWidth: 32
            implicitHeight: 32
            radius: 16
            color: {
                if (root.isFolder) return "transparent"
                if (root.isPlaying) return "#2563eb"
                if (cardMouse.containsMouse) return "#1d4ed8"
                return "transparent"
            }
            visible: !root.isFolder

            Text {
                anchors.centerIn: parent
                text: root.isPlaying ? "❚❚" : "▶"
                color: (cardMouse.containsMouse || root.isPlaying) ? "#ffffff" : "#475569"
                font.pixelSize: root.isPlaying ? 11 : 12
            }
        }

        // Folder Tag
        Rectangle {
            visible: root.isFolder
            implicitWidth: 54
            implicitHeight: 22
            radius: 4
            color: "#1e293b"

            Text {
                anchors.centerIn: parent
                text: "Folder"
                color: "#94a3b8"
                font.pixelSize: 10
            }
        }
    }

    MouseArea {
        id: cardMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.isFolder ? Qt.ArrowCursor : Qt.PointingHandCursor
        onClicked: {
            if (!root.isFolder && root.playUrl.length > 0) {
                root.playRequested(root.playUrl, root.title)
            }
        }
    }

    ToolTip.visible: cardMouse.containsMouse
    ToolTip.delay: 500
    ToolTip.text: root.isFolder ? ("Folder: " + root.title) : (root.title + "\nSize: " + root.formattedSize + "\nPath: " + root.parentPath)
}
