import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "components/Theme.js" as Theme

ApplicationWindow {
    id: window

    visible: true
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    title: "FLUX"
    color: Theme.bg
    font.family: Theme.fontFamily
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

    function returnToHome() {
        if (fluxPlayer) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = "search"
        if (fluxSearch) {
            fluxSearch.clear()
        }
    }

    function returnToSearch() {
        if (fluxPlayer) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = "search"
    }

    // ---- Shortcuts ---------------------------------------------------------------

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (isFullscreen) {
                toggleFullscreen()
            } else if (currentPage === "player") {
                if (playerView.menuOpen) {
                    playerView.closeMenus()
                } else {
                    window.returnToSearch()
                }
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

    // Player-only conveniences
    Shortcut {
        sequence: "Left"
        enabled: currentPage === "player"
        onActivated: {
            if (fluxPlayer) fluxPlayer.seekRelative(-10000)
        }
    }

    Shortcut {
        sequence: "Right"
        enabled: currentPage === "player"
        onActivated: {
            if (fluxPlayer) fluxPlayer.seekRelative(10000)
        }
    }

    Shortcut {
        sequence: "Up"
        enabled: currentPage === "player"
        onActivated: {
            if (fluxPlayer) fluxPlayer.volume = Math.min(100, fluxPlayer.volume + 5)
        }
    }

    Shortcut {
        sequence: "Down"
        enabled: currentPage === "player"
        onActivated: {
            if (fluxPlayer) fluxPlayer.volume = Math.max(0, fluxPlayer.volume - 5)
        }
    }

    Shortcut {
        sequence: "M"
        enabled: currentPage === "player"
        onActivated: {
            if (fluxPlayer) fluxPlayer.muted = !fluxPlayer.muted
        }
    }

    Shortcut {
        sequence: "F"
        enabled: currentPage === "player"
        onActivated: toggleFullscreen()
    }

    // ---- Stage: pages crossfade; nav bar floats on top ----------------------------------
    Item {
        id: stage
        anchors.fill: parent

        // Search / browse experience
        SearchPage {
            id: searchPage

            anchors.fill: parent
            topInset: customTitleBar.height
            opacity: currentPage === "search" ? 1.0 : 0.0
            visible: opacity > 0.0

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            onPlayMediaRequested: function(url, title) {
                window.playMedia(url, title)
            }
        }

        // Cinematic full-viewport player
        PlayerView {
            id: playerView

            anchors.fill: parent
            opacity: currentPage === "player" ? 1.0 : 0.0
            visible: opacity > 0.0
            player: fluxPlayer
            mediaTitle: window.currentPlayingTitle
            isFullscreen: window.isFullscreen

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            onToggleFullscreenRequested: window.toggleFullscreen()
            onBackRequested: window.returnToSearch()
        }

        // Nav / title bar (frameless window chrome)
        ClassicTitleBar {
            id: customTitleBar

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            z: 100
            window: window
            solid: searchPage.navSolid
            overlayMode: currentPage === "player"
            shown: !window.isFullscreen && (currentPage !== "player" || playerView.showControls)
            showNowPlaying: !!fluxPlayer && fluxPlayer.isPlaying && currentPage !== "player"

            onNowPlayingClicked: currentPage = "player"
            onDevToolsClicked: devDrawer.open()
            onHomeClicked: window.returnToHome()
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
