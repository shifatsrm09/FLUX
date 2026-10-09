import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "components/Theme.js" as Theme
import "components/MediaFormatter.js" as Formatter

ApplicationWindow {
    id: window

    visible: true
    // Restore the last window size (clamped to the minimum); position is left to the OS
    width: fluxUser ? Math.max(960, fluxUser.intValue("window/width", 1280)) : 1280
    height: fluxUser ? Math.max(600, fluxUser.intValue("window/height", 800)) : 800
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

        // Read the saved position BEFORE noteStart() touches the history entry
        var resumeMs = fluxUser ? fluxUser.resumePositionFor(url) : 0
        if (fluxUser) fluxUser.noteStart(url, title)

        // Look up the next episode in the background (for the "Up next" card)
        if (fluxBrowser) fluxBrowser.findNext(url)

        playerView.offerResume(resumeMs)

        if (fluxPlayer) {
            if (resumeMs > 0) {
                fluxPlayer.playFrom(url, resumeMs)
            } else {
                fluxPlayer.play(url)
            }
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
        if (fluxBrowser) {
            fluxBrowser.close()
        }
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
            } else if (currentPage === "search" && fluxBrowser && fluxBrowser.active) {
                // Inside a folder: step out one level (closes the browser at the top)
                fluxBrowser.goUp()
            }
        }
    }

    // Space / arrows / M / F are handled by the stage's Keys handler below (it owns keyboard
    // focus while the player is open). Window-wide shortcuts that don't depend on focus:
    Shortcut {
        sequence: "Ctrl+D"
        onActivated: devDrawer.open()
    }

    Shortcut {
        sequence: "F11"
        onActivated: toggleFullscreen()
    }

    // ---- Keyboard focus management -------------------------------------------------
    Component.onCompleted: {
        if (fluxUser && fluxUser.boolValue("window/maximized", false)) {
            window.showMaximized()
        }
    }

    onClosing: function(close) {
        if (!fluxUser) return
        fluxUser.setValue("window/maximized", window.visibility === Window.Maximized)
        // Only remember the size of a normal window (not maximized / fullscreen)
        if (window.visibility === Window.Windowed) {
            fluxUser.setValue("window/width", window.width)
            fluxUser.setValue("window/height", window.height)
        }
    }
    onCurrentPageChanged: {
        if (currentPage === "player") stage.forceActiveFocus()
    }

    onActiveChanged: {
        if (active && currentPage === "player") stage.forceActiveFocus()
    }

    Connections {
        target: playerView

        function onMenuOpenChanged() {
            // Popup closed: hand keyboard focus back so keys keep working
            if (!playerView.menuOpen) stage.forceActiveFocus()
        }
    }

    Connections {
        target: devDrawer

        function onClosed() {
            if (currentPage === "player") stage.forceActiveFocus()
        }
    }

    // ---- Stage: pages crossfade; nav bar floats on top ----------------------------------
    Item {
        id: stage
        anchors.fill: parent
        focus: true

        // Player keyboard controls
        Keys.onPressed: function(event) {
            if (currentPage !== "player") return

            var shift = (event.modifiers & Qt.ShiftModifier) !== 0

            switch (event.key) {
            case Qt.Key_Space:
                if (!event.isAutoRepeat && fluxPlayer) fluxPlayer.togglePlay()
                event.accepted = true
                break
            case Qt.Key_Left:
                playerView.seekBy(shift ? -60000 : -10000)
                event.accepted = true
                break
            case Qt.Key_Right:
                playerView.seekBy(shift ? 60000 : 10000)
                event.accepted = true
                break
            case Qt.Key_Up:
                playerView.adjustVolume(5)
                event.accepted = true
                break
            case Qt.Key_Down:
                playerView.adjustVolume(-5)
                event.accepted = true
                break
            case Qt.Key_M:
                if (!event.isAutoRepeat) playerView.toggleMute()
                event.accepted = true
                break
            case Qt.Key_F:
                if (!event.isAutoRepeat) window.toggleFullscreen()
                event.accepted = true
                break
            }
        }

        // Any click while playing re-claims keyboard focus (without blocking the click)
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onTapped: {
                if (currentPage === "player") stage.forceActiveFocus()
            }
        }

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
            nextUrl: fluxBrowser ? fluxBrowser.nextUrl : ""
            nextName: fluxBrowser ? fluxBrowser.nextName : ""
            autoplayNext: fluxUser ? fluxUser.autoplayNext : true

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            onToggleFullscreenRequested: window.toggleFullscreen()
            onBackRequested: window.returnToSearch()
            onPlayNextRequested: function(url, name) {
                window.playMedia(url, Formatter.formatMedia(name, false, "").title)
            }
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
