import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"

ApplicationWindow {
    id: window

    visible: true
    width: 1280
    height: 840
    minimumWidth: 960
    minimumHeight: 640
    title: "FLUX — Native Media Player"
    color: "#080c14"

    property bool isFullscreen: false

    function toggleFullscreen() {
        isFullscreen = !isFullscreen
        if (isFullscreen) {
            window.showFullScreen()
        } else {
            window.showNormal()
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (isFullscreen) toggleFullscreen()
        }
    }

    Shortcut {
        sequence: "Space"
        onActivated: {
            if (fluxPlayer) fluxPlayer.togglePlay()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: isFullscreen ? 0 : 16
        spacing: 12

        // Top Header (Hidden in fullscreen)
        RowLayout {
            Layout.fillWidth: true
            visible: !isFullscreen
            spacing: 12

            Text {
                text: "FLUX"
                font.pixelSize: 22
                font.bold: true
                color: "#ffffff"
                font.letterSpacing: 2
            }

            Rectangle {
                implicitWidth: badgeText.implicitWidth + 12
                implicitHeight: 22
                radius: 11
                color: "#1e3a8a"

                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: "libVLC Native Direct Streamer • v0.1.0"
                    color: "#93c5fd"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Target: 172.16.50.14 (HTTP Byte Ranges)"
                color: "#64748b"
                font.pixelSize: 11
                font.family: "Consolas, monospace"
            }
        }

        // Hardcoded Test Catalog Section (Hidden in fullscreen)
        ColumnLayout {
            Layout.fillWidth: true
            visible: !isFullscreen
            spacing: 6

            Text {
                text: "TEST CATALOG (172.16.50.14)"
                color: "#94a3b8"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Repeater {
                    model: testMediaModel

                    MediaCard {
                        Layout.fillWidth: true
                        title: model.title
                        year: model.year
                        badge: model.badge
                        description: model.description
                        url: model.url
                        isSelected: (fluxPlayer && fluxPlayer.url === model.url)
                        onClicked: {
                            urlInput.text = model.url
                            if (fluxPlayer) fluxPlayer.play(model.url)
                        }
                    }
                }
            }
        }

        // Custom URL Input Row (Hidden in fullscreen)
        RowLayout {
            Layout.fillWidth: true
            visible: !isFullscreen
            spacing: 8

            TextField {
                id: urlInput
                Layout.fillWidth: true
                implicitHeight: 42
                placeholderText: "Paste media stream URL: http://172.16.50.14/... (MKV / MP4)"
                text: fluxPlayer && fluxPlayer.url.length > 0 ? fluxPlayer.url : "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/Ant-Man%20and%20the%20Wasp-Quantumania%20%282023%29%201080p%20DSNP/Ant-Man%20and%20the%20Wasp%20Quantumania%20%282023%29%201080p%20DSNP-WEB%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-PSA.mkv"
                font.pixelSize: 12
                font.family: "Consolas, monospace"
                color: "#f8fafc"
                selectedTextColor: "#ffffff"
                selectionColor: "#2563eb"

                background: Rectangle {
                    color: "#0f172a"
                    border.color: urlInput.activeFocus ? "#3b82f6" : "#1e293b"
                    border.width: urlInput.activeFocus ? 2 : 1
                    radius: 6
                }

                onAccepted: {
                    if (fluxPlayer && text.trim().length > 0) {
                        fluxPlayer.play(text.trim())
                    }
                }
            }

            Button {
                id: playUrlBtn
                implicitWidth: 100
                implicitHeight: 42
                background: Rectangle {
                    color: playUrlBtn.down ? "#1d4ed8" : (playUrlBtn.hovered ? "#2563eb" : "#3b82f6")
                    radius: 6
                }
                contentItem: Text {
                    text: "▶  PLAY"
                    color: "#ffffff"
                    font.pixelSize: 13
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (fluxPlayer && urlInput.text.trim().length > 0) {
                        fluxPlayer.play(urlInput.text.trim())
                    }
                }
            }

            Button {
                id: stopUrlBtn
                implicitWidth: 80
                implicitHeight: 42
                background: Rectangle {
                    color: stopUrlBtn.down ? "#334155" : (stopUrlBtn.hovered ? "#1e293b" : "#0f172a")
                    border.color: "#334155"
                    border.width: 1
                    radius: 6
                }
                contentItem: Text {
                    text: "■  STOP"
                    color: "#94a3b8"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: {
                    if (fluxPlayer) fluxPlayer.stop()
                }
            }
        }

        // Video Viewport
        PlayerView {
            id: playerView
            Layout.fillWidth: true
            Layout.fillHeight: true
            player: fluxPlayer
            onToggleFullscreenRequested: window.toggleFullscreen()
        }

        // Diagnostics Drawer (Hidden in fullscreen)
        DiagnosticPanel {
            Layout.fillWidth: true
            visible: !isFullscreen
            player: fluxPlayer
            logger: fluxLogger
        }
    }
}
