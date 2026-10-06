import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Flux.Media 1.0
import "components"

Rectangle {
    id: root

    property var player: null
    property string mediaTitle: ""
    signal toggleFullscreenRequested()
    signal backRequested()

    color: "#000000"
    clip: true

    property bool showControls: true

    Timer {
        id: hideControlsTimer
        interval: 3500
        running: root.player && root.player.isPlaying && !controlsBar.isUserInteracting
        repeat: false
        onTriggered: root.showControls = false
    }

    function wakeControls() {
        root.showControls = true
        hideControlsTimer.restart()
    }

    // Embedded VLC Video Surface
    VLCVideoItem {
        id: videoSurface
        anchors.fill: parent
        player: root.player
    }

    // Mouse Tracking Area for waking controls & double click fullscreen
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onPositionChanged: root.wakeControls()
        onClicked: root.wakeControls()
        onDoubleClicked: root.toggleFullscreenRequested()
    }

    // Top Navigation & Title Bar (Fades in/out)
    Rectangle {
        id: topBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#d0000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            spacing: 16

            Button {
                id: backBtn
                implicitHeight: 36
                implicitWidth: backRow.implicitWidth + 24

                background: Rectangle {
                    radius: 18
                    color: backBtn.down ? "#1d4ed8" : (backBtn.hovered ? "#2563eb" : "#cc0f172a")
                    border.color: "#334155"
                    border.width: 1
                }

                contentItem: RowLayout {
                    id: backRow
                    anchors.centerIn: parent
                    spacing: 8

                    Text {
                        text: "←"
                        color: "#ffffff"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        text: "Back to Search"
                        color: "#ffffff"
                        font.pixelSize: 12
                        font.bold: true
                    }
                }

                onClicked: root.backRequested()
            }

            Text {
                Layout.fillWidth: true
                text: root.mediaTitle.length > 0 ? root.mediaTitle : (root.player ? root.player.url : "")
                color: "#f8fafc"
                font.pixelSize: 14
                font.bold: true
                elide: Text.ElideRight
            }
        }
    }

    // Buffering Spinner Overlay
    Rectangle {
        anchors.centerIn: parent
        width: 140
        height: 54
        radius: 8
        color: "#d90b1329"
        border.color: "#1e293b"
        visible: root.player && root.player.isBuffering

        RowLayout {
            anchors.centerIn: parent
            spacing: 10

            BusyIndicator {
                implicitWidth: 20
                implicitHeight: 20
                running: true
            }

            ColumnLayout {
                spacing: 2
                Text {
                    text: "Buffering..."
                    color: "#38bdf8"
                    font.pixelSize: 12
                    font.bold: true
                }
                Text {
                    text: root.player ? Math.round(root.player.bufferingPercent) + "%" : ""
                    color: "#94a3b8"
                    font.pixelSize: 10
                    font.family: "Consolas, monospace"
                }
            }
        }
    }

    // Playback Controls Bar (bottom overlay, autohides)
    PlaybackControls {
        id: controlsBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20
        player: root.player
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        onToggleFullscreenRequested: root.toggleFullscreenRequested()
    }
}
