import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"
import "components/Theme.js" as Theme
import "components/MediaFormatter.js" as Formatter

ApplicationWindow {
    id: window

    visible: false
    // Restore the last window size (clamped to the minimum); position is left to the OS
    width: fluxUser ? Math.max(960, fluxUser.intValue("window/width", 1280)) : 1280
    height: fluxUser ? Math.max(600, fluxUser.intValue("window/height", 800)) : 800
    minimumWidth: 960
    minimumHeight: 600
    title: "FLUX"
    color: Theme.bg
    font.family: Theme.fontFamily
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinMaxButtonsHint

    property bool startupComplete: false
    property bool isFullscreen: false
    property string currentPage: "search" // "search" | "library" | "player"
    property string pageBeforePlayer: "search" // where Back from the player returns to
    property string currentPlayingTitle: ""
    property string currentSourceUrl: ""
    property string cacheSourceUrl: ""
    property string cacheSourceTitle: ""
    readonly property bool inTeleparty: !!fluxTeleparty && fluxTeleparty.active
    onInTelepartyChanged: {
        if (inTeleparty && currentPage === "library") {
            window.returnToSearch()
        }
    }

    // Round floating button for the bottom-right dock (optional badge = active downloads)
    component DockButton: Rectangle {
        id: dockBtn

        property string iconName: "settings"
        property string tip: ""
        property int badge: 0
        property bool active: false
        property bool attention: false
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
            visible: dockBtn.attention
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 7
            anchors.topMargin: 7
            width: 9
            height: 9
            radius: 5
            color: "#FF4D5E"
            border.width: 1
            border.color: Theme.bg
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
            if (decodeURIComponent(a) === decodeURIComponent(b)) return true
        } catch (e) {}

        if (a.indexOf("player.php") !== -1 && b.indexOf("player.php") !== -1) {
            var ma = a.match(/[?&]play=(\d+)/)
            var mb = b.match(/[?&]play=(\d+)/)
            if (ma && mb && ma[1] === mb[1]) {
                return true
            }
        }

        if (fluxDownloads) {
            try {
                var dlA = fluxDownloads.downloadedFileForUrl(a)
                if (dlA && (dlA === b || decodeURIComponent(dlA) === decodeURIComponent(b))) return true
                var dlB = fluxDownloads.downloadedFileForUrl(b)
                if (dlB && (dlB === a || decodeURIComponent(dlB) === decodeURIComponent(a))) return true
            } catch (e) {}
        }

        if (fluxCache) {
            try {
                var caA = fluxCache.cachedFileForUrl(a)
                if (caA && (caA === b || decodeURIComponent(caA) === decodeURIComponent(b))) return true
                var caB = fluxCache.cachedFileForUrl(b)
                if (caB && (caB === a || decodeURIComponent(caB) === decodeURIComponent(a))) return true
            } catch (e) {}
        }

        function extractCleanFileName(u) {
            if (!u) return ""
            var s = ""
            try {
                s = decodeURIComponent(u)
            } catch (e) {
                s = u
            }
            s = s.split("?")[0].split("#")[0]
            var slash = Math.max(s.lastIndexOf("/"), s.lastIndexOf("\\"))
            return (slash >= 0 ? s.substring(slash + 1) : s).toLowerCase().trim()
        }

        var fnA = extractCleanFileName(a)
        var fnB = extractCleanFileName(b)
        if (fnA.length > 0 && fnA === fnB) {
            var videoExts = [".mkv", ".mp4", ".avi", ".mov", ".wmv", ".m4v", ".ts", ".m2ts", ".webm", ".flv", ".mpg", ".mpeg"]
            for (var i = 0; i < videoExts.length; ++i) {
                if (fnA.endsWith(videoExts[i])) return true
            }
        }

        return false
    }

    property string currentAltUrl: ""

    function playMedia(url, title, altUrl) {
        currentAltUrl = (altUrl && altUrl.length > 0) ? altUrl : ""
        if (url && url.indexOf("player.php") !== -1 && fluxSearch) {
            fluxSearch.resolvePlayUrl(url, function(directUrl) {
                if (directUrl && directUrl.length > 0) {
                    playResolvedMedia(url, directUrl, title)
                } else if (currentAltUrl.length > 0) {
                    var alt = currentAltUrl
                    currentAltUrl = ""
                    console.log("[Main] Failed to resolve play URL, auto-falling back to altUrl:", alt)
                    if (playerView && playerView.showOsd) {
                        playerView.showOsd("Primary server unavailable. Auto-switching...", -1)
                    }
                    playMedia(alt, title, "")
                } else {
                    playResolvedMedia(url, url, title)
                }
            })
            return
        }
        playResolvedMedia(url, url, title)
    }

    function playResolvedMedia(sourceUrl, directUrl, title) {
        var playbackUrl = directUrl
        // Teleparty is for direct streams only. Offline Library playback stays outside the room.
        if (window.inTeleparty && fluxSync && !fluxSync.isStreamUrl(sourceUrl) && !fluxSync.isStreamUrl(directUrl)) {
            return
        }

        window.currentSourceUrl = sourceUrl

        var downloadedUrl = fluxDownloads ? fluxDownloads.downloadedFileForUrl(sourceUrl, title) : ""
        if (!downloadedUrl && directUrl && directUrl !== sourceUrl && fluxDownloads) {
            downloadedUrl = fluxDownloads.downloadedFileForUrl(directUrl, title)
        }
        var cachedUrl = (!downloadedUrl && fluxCache) ? fluxCache.cachedFileForUrl(sourceUrl) : ""
        if (!downloadedUrl && !cachedUrl && directUrl && directUrl !== sourceUrl && fluxCache) {
            cachedUrl = fluxCache.cachedFileForUrl(directUrl)
        }

        if (downloadedUrl) {
            playbackUrl = downloadedUrl
        } else if (cachedUrl) {
            playbackUrl = cachedUrl
        } else if (!window.inTeleparty && fluxSync && fluxSync.isStreamUrl(sourceUrl)) {
            cacheSourceUrl = sourceUrl
            cacheSourceTitle = title
            cacheAfterPlaybackTimer.restart()
        }

        // Clicking the video the party is already watching (e.g. from Continue Watching
        // after joining late) joins at the party's live timestamp instead of restarting from 0
        if (window.inTeleparty && fluxSync && fluxSync.hasPartyMedia
                && (window.sameMediaUrl(fluxSync.currentUrl, sourceUrl) || (directUrl && window.sameMediaUrl(fluxSync.currentUrl, directUrl)))) {
            fluxSync.joinPartyPlayback()
            return
        }

        currentPlayingTitle = title
        if (currentPage !== "player") pageBeforePlayer = currentPage
        currentPage = "player"

        // Read the saved position BEFORE noteStart() touches the history entry.
        var resumeMs = fluxUser ? fluxUser.resumePositionFor(sourceUrl) : 0
        if (resumeMs <= 0 && directUrl && directUrl !== sourceUrl && fluxUser) {
            resumeMs = fluxUser.resumePositionFor(directUrl)
        }
        var resumeProgress = (resumeMs > 0 && fluxUser) ? fluxUser.progressFor(sourceUrl) : ({})
        if ((!resumeProgress || !resumeProgress.fraction) && resumeMs > 0 && directUrl && directUrl !== sourceUrl && fluxUser) {
            resumeProgress = fluxUser.progressFor(directUrl)
        }
        var resumeFraction = (resumeProgress && resumeProgress.fraction > 0) ? resumeProgress.fraction : -1
        if (fluxUser) fluxUser.noteStart(sourceUrl, title, playbackUrl)

        // Look up the next episode in the background (for the "Up next" card)
        if (fluxBrowser) fluxBrowser.findNext(sourceUrl)

        playerView.offerResume(resumeMs)

        if (playerView && playerView.armTimeoutWatchdog) {
            playerView.armTimeoutWatchdog()
        }

        if (fluxPlayer) {
            if (resumeMs > 0) {
                fluxPlayer.playFrom(playbackUrl, resumeMs, resumeFraction)
            } else {
                fluxPlayer.play(playbackUrl)
            }
        }

        if (window.inTeleparty && fluxSync) {
            var partyUrl = (playbackUrl && fluxSync.isStreamUrl(playbackUrl))
                ? playbackUrl
                : ((directUrl && fluxSync.isStreamUrl(directUrl)) ? directUrl : sourceUrl)
            fluxSync.broadcastOpen(partyUrl, title, resumeMs)
        }
    }

    Timer {
        id: cacheAfterPlaybackTimer
        interval: 30000
        repeat: false
        onTriggered: {
            if (!window.inTeleparty && fluxPlayer && fluxPlayer.isPlaying
                    && window.sameMediaUrl(fluxPlayer.url, window.cacheSourceUrl)
                    && fluxDownloads && !fluxDownloads.downloadedFileForUrl(window.cacheSourceUrl, window.cacheSourceTitle)
                    && fluxCache) {
                fluxCache.cacheMedia(window.cacheSourceUrl, window.cacheSourceTitle)
            }
        }
    }

    // Open a stream requested by another Teleparty member (never broadcast back)
    function openFromParty(url, title, startMs) {
        customTitleBar.closeMenu()
        if (settingsPanel.opened) settingsPanel.dismiss()
        if (downloadsPanel.opened) downloadsPanel.close()

        window.currentSourceUrl = url
        currentPlayingTitle = title
        if (currentPage !== "player") {
            pageBeforePlayer = currentPage
        }
        currentPage = "player"

        if (fluxBrowser) fluxBrowser.findNext(url)

        playerView.offerResume(0)

        function startPlayback(targetUrl) {
            var finalUrl = targetUrl
            var downloadedUrl = fluxDownloads ? fluxDownloads.downloadedFileForUrl(targetUrl, title) : ""
            if (!downloadedUrl && url !== targetUrl && fluxDownloads) {
                downloadedUrl = fluxDownloads.downloadedFileForUrl(url, title)
            }
            var cachedUrl = (!downloadedUrl && fluxCache) ? fluxCache.cachedFileForUrl(targetUrl) : ""
            if (!downloadedUrl && !cachedUrl && url !== targetUrl && fluxCache) {
                cachedUrl = fluxCache.cachedFileForUrl(url)
            }
            if (downloadedUrl) {
                finalUrl = downloadedUrl
            } else if (cachedUrl) {
                finalUrl = cachedUrl
            }

            if (fluxUser) fluxUser.noteStart(url, title, finalUrl)

            if (fluxPlayer) {
                if (window.sameMediaUrl(fluxPlayer.url, finalUrl)
                        && (fluxPlayer.state === "Paused" || fluxPlayer.state === "Playing")) {
                    if (startMs > 0) fluxPlayer.remoteSeekTo(startMs)
                    if (fluxPlayer.isPaused) fluxPlayer.remoteResume()
                } else if (startMs > 0) {
                    fluxPlayer.playFrom(finalUrl, startMs)
                } else {
                    fluxPlayer.play(finalUrl)
                }
            }
        }

        if (url && url.indexOf("player.php") !== -1 && fluxSearch) {
            fluxSearch.resolvePlayUrl(url, function(directUrl) {
                startPlayback((directUrl && directUrl.length > 0) ? directUrl : url)
            })
            return
        }

        startPlayback(url)
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
        searchPage.bookmarksOpen = false
        searchPage.searchOpen = false
    }

    function showSearch() {
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = "search"
        if (fluxBrowser) {
            fluxBrowser.close()
        }
        searchPage.bookmarksOpen = false
        searchPage.searchOpen = true
    }

    // Bookmarks: the saved files / folders, shown with the same cards as search results
    function showBookmarks() {
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = "search"
        if (fluxBrowser) {
            fluxBrowser.close()     // show the bookmarks list itself, not a folder opened earlier
        }
        searchPage.searchOpen = false
        searchPage.bookmarksOpen = true
    }

    // Back from the player: to the page it was started from (browse or Library)
    function returnToSearch() {
        if (fluxPlayer) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = (pageBeforePlayer === "library") ? "library" : "search"
    }

    function showLibrary(rel) {
        if (window.inTeleparty) {
            return
        }
        if (fluxPlayer) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = "library"
        if (rel !== undefined && rel !== null && rel.length > 0) {
            libraryPage.clearSearch()
            libraryPage.rel = rel
            libraryPage.refreshTick++
        }
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

            if (currentPage === "player" && playerView.menuOpen) {
                playerView.closeMenus()
                return
            }

            if (isFullscreen) {
                toggleFullscreen()
            } else if (currentPage === "player") {
                window.returnToSearch()
            } else if (currentPage === "library") {
                // Inside a Library folder: step out one level
                libraryPage.goUp()
            } else if (currentPage === "search" && fluxBrowser && fluxBrowser.active) {
                // Inside a folder: step out one level (closes the browser at the top)
                fluxBrowser.goUp()
            } else if (currentPage === "search" && searchPage.bookmarksOpen) {
                // On the Bookmarks page: back to where we were
                searchPage.bookmarksOpen = false
            }
        }
    }

    Shortcut {
        sequence: "F11"
        onActivated: toggleFullscreen()
    }

    // ---- Startup transition & keyboard focus management ----------------------------
    function completeStartup() {
        if (window.startupComplete) return
        window.startupComplete = true
        if (fluxUser && fluxUser.boolValue("window/maximized", false)) {
            window.showMaximized()
        } else {
            window.showNormal()
        }
        window.raise()
        window.requestActivate()
        startupPopup.visible = false
    }

    onClosing: function(close) {
        if (!fluxUser || !window.startupComplete) return
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
            if (fluxPlayer && fluxPlayer.url.length > 0) {
                var streamUrl = (fluxSync && fluxSync.isStreamUrl(fluxPlayer.url))
                    ? fluxPlayer.url
                    : ((window.currentSourceUrl && fluxSync && fluxSync.isStreamUrl(window.currentSourceUrl)) ? window.currentSourceUrl : "")

                if (streamUrl.length > 0) {
                    // Host already watching (or paused on) a direct stream: make it the party's current video
                    if (fluxTeleparty.isHost && fluxPlayer.state !== "Idle" && fluxPlayer.state !== "Stopped") {
                        fluxSync.adoptCurrent(streamUrl, window.currentPlayingTitle)
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
            if (fluxUser && !url.startsWith("local://")) {
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
            case Qt.Key_Escape:
                if (playerView.episodesPanelOpen) {
                    playerView.episodesPanelOpen = false
                    event.accepted = true
                }
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

            onPlayMediaRequested: function(url, title, altUrl) {
                window.playMedia(url, title, altUrl)
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
            section: currentPage === "library" ? "library"
                                                : (currentPage === "search" ? (searchPage.bookmarksOpen ? "bookmarks" : (searchPage.showSearch ? "search" : "home")) : "")
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
            onSearchClicked: window.showSearch()
            onBookmarksClicked: window.showBookmarks()
            onLibraryClicked: if (!window.inTeleparty) window.showLibrary()
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
                attention: !!fluxDownloads && fluxDownloads.history.some(function(entry) {
                    return entry.status === "interrupted" || entry.status === "failed"
                })
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

        onLibraryRequested: function(rel) {
            if (!window.inTeleparty) {
                downloadsPanel.close()
                window.showLibrary(rel)
            }
        }

        onPlayRequested: function(url, title) {
            downloadsPanel.close()
            window.playMedia(url, title, "")
        }
    }


    // Frameless Window Edge Resize Handles
    WindowResizeBorders {
        window: window
    }

    // Discord-style startup updater popup window
    StartupPopup {
        id: startupPopup
        onFinished: window.completeStartup()
    }
}
