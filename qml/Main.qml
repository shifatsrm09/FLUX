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
    property bool wasMaximizedBeforeFullscreen: false

    property string currentPage: "search" // "search" | "library" | "torrent" | "player"
    property string pageBeforePlayer: "search" // where Back from the player returns to
    property string currentPlayingTitle: ""
    property string currentSourceUrl: ""
    property string cacheSourceUrl: ""
    property string cacheSourceTitle: ""
    property var localMatchCandidates: []
    property int localMatchCandidateIndex: 0
    property int localMatchDurationMs: 0
    property string localMatchTitle: ""
    property bool localAutoMatchActive: false
    readonly property bool inTeleparty: !!fluxTeleparty && fluxTeleparty.active
    property bool partyServerMismatch: false
    property string partyMismatchServerName: ""
    property string partyMismatchTitle: ""
    property string partyMismatchUrl: ""

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
        if (!isFullscreen) {
            wasMaximizedBeforeFullscreen = (window.visibility === Window.Maximized)
            isFullscreen = true
            window.showFullScreen()
        } else {
            isFullscreen = false
            if (wasMaximizedBeforeFullscreen) {
                window.showMaximized()
            } else {
                window.showNormal()
            }
        }
    }

    onVisibilityChanged: {
        if (window.visibility === Window.FullScreen) {
            isFullscreen = true
        } else if (isFullscreen) {
            isFullscreen = false
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
        // Teleparty: if attempting to play local media while in a room, route to local teleparty playback
        if (window.inTeleparty && fluxSync && !fluxSync.isStreamUrl(sourceUrl) && !fluxSync.isStreamUrl(directUrl)) {
            window.playLibraryInTeleparty(playbackUrl || sourceUrl, title)
            return
        }

        window.partyServerMismatch = false
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

    function playLibraryInTeleparty(url, title) {
        if (!inTeleparty || !fluxSync || !url || url.length === 0) return
        if (window.partyServerMismatch) {
            window.partyServerMismatch = false
            if (fluxSync) {
                fluxSync.setLocalPlaybackOverride(url)
            }
            window.playMedia(url, title || window.partyMismatchTitle)
            return
        }
        if (fluxSync.localMode && fluxSync.hasPartyMedia) {
            if (fluxSync.localVerificationState === "WaitingForLocalSelection"
                    || fluxSync.localVerificationState === "IncompatibleMedia"
                    || fluxSync.localVerificationState === "UnresolvedMetadata"
                    || fluxSync.localVerificationState === "PlaybackError") {
                if (fluxSync.selectGuestLocalMedia(url, title, 0, false)) {
                    return
                }
            } else if (window.sameMediaUrl(fluxSync.currentUrl, url)) {
                if (currentPage !== "player") pageBeforePlayer = currentPage
                currentPage = "player"
                return
            }
        }
        // Any participant can choose the next shared local video. Matching copies
        // are resolved by the other room members after the new declaration arrives.
        fluxSync.broadcastLocalOpen(url, title)
    }

    function findLocalTelepartyCopy(title, durationMs) {
        if (!inTeleparty || !fluxSync || !fluxDownloads) return

        localAutoMatchActive = true
        localMatchTitle = title || fluxSync.currentTitle
        localMatchDurationMs = durationMs || 0
        localMatchCandidateIndex = 0
        localMatchCandidates = []

        var tag = (fluxSync.localEpisodeTag || "").toLowerCase()
        var results = fluxDownloads.offlineSearch(localMatchTitle)
        var expectedEpisode = tag.match(/^s(\d{1,2})e(\d{1,3})$/)
        var tagged = []
        var videos = []
        for (var i = 0; i < results.length; ++i) {
            var item = results[i]
            if (!item || item.kind !== "video" || !item.url) continue
            videos.push(item)
            if (expectedEpisode) {
                var fileIdentity = (item.name || "") + " " + (item.path || "")
                var fileEpisode = fileIdentity.match(/s(\d{1,2})[ ._-]*e(\d{1,3})/i)
                if (!fileEpisode) fileEpisode = fileIdentity.match(/(\d{1,2})x(\d{1,3})/i)
                if (fileEpisode && parseInt(fileEpisode[1], 10) === parseInt(expectedEpisode[1], 10)
                        && parseInt(fileEpisode[2], 10) === parseInt(expectedEpisode[2], 10)) {
                    tagged.push(item)
                }
            }
        }

        // A renamed copy may not include the show's title in its path. Try its episode
        // tag as a second discovery key, then let the content verifier decide the match.
        if (videos.length === 0 && tag.length > 0) {
            results = fluxDownloads.offlineSearch(tag)
            for (var j = 0; j < results.length; ++j) {
                var fallback = results[j]
                if (fallback && fallback.kind === "video" && fallback.url) videos.push(fallback)
            }
        }

        // Episode tags keep a broad series title from causing dozens of unnecessary
        // fingerprint checks. Fall back to title matches for unusual episode naming.
        localMatchCandidates = (tagged.length > 0 ? tagged : videos).slice(0, 40)
        tryNextLocalTelepartyCopy()
    }

    function tryNextLocalTelepartyCopy() {
        if (!localAutoMatchActive || !fluxSync) return
        while (localMatchCandidateIndex < localMatchCandidates.length) {
            var candidate = localMatchCandidates[localMatchCandidateIndex++]
            if (window.partyServerMismatch) {
                window.partyServerMismatch = false
                if (fluxSync) fluxSync.setLocalPlaybackOverride(candidate.url)
                window.playMedia(candidate.url, localMatchTitle)
                localAutoMatchActive = false
                return
            }
            if (fluxSync.selectGuestLocalMedia(candidate.url, localMatchTitle, localMatchDurationMs, true)) {
                return // Inspection runs asynchronously; failures trigger onLocalCandidateCheckFailed quietly.
            }
        }
        localAutoMatchActive = false // No match: leave the guest on the red missing-file pill.
        if (window.partyServerMismatch) {
            window.showLibrary()
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

        // Check server reachability and local/cached copies
        var s = fluxUser ? fluxUser.serverForUrl(url) : 0
        var isServerSupported = (s === 0) || (fluxUser && fluxUser.isServerAvailableForUrl(url))
        var downloadedUrl = fluxDownloads ? fluxDownloads.downloadedFileForUrl(url, title) : ""
        var cachedUrl = (!downloadedUrl && fluxCache) ? fluxCache.cachedFileForUrl(url) : ""

        if (!isServerSupported && !downloadedUrl && !cachedUrl) {
            if (fluxPlayer) fluxPlayer.stop()
            window.partyServerMismatch = true
            window.partyMismatchServerName = fluxUser ? fluxUser.serverNameForUrl(url) : "Unknown Server"
            window.partyMismatchTitle = title || "Party Stream"
            window.partyMismatchUrl = url

            // DO NOT open the player on server mismatch!
            if (currentPage === "player") {
                currentPage = (pageBeforePlayer && pageBeforePlayer !== "player") ? pageBeforePlayer : "search"
            }

            if (fluxSync) {
                fluxSync.reportServerMismatch(window.partyMismatchServerName, window.partyMismatchTitle)
            }
            return
        }

        window.partyServerMismatch = false
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
                    if (window.inTeleparty && fluxSync) {
                        if (fluxSync.partyPlaying && fluxPlayer.isPaused) {
                            fluxPlayer.remoteResume()
                        } else if (!fluxSync.partyPlaying && fluxPlayer.isPlaying) {
                            fluxPlayer.remotePause()
                        }
                    } else {
                        if (fluxPlayer.isPaused) fluxPlayer.remoteResume()
                    }
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
        window.partyServerMismatch = false
        if (fluxPlayer && !window.inTeleparty) {
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
        window.partyServerMismatch = false
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
        window.partyServerMismatch = false
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
        if (fluxPlayer && !window.inTeleparty) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = (pageBeforePlayer === "library") ? "library" : ((pageBeforePlayer === "torrent") ? "torrent" : "search")
    }

    function showTorrent() {
        window.partyServerMismatch = false
        if (fluxPlayer && !window.inTeleparty) {
            fluxPlayer.pause()
        }
        if (isFullscreen) {
            toggleFullscreen()
        }
        currentPage = "torrent"
    }

    function showLibrary(rel) {
        window.partyServerMismatch = false
        if (fluxPlayer && !window.inTeleparty) {
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
            if (networkNoticeDialog.opened) {
                networkNoticeDialog.close()
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
    property bool networkNoticeShown: false

    function checkNetworkNotice() {
        if (networkNoticeShown || !startupComplete) return
        if (fluxUser && fluxUser.bothServersDown) {
            networkNoticeShown = true
            networkNoticeDialog.open()
        }
    }

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
        window.checkNetworkNotice()
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
        target: fluxUser

        function onServerAvailabilityChanged() {
            window.checkNetworkNotice()
            if (window.partyServerMismatch && window.partyMismatchUrl) {
                if (fluxUser && fluxUser.isServerAvailableForUrl(window.partyMismatchUrl)) {
                    var recoveredUrl = window.partyMismatchUrl
                    var recoveredTitle = window.partyMismatchTitle
                    window.partyServerMismatch = false
                    window.openFromParty(recoveredUrl, recoveredTitle, 0)
                }
            }
        }
    }

    Connections {
        target: fluxTeleparty

        function onStateChanged() {
            if (!fluxTeleparty.active) {
                window.localAutoMatchActive = false
                window.partyServerMismatch = false
                if (fluxUser) fluxUser.clearPartyMedia()
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

        function onCurrentMediaChanged() {
            if (fluxSync && fluxSync.hasPartyMedia) {
                var url = fluxSync.currentUrl
                var s = fluxUser ? fluxUser.serverForUrl(url) : 0
                var supported = (s === 0) || (fluxUser && fluxUser.isServerAvailableForUrl(url))
                if (!supported) {
                    var downloadedUrl = fluxDownloads ? fluxDownloads.downloadedFileForUrl(url, fluxSync.currentTitle) : ""
                    var cachedUrl = (!downloadedUrl && fluxCache) ? fluxCache.cachedFileForUrl(url) : ""
                    if (!downloadedUrl && !cachedUrl) {
                        window.partyServerMismatch = true
                        window.partyMismatchServerName = fluxUser ? fluxUser.serverNameForUrl(url) : "Unknown Server"
                        window.partyMismatchTitle = fluxSync.currentTitle
                        window.partyMismatchUrl = url
                        if (fluxPlayer) fluxPlayer.stop()
                        if (currentPage === "player") {
                            currentPage = (pageBeforePlayer && pageBeforePlayer !== "player") ? pageBeforePlayer : "search"
                        }
                        return
                    }
                }
                window.partyServerMismatch = false
            }
        }

        function onRemoteOpenRequested(url, title, startMs) {
            window.openFromParty(url, title, startMs)
        }

        function onPartyMediaAvailable(url, title, positionMs, durationMs) {
            if (fluxUser && !url.startsWith("local://")) {
                fluxUser.notePartyMedia(url, title, positionMs, durationMs)
            }
        }

        function onLocalMediaPromptRequested(expectedTitle, expectedDurationMs, expectedContainer) {
            window.findLocalTelepartyCopy(expectedTitle, expectedDurationMs)
        }

        function onLocalCandidateCheckFailed(reason) {
            if (window.localAutoMatchActive) {
                Qt.callLater(window.tryNextLocalTelepartyCopy)
            }
        }

        function onLocalVerificationChanged() {
            if (!fluxSync) return
            var state = fluxSync.localVerificationState
            if (state === "VerifiedCompatible" || state === "OpeningMedia"
                    || state === "BufferingMedia" || state === "ReadyToSync") {
                window.localAutoMatchActive = false
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
            case Qt.Key_V:
                if (!event.isAutoRepeat) playerView.cycleSubtitles()
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
                } else if (window.isFullscreen) {
                    window.toggleFullscreen()
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
            telepartyActive: window.inTeleparty
            opacity: currentPage === "library" ? 1.0 : 0.0
            visible: opacity > 0.0

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            onPlayRequested: function(url, title) {
                if (window.inTeleparty) {
                    window.playLibraryInTeleparty(url, title)
                } else {
                    window.playMedia(url, title)
                }
            }
        }

        // Torrent: stream from magnet links or .torrent files
        TorrentPage {
            id: torrentPage

            anchors.fill: parent
            topInset: customTitleBar.height
            active: currentPage === "torrent"
            opacity: currentPage === "torrent" ? 1.0 : 0.0
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
                if (window.inTeleparty && fluxSync && fluxSync.localMode
                        && !fluxSync.isStreamUrl(url)) {
                    fluxSync.broadcastLocalOpen(url, name)
                    return
                }
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
            solid: searchPage.navSolid || currentPage === "library" || currentPage === "torrent"
            section: currentPage === "library" ? "library"
                                                : (currentPage === "torrent" ? "torrent"
                                                : (currentPage === "search" ? (searchPage.bookmarksOpen ? "bookmarks" : (searchPage.showSearch ? "search" : "home")) : ""))
            overlayMode: currentPage === "player"
            shown: !window.isFullscreen && (currentPage !== "player" || playerView.showControls || menuOpen)
            offlineLocked: false
            nowPlayingNeedsMedia: (window.inTeleparty && !!fluxSync && fluxSync.localMode
                                  && !window.localAutoMatchActive
                                  && (fluxSync.localVerificationState === "WaitingForLocalSelection"
                                      || fluxSync.localVerificationState === "IncompatibleMedia"
                                      || fluxSync.localVerificationState === "UnresolvedMetadata"
                                      || fluxSync.localVerificationState === "PlaybackError"))
                                  || window.partyServerMismatch
            nowPlayingWarning: {
                if (window.partyServerMismatch) {
                    var sName = (window.partyMismatchServerName || "").toLowerCase()
                    var sTag = "ICC/DHAKA FLIX"
                    if (sName.indexOf("icc") !== -1) sTag = "ICC"
                    else if (sName.indexOf("dhaka") !== -1) sTag = "DHAKA FLIX"
                    return "Now Playing: " + sTag + " Content Unsupported on Your Network"
                }
                return fluxSync
                    ? ("A participant is playing \"" + fluxSync.currentTitle + "\". Add this file to your Library and open it to join.")
                    : "Add the playing file to your Library to join playback."
            }
            showNowPlaying: ((fluxPlayer
                             && (fluxPlayer.isPlaying
                                 || (window.inTeleparty
                                     && ((fluxPlayer.url.length > 0
                                          && fluxPlayer.state !== "Idle" && fluxPlayer.state !== "Stopped")
                                         || (fluxSync && fluxSync.hasPartyMedia)))))
                             || window.partyServerMismatch)
                            && currentPage !== "player"

            onNowPlayingClicked: {
                if (window.partyServerMismatch) {
                    return
                }
                if (window.inTeleparty && fluxSync && fluxSync.localMode
                        && (fluxSync.localVerificationState === "WaitingForLocalSelection"
                            || fluxSync.localVerificationState === "IncompatibleMedia")) {
                    window.showLibrary()
                    return
                }
                if (window.inTeleparty && fluxSync && fluxSync.hasPartyMedia
                        && (!fluxPlayer || !window.sameMediaUrl(fluxPlayer.url, fluxSync.currentUrl)
                            || fluxPlayer.state === "Idle" || fluxPlayer.state === "Stopped")) {
                    fluxSync.joinPartyPlayback()
                    return
                }
                currentPage = "player"
            }
            onHomeClicked: window.returnToHome()
            onSearchClicked: window.showSearch()
            onBookmarksClicked: window.showBookmarks()
            onLibraryClicked: window.showLibrary()

            onTorrentClicked: window.showTorrent()
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
            visible: (currentPage === "search" || currentPage === "library" || currentPage === "torrent") && !window.isFullscreen

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

    NetworkNoticeDialog {
        id: networkNoticeDialog
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
