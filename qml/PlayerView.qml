import QtQuick
import QtQuick.Controls
import Flux.Media 1.0
import "components"

Rectangle {
    id: root

    property var player: null
    signal toggleFullscreenRequested()

    color: "#000000"
    radius: 8
    clip: true

    // Embedded VLC Video Surface
    VLCVideoItem {
        id: videoSurface
        anchors.fill: parent
        player: root.player
    }

    // Idle Placeholder
    Column {
        anchors.centerIn: parent
        spacing: 12
        visible: (!root.player || !root.player.isPlaying && !root.player.isBuffering && !videoSurface.hasVideoFrame)

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "FLUX"
            font.pixelSize: 42
            font.bold: true
            color: "#1e293b"
            font.letterSpacing: 4
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Ready to stream directly from BDIX (172.16.50.14)"
            font.pixelSize: 13
            color: "#475569"
        }
    }

    // Buffering Spinner Overlay
    Rectangle {
        anchors.centerIn: parent
        width: 140
        height: 60
        radius: 8
        color: "#cc0f172a"
        border.color: "#334155"
        visible: root.player && root.player.isBuffering

        Column {
            anchors.centerIn: parent
            spacing: 4

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "⏳ Buffering..."
                color: "#38bdf8"
                font.pixelSize: 13
                font.bold: true
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: root.player ? Math.round(root.player.bufferingPercent) + "%" : ""
                color: "#94a3b8"
                font.pixelSize: 11
                font.family: "Consolas, monospace"
            }
        }
    }

    // Playback Controls Bar (bottom overlay)
    PlaybackControls {
        id: controlsBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 12
        player: root.player
        onToggleFullscreenRequested: root.toggleFullscreenRequested()
    }

    // Double-click to toggle fullscreen
    MouseArea {
        anchors.fill: parent
        anchors.bottomMargin: controlsBar.height + 12
        onDoubleClicked: root.toggleFullscreenRequested()
    }
}
