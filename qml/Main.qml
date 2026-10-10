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
    property string currentPage: "search" // "search" | "library" | "player"
    property string pageBeforePlayer: "search" // where Back from the player returns to
    property string currentPlayingTitle: ""
    readonly property bool inTeleparty: !!fluxTeleparty && fluxTeleparty.active

    // Round floating button for the bottom-right dock (optional badge = active downloads)
    component DockButton: Rectangle {
        id: dockBtn

        property string iconName: "settings"
        property string tip: ""
        property int badge: 0
        property bool active: false
        signal clicked()

        width: 48
        height: 48
        radius: 24
        color: dockMouse.pressed ? Theme.surfaceTop
                                 : (dockMouse.containsMouse || dockBtn.active ? Theme.surfaceHi : "#E615151A")
        border.width: 1
        border.color: (dockMouse.containsMouse || dockBtn.active) ? Theme.borderHi : Theme.border
        scale: dockMouse.pressed ? 0.94 : 1.0

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on scale { NumberAnimation { duration: 90 } }

        FluxIcon {
            anchors.centerIn: parent
            name: dockBtn.iconName
            size: 22
            color: dockMouse.containsMouse ? "#FFFFFF" : Theme.textDim
            rotation: (dockBtn.iconName === "settings" && dockMouse.containsMouse) ? 30 : 0

            Behavior on rotation { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
        }

        Rectangle {
            visible: dockBtn.badge > 0
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: -3
            anchors.topMargin: -3
            implicitWidth: Math.max(20, badgeText.implicitWidth + 10)
            implicitHeight: 20
            width: implicitWidth
            height: implicitHeight
            radius: 10
            color: Theme.accent
            border.width: 2
            border.color: Theme.bg

            Text {
                id: badgeText
                anchors.centerIn: parent
                text: dockBtn.badge
                color: "#FFFFFF"
                font.pixelSize: 10
                font.weight: Font.Bold
            }
        }

        MouseArea {
            id: dockMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: dockBtn.clicked()
        }

        FluxToolTip {
            visible: dockMouse.containsMouse && dockBtn.tip.length > 0
            text: dockBtn.tip
            above: true
        }
    }

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

    function sameMediaUrl(a, b) {
        if (!a || !b) return false
        if (a === b) return true
        try {
            return decodeURIComponent(a) === decodeURIComponent(b)
        } catch (e) {
            return false
        }
    }

    function playMedia(url, title) {
        // Offline playback is disabled while in a Teleparty session
        if (window.inTeleparty && fluxSync && !fluxSync.isStreamUrl(url)) {
            return
        }

        // Clicking the video the party is already watching (e.g. from Continue Watching
        // after joining late) joins at the party's live timestamp instead of restarting from 0
        if (window.inTeleparty && fluxSync && fluxSync.hasPartyMedia
                && window.sameMediaUrl(fluxSync.currentUrl, url)) {
            fluxSync.joinPartyPlayback()
            return
        }

        currentPlayingTitle = title
        if (currentPage !== "player") pageBeforePlayer = currentPage
        currentPage = "player"

        // Read the saved position BEFORE noteStart() touches the history entry.
        // In a Teleparty everyone starts together from the beginning.
        var resumeMs = (!window.inTeleparty && fluxUser) ? fluxUser.resumePositionFor(url) : 0
        if (fluxUser) fluxUser.noteStart(url, title)

        // Look up the next episode in the background (for the "Up next" card)
        if (fluxBrowser) fluxBrowser.findNext(url)

        playerView.offerResume(resumeMs)

        if (window.inTeleparty && fluxSync) {
            fluxSync.broadcastOpen(url, title)
        }

        if (fluxPlayer) {
            if (resumeMs > 0) {
                fluxPlayer.playFrom(url, resumeMs)
            } else {
                fluxPlayer.play(url)
            }
        }
    }

    // Open a stream requested by another Teleparty member (never broadcast back)
    function openFromParty(url, title, startMs) {
        customTitleBar.closeMenu()
        if (settingsPanel.opened) settingsPanel.dismiss()
        if (downloadsPanel.opened) downloadsPanel.close()

        currentPlayingTitle = title
        if (currentPage !== "player") {
            pageBeforePlayer = (currentPage === "library") ? "search" : currentPage
        }
        currentPage = "player"

        if (fluxUser) fluxUser.noteStart(url, title)
        if (fluxBrowser) fluxBrowser.findNext(url)

        playerView.offerResume(0)

        if (fluxPlayer) {
            if (window.sameMediaUrl(fluxPlayer.url, url)
                    && (fluxPlayer.state === "Paused" || fluxPlayer.state === "Playing")) {
                if (startMs > 0) fluxPlayer.remoteSeekTo(startMs)
                if (fluxPlayer.isPaused) fluxPlayer.remoteResume()
            } else if (startMs > 0) {
                fluxPlayer.playFrom(url, startMs)
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

    // Back from the player: to the page it was started from (browse or Library)
    function returnToSearch() {
        if (fluxPlayer) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = (!window.inTeleparty && pageBeforePlayer === "library") ? "library" : "search"
    }

    function showLibrary() {
        if (window.inTeleparty) return
        if (fluxPlayer) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = "library"
    }

    // ---- Shortcuts ---------------------------------------------------------------

    Shortcut {
        sequence: "Escape"
        onActivated: {
            // Dialogs first: Escape closes the topmost one
            if (customTitleBar.menuOpen) {
                customTitleBar.closeMenu()
                return
            }
            if (settingsPanel.opened) {
                settingsPanel.dismiss()
                return
            }
            if (downloadsPanel.opened) {
                downloadsPanel.close()
                return
            }

            if (isFullscreen) {
                toggleFullscreen()
            } else if (currentPage === "player") {
                if (playerView.menuOpen) {
                    playerView.closeMenus()
                } else {
                    window.returnToSearch()
                }
            } else if (currentPage === "library") {
                // Inside a Library folder: step out one level
                libraryPage.goUp()
            } else if (currentPage === "search" && fluxBrowser && fluxBrowser.active) {
                // Inside a folder: step out one level (closes the browser at the top)
                fluxBrowser.goUp()
            }
        }
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
        if (fluxSync) fluxSync.watching = (currentPage === "player")
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
        target: customTitleBar

        function onMenuOpenChanged() {
            if (!customTitleBar.menuOpen && currentPage === "player") stage.forceActiveFocus()
        }
    }

    Connections {
        target: fluxTeleparty

        function onStateChanged() {
            if (!fluxTeleparty.active && fluxUser) {
                fluxUser.clearPartyMedia()
            }
        }

        function onJoinedSession() {
            // Offline playback is unavailable during a Teleparty: leave the Library page
            // and stop any offline file that might be loaded
            if (window.pageBeforePlayer === "library") window.pageBeforePlayer = "search"
            if (window.currentPage === "library") window.currentPage = "search"

            if (fluxPlayer && fluxPlayer.url.length > 0) {
                if (fluxSync && fluxSync.isStreamUrl(fluxPlayer.url)) {
                    // Host already watching (or paused on) a stream: make it the party's current video
                    if (fluxTeleparty.isHost && fluxPlayer.state !== "Idle" && fluxPlayer.state !== "Stopped") {
                        fluxSync.adoptCurrent(fluxPlayer.url, window.currentPlayingTitle)
                    }
                } else {
                    fluxPlayer.stop()
                    if (window.currentPage === "player") window.currentPage = "search"
                }
            }
        }
    }

    Connections {
        target: fluxSync

        function onRemoteOpenRequested(url, title, startMs) {
            window.openFromParty(url, title, startMs)
        }

        function onPartyMediaAvailable(url, title, positionMs, durationMs) {
            if (fluxUser) {
                fluxUser.notePartyMedia(url, title, positionMs, durationMs)
            }
        }

        function onActivity(text) {
            if (window.currentPage === "player") {
                playerView.showOsd(text, -1)
            }
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

        // Library: downloaded media, playable offline
        LibraryPage {
            id: libraryPage

            anchors.fill: parent
            topInset: customTitleBar.height
            active: currentPage === "library"
            opacity: currentPage === "library" ? 1.0 : 0.0
            visible: opacity > 0.0

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            onPlayRequested: function(url, title) {
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
            solid: searchPage.navSolid || currentPage === "library"
            section: currentPage === "library" ? "library" : (currentPage === "search" ? "home" : "")
            overlayMode: currentPage === "player"
            shown: !window.isFullscreen && (currentPage !== "player" || playerView.showControls || menuOpen)
            offlineLocked: window.inTeleparty
            showNowPlaying: !!fluxPlayer
                            && (fluxPlayer.isPlaying
                                || (window.inTeleparty
                                    && ((fluxPlayer.url.length > 0
                                         && fluxPlayer.state !== "Idle" && fluxPlayer.state !== "Stopped")
                                        || (fluxSync && fluxSync.hasPartyMedia))))
                            && currentPage !== "player"

            onNowPlayingClicked: {
                if (window.inTeleparty && fluxSync && fluxSync.hasPartyMedia
                        && (!fluxPlayer || !window.sameMediaUrl(fluxPlayer.url, fluxSync.currentUrl)
                            || fluxPlayer.state === "Idle" || fluxPlayer.state === "Stopped")) {
                    fluxSync.joinPartyPlayback()
                    return
                }
                currentPage = "player"
                if (fluxPlayer && fluxPlayer.isPaused) fluxPlayer.resume()
            }
            onHomeClicked: window.returnToHome()
            onLibraryClicked: window.showLibrary()
        }

        // Bottom-right dock: downloads + settings (browsing pages only)
        Row {
            id: dock

            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 22
            anchors.bottomMargin: 22
            spacing: 10
            z: 90
            visible: (currentPage === "search" || currentPage === "library") && !window.isFullscreen

            DockButton {
                iconName: "download"
                tip: "Downloads"
                badge: fluxDownloads ? fluxDownloads.activeCount : 0
                active: downloadsPanel.opened
                onClicked: downloadsPanel.opened ? downloadsPanel.close() : downloadsPanel.open()
            }

            DockButton {
                iconName: "settings"
                tip: "Settings"
                active: settingsPanel.opened
                onClicked: settingsPanel.open()
            }
        }
    }

    // Settings dialog and download manager
    SettingsPanel {
        id: settingsPanel
    }

    DownloadsPanel {
        id: downloadsPanel

        onLibraryRequested: {
            downloadsPanel.close()
            window.showLibrary()
        }
    }


    // Frameless Window Edge Resize Handles
    WindowResizeBorders {
        window: window
    }
}
