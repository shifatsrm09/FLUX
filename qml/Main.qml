import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"

ApplicationWindow {
    id: window

    visible: true
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: "FLUX"
    color: "#08090C"

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

    Shortcut {
        sequence: "Ctrl+D"
        onActivated: devDrawer.open()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // Top Navigation Bar (Hidden in fullscreen & player)
        // =====================================================================
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: "#08090C"
            visible: !isFullscreen && currentPage !== "player"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 28
                anchors.rightMargin: 28
                spacing: 20

                // Brand Logo
                Text {
                    text: "FLUX"
                    font.pixelSize: 18
                    font.weight: Font.Bold
                    color: "#F5F5F5"
                    font.letterSpacing: 3
                }

                Item { Layout.fillWidth: true }

                // Clean Navigation Links
                RowLayout {
                    spacing: 20

                    // Search Link
                    Text {
                        text: "Search"
                        color: currentPage === "search" ? "#F5F5F5" : "#8F96A3"
                        font.pixelSize: 13
                        font.weight: currentPage === "search" ? Font.Medium : Font.Normal

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: currentPage = "search"
                        }
                    }

                    // Return to Player if already streaming
                    Text {
                        visible: fluxPlayer && fluxPlayer.isPlaying
                        text: "Now Playing ▶"
                        color: "#38BDF8"
                        font.pixelSize: 13
                        font.weight: Font.Medium

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: currentPage = "player"
                        }
                    }

                    // Unobtrusive Developer & Settings Access
                    Item {
                        implicitWidth: 24
                        implicitHeight: 24

                        Text {
                            anchors.centerIn: parent
                            text: "···"
                            color: devMouse.containsMouse ? "#F5F5F5" : "#555C6A"
                            font.pixelSize: 16
                            font.weight: Font.Bold
                        }

                        MouseArea {
                            id: devMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: devDrawer.open()
                        }

                        ToolTip.visible: devMouse.containsMouse
                        ToolTip.delay: 500
                        ToolTip.text: "Developer Tools (Ctrl+D)"
                    }
                }
            }

            // Hairline bottom separator
            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: "#141721"
            }
        }

        // =====================================================================
        // Content Area (Search View or Player View)
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Search Experience
            SearchPage {
                anchors.fill: parent
                visible: currentPage === "search"
                onPlayMediaRequested: function(url, title) {
                    window.playMedia(url, title)
                }
            }

            // Cinematic Full-Viewport Player
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

    // Unobtrusive Developer Slide-Out Drawer
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
