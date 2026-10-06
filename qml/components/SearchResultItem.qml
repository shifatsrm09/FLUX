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
    property bool isPlaying: false

    signal playRequested(string url)

    implicitWidth: 380
    implicitHeight: 64
    radius: 6
    color: {
        if (root.isPlaying) return "#1e293b"
        if (mouseArea.containsMouse && !root.isFolder) return "#1e293b"
        if (mouseArea.containsMouse && root.isFolder) return "#111827"
        return "#0f172a"
    }
    border.color: {
        if (root.isPlaying) return "#3b82f6"
        if (mouseArea.containsMouse && !root.isFolder) return "#475569"
        return "#1e293b"
    }
    border.width: root.isPlaying ? 2 : 1

    Behavior on color { ColorAnimation { duration: 120 } }
    Behavior on border.color { ColorAnimation { duration: 120 } }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        // Type Icon
        Rectangle {
            implicitWidth: 36
            implicitHeight: 36
            radius: 6
            color: root.isFolder ? "#1e293b" : (root.isPlaying ? "#1d4ed8" : "#1e3a8a")

            Text {
                anchors.centerIn: parent
                text: root.isFolder ? "📁" : (root.isPlaying ? "▶" : "🎬")
                font.pixelSize: 16
            }
        }

        // Info Block (Title & Parent Path)
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: root.title
                    color: root.isPlaying ? "#60a5fa" : (root.isFolder ? "#cbd5e1" : "#f8fafc")
                    font.pixelSize: 12
                    font.bold: true
                    elide: Text.ElideRight
                }

                // Extension Badge
                Rectangle {
                    visible: root.extension.length > 0
                    implicitWidth: extText.implicitWidth + 8
                    implicitHeight: 18
                    radius: 3
                    color: root.isFolder ? "#334155" : (root.extension === "MKV" || root.extension === "MP4" ? "#1e3a8a" : "#374151")

                    Text {
                        id: extText
                        anchors.centerIn: parent
                        text: root.extension
                        color: root.isFolder ? "#94a3b8" : (root.extension === "MKV" || root.extension === "MP4" ? "#93c5fd" : "#d1d5db")
                        font.pixelSize: 9
                        font.bold: true
                        font.family: "Consolas, monospace"
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: root.parentPath
                    color: "#64748b"
                    font.pixelSize: 10
                    font.family: "Consolas, monospace"
                    elide: Text.ElideLeft
                }

                // Size Badge or Folder Tag
                Rectangle {
                    implicitWidth: sizeText.implicitWidth + 8
                    implicitHeight: 16
                    radius: 3
                    color: root.isFolder ? "transparent" : "#064e3b"

                    Text {
                        id: sizeText
                        anchors.centerIn: parent
                        text: root.formattedSize
                        color: root.isFolder ? "#64748b" : "#6ee7b7"
                        font.pixelSize: 10
                        font.bold: true
                        font.family: "Consolas, monospace"
                    }
                }
            }
        }

        // Action indicator / Play hint
        Rectangle {
            implicitWidth: 28
            implicitHeight: 28
            radius: 14
            color: mouseArea.containsMouse && !root.isFolder ? "#2563eb" : "transparent"
            visible: !root.isFolder

            Text {
                anchors.centerIn: parent
                text: "▶"
                color: mouseArea.containsMouse ? "#ffffff" : "#475569"
                font.pixelSize: 11
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

    ToolTip.visible: mouseArea.containsMouse && root.isFolder
    ToolTip.delay: 400
    ToolTip.text: "Directory: " + root.title + " (cannot be played directly)"
}
