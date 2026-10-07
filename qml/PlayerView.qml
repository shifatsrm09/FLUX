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
        height: 72
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 180 } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#CC000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 16

            Button {
                id: backBtn
                implicitHeight: 34
                implicitWidth: backRow.implicitWidth + 20

                background: Rectangle {
                    radius: 6
                    color: backBtn.down ? "#1F2430" : (backBtn.hovered ? "#161922" : "#0D0F14")
                    border.color: backBtn.hovered ? "#38BDF8" : "#222634"
                    border.width: 1
                }

                contentItem: RowLayout {
                    id: backRow
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "←"
                        color: backBtn.hovered ? "#38BDF8" : "#F5F5F5"
                        font.pixelSize: 13
                    }

                    Text {
                        text: "Back"
                        color: backBtn.hovered ? "#38BDF8" : "#F5F5F5"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                    }
                }

                onClicked: {
                    if (root.player) {
                        root.player.pause()
                    }
                    root.backRequested()
                }
            }

            Text {
                Layout.fillWidth: true
                text: root.mediaTitle
                color: "#F5F5F5"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }
    }

    // Buffering Spinner Overlay
    Rectangle {
        anchors.centerIn: parent
        implicitWidth: 120
        implicitHeight: 44
        radius: 6
        color: "#E60A0D13"
        border.color: "#1E222D"
        border.width: 1
        visible: root.player && root.player.isBuffering

        RowLayout {
            anchors.centerIn: parent
            spacing: 10

            BusyIndicator {
                implicitWidth: 16
                implicitHeight: 16
                running: true
            }

            Text {
                text: "Buffering"
                color: "#E2E8F0"
                font.pixelSize: 12
            }
        }
    }

    // Playback Controls Bar (bottom overlay, autohides)
    PlaybackControls {
        id: controlsBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        player: root.player
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0

        Behavior on opacity { NumberAnimation { duration: 180 } }

        onToggleFullscreenRequested: root.toggleFullscreenRequested()
    }
}
