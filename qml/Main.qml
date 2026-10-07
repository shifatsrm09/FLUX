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
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinMaxButtonsHint

    property bool isFullscreen: false
    property string currentPage: "search" // "search" | "player"
    property string currentPlayingTitle: ""

    function toggleMaximize() {
        if (window.visibility === Window.Maximized) {
            window.showNormal()
        } else {
            window.showMaximized()
        }
    }

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

    Shortcut {
        sequence: "F11"
        onActivated: toggleFullscreen()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // =====================================================================
        // Classic Custom Titlebar (Native Window Controls)
        // =====================================================================
        ClassicTitleBar {
            id: customTitleBar
            Layout.fillWidth: true
            height: 36
            window: window
            visible: !window.isFullscreen
            showNowPlaying: fluxPlayer && fluxPlayer.isPlaying && currentPage !== "player"
            onNowPlayingClicked: currentPage = "player"
            onDevToolsClicked: devDrawer.open()
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

    // Frameless Window Edge Resize Handles
    WindowResizeBorders {
        window: window
    }
}
