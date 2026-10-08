import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "MediaFormatter.js" as Formatter
import "Theme.js" as Theme

// Streaming-service style tile for one search result.
// Sized by its parent (GridView cell); the visible card is inset by 7px so
// hover-zoom has room to breathe.
Item {
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
    readonly property var meta: Formatter.describe(root.title, root.isFolder, root.formattedSize)
    readonly property bool hovered: rowMouse.containsMouse
    readonly property string posterTop: Theme.posterTop(root.title)
    readonly property string posterBottom: Theme.posterBottom(root.title)

    readonly property string infoLine: {
        if (root.isFolder) return "Folder"
        var parts = []
        if (root.meta.episode.length > 0) parts.push(root.meta.episode)
        else if (root.meta.year.length > 0) parts.push(root.meta.year)
        if (root.meta.source.length > 0) parts.push(root.meta.source)
        if (root.meta.codec.length > 0) parts.push(root.meta.codec)
        if (root.formattedSize.length > 0 && root.formattedSize !== "Folder") parts.push(root.formattedSize)
        return parts.join("  \u00B7  ")
    }

    // Raise the hovered tile above its neighbours while it is scaled up
    z: root.hovered ? 20 : 0

    // Small label chip
    component Badge: Rectangle {
        property string label: ""
        property color labelColor: "#FFFFFF"

        implicitHeight: 20
        implicitWidth: badgeText.implicitWidth + 14
        radius: 5
        color: "#99000000"
        border.width: 1
        border.color: "#2EFFFFFF"

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: parent.label
            color: parent.labelColor
            font.pixelSize: 10
            font.weight: Font.Bold
            font.letterSpacing: 0.6
        }
    }

    Rectangle {
        id: card

        anchors.fill: parent
        anchors.margins: 7
        radius: 12
        clip: true
        scale: (root.hovered && !root.isFolder) ? 1.045 : (root.hovered ? 1.015 : 1.0)

        Behavior on scale { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: root.isFolder ? Theme.surfaceHi : root.posterTop }
            GradientStop { position: 1.0; color: root.isFolder ? Theme.surface : root.posterBottom }
        }

        // Soft top gloss
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: parent.height * 0.55
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1AFFFFFF" }
                GradientStop { position: 1.0; color: "#00FFFFFF" }
            }
        }

        // Giant faded initial: typographic poster art
        Text {
            visible: !root.isFolder
            anchors.right: parent.right
            anchors.rightMargin: -card.width * 0.03
            anchors.top: parent.top
            anchors.topMargin: -card.height * 0.2
            text: root.parsed.title.length > 0 ? root.parsed.title.charAt(0).toUpperCase() : ""
            color: "#14FFFFFF"
            font.pixelSize: card.height * 1.25
            font.weight: Font.Black
        }

        // Folder glyph
        Loader {
            active: root.isFolder
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -12
            sourceComponent: FluxIcon {
                name: "folder"
                size: 46
                strokeWidth: 1.6
                color: "#4DFFFFFF"
            }
        }

        // Bottom scrim for legibility
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.8
            radius: card.radius
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 0.55; color: "#99000000" }
                GradientStop { position: 1.0; color: "#EB000000" }
            }
        }

        // Title block
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: 12
            spacing: 3

            Text {
                Layout.fillWidth: true
                visible: root.libraryName.length > 0
                text: root.libraryName.toUpperCase()
                color: root.isPlaying ? Theme.accentHover : "#B3FFFFFF"
                font.pixelSize: 9
                font.weight: Font.Bold
                font.letterSpacing: 1.1
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.parsed.title
                color: "#FFFFFF"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: root.infoLine.length > 0
                text: root.infoLine
                color: "#B8FFFFFF"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        // Quality badges (top-left)
        RowLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            spacing: 6
            visible: !root.isFolder

            Badge {
                visible: root.meta.resolution.length > 0
                label: root.meta.resolution
                labelColor: root.meta.resolution === "4K" ? Theme.warning : "#FFFFFF"
            }

            Badge {
                visible: root.meta.hdr.length > 0
                label: root.meta.hdr
                labelColor: Theme.warning
            }
        }

        // Extension (top-right) or live "playing" chip
        Badge {
            visible: !root.isPlaying && !root.isFolder && root.extension.length > 0
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            label: root.extension.toUpperCase()
            labelColor: "#CCFFFFFF"
        }

        Badge {
            visible: root.isFolder
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            label: "FOLDER"
            labelColor: "#CCFFFFFF"
        }

        Rectangle {
            visible: root.isPlaying
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            implicitHeight: 22
            implicitWidth: playingRow.implicitWidth + 16
            radius: 11
            color: Theme.accent

            RowLayout {
                id: playingRow
                anchors.centerIn: parent
                spacing: 6

                Equalizer {
                    color: "#FFFFFF"
                    running: root.isPlaying
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: "PLAYING"
                    color: "#FFFFFF"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    font.letterSpacing: 0.8
                }
            }
        }

        // Hover dim + play button
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "#000000"
            opacity: (root.hovered && !root.isFolder) ? 0.3 : (root.hovered ? 0.1 : 0.0)

            Behavior on opacity { NumberAnimation { duration: 160 } }
        }

        Rectangle {
            visible: !root.isFolder
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -10
            width: 54
            height: 54
            radius: 27
            color: "#F2FFFFFF"
            opacity: root.hovered ? 1.0 : 0.0
            scale: root.hovered ? 1.0 : 0.65

            Behavior on opacity { NumberAnimation { duration: 160 } }
            Behavior on scale { NumberAnimation { duration: 200; easing.type: Easing.OutBack } }

            Loader {
                active: root.hovered && !root.isFolder
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: 2
                sourceComponent: FluxIcon {
                    name: "play"
                    size: 28
                    color: Theme.bg
                }
            }
        }

        // Now-playing progress accent
        Rectangle {
            visible: root.isPlaying
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            color: Theme.accent
        }

        // Frame
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "transparent"
            border.width: root.isPlaying ? 2 : 1
            border.color: root.isPlaying ? Theme.accent
                                         : (root.hovered && !root.isFolder ? "#66FFFFFF" : "#1FFFFFFF")

            Behavior on border.color { ColorAnimation { duration: 140 } }
        }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        anchors.margins: 7
        hoverEnabled: true
        cursorShape: root.isFolder ? Qt.ArrowCursor : Qt.PointingHandCursor

        onClicked: {
            if (!root.isFolder && root.playUrl.length > 0) {
                root.playRequested(root.playUrl, root.parsed.title)
            }
        }
    }

    FluxToolTip {
        visible: rowMouse.containsMouse && root.title.length > 30
        delay: 600
        text: root.title + (root.parentPath.length > 0 ? ("\n" + root.parentPath) : "")
    }
}
