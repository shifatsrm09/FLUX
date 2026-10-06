import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"

ApplicationWindow {
    id: window

    visible: true
    width: 1280
    height: 820
    minimumWidth: 960
    minimumHeight: 640
    title: "FLUX"
    color: "#070a13"

    property bool isFullscreen: false
    property string currentPage: "search" // "search" | "player"
    property string currentPlayingTitle: ""

    function toggleFullscreen() {
        isFullscreen = !isFullscreen
        if (isFullscreen) {
            window.showFullScreen()
        } else {
            window.showNormal()
        }
    }

    function playMedia(url, title) {
        currentPlayingTitle = title
        currentPage = "player"
        if (fluxPlayer) {
            fluxPlayer.play(url)
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (isFullscreen) {
                toggleFullscreen()
            } else if (currentPage === "player") {
                currentPage = "search"
            }
        }
    }

    Shortcut {
        sequence: "Space"
        onActivated: {
            if (currentPage === "player" && fluxPlayer) {
                fluxPlayer.togglePlay()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // Top Navigation Bar (Hidden in fullscreen & hidden in player view)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            height: 60
            color: "#0a0f1d"
            border.color: "#162035"
            border.width: 1
            visible: !isFullscreen && currentPage !== "player"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 24
                anchors.rightMargin: 24
                spacing: 16

                // Logo Brand
                RowLayout {
                    spacing: 10

                    Text {
                        text: "FLUX"
                        font.pixelSize: 22
                        font.bold: true
                        color: "#ffffff"
                        font.letterSpacing: 2
                    }

                    Rectangle {
                        implicitWidth: 84
                        implicitHeight: 20
                        radius: 10
                        color: "#1e3a8a"

                        Text {
                            anchors.centerIn: parent
                            text: "BDIX STREAM"
                            color: "#93c5fd"
                            font.pixelSize: 9
                            font.bold: true
                            font.letterSpacing: 1
                        }
                    }
                }

                Item { Layout.fillWidth: true }

                // Navigation Items
                RowLayout {
                    spacing: 12

                    // Search Nav
                    Button {
                        id: navSearchBtn
                        implicitHeight: 34
                        background: Rectangle {
                            radius: 6
                            color: currentPage === "search" ? "#1e293b" : "transparent"
                        }
                        contentItem: Text {
                            text: "Search"
                            color: currentPage === "search" ? "#ffffff" : "#94a3b8"
                            font.pixelSize: 13
                            font.bold: currentPage === "search"
                        }
                        onClicked: currentPage = "search"
                    }

                    // Return to Player if already active
                    Button {
                        id: navPlayerBtn
                        visible: fluxPlayer && fluxPlayer.isPlaying
                        implicitHeight: 34
                        background: Rectangle {
                            radius: 6
                            color: "#1e3a8a"
                        }
                        contentItem: RowLayout {
                            spacing: 6
                            Text {
                                text: "▶"
                                color: "#60a5fa"
                                font.pixelSize: 11
                            }
                            Text {
                                text: "Now Playing"
                                color: "#ffffff"
                                font.pixelSize: 12
                                font.bold: true
                            }
                        }
                        onClicked: currentPage = "player"
                    }

                    // Developer Tools & Test Catalog Toggle
                    Button {
                        id: navDevBtn
                        implicitHeight: 34
                        background: Rectangle {
                            radius: 6
                            color: navDevBtn.hovered ? "#1e293b" : "transparent"
                            border.color: "#334155"
                            border.width: 1
                        }
                        contentItem: RowLayout {
                            spacing: 6
                            Text {
                                text: "⚙"
                                color: "#94a3b8"
                                font.pixelSize: 12
                            }
                            Text {
                                text: "Dev Tools"
                                color: "#94a3b8"
                                font.pixelSize: 12
                            }
                        }
                        onClicked: devDrawer.open()
                    }
                }
            }
        }

        // =====================================================================
        // Main Content View Area
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Page 1: Search View
            SearchPage {
                anchors.fill: parent
                visible: currentPage === "search"
                onPlayMediaRequested: function(url, title) {
                    window.playMedia(url, title)
                }
            }

            // Page 2: Dedicated Player View
            PlayerView {
                anchors.fill: parent
                visible: currentPage === "player"
                player: fluxPlayer
                mediaTitle: window.currentPlayingTitle
                onToggleFullscreenRequested: window.toggleFullscreen()
                onBackRequested: {
                    currentPage = "search"
                }
            }
        }
    }

    // Developer Tools Slide-Out Drawer
    DevToolsDrawer {
        id: devDrawer
        player: fluxPlayer
        logger: fluxLogger
        catalogModel: testMediaModel
        onPlayTestRequested: function(url, title) {
            window.playMedia(url, title)
        }
    }
}
