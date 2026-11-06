import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "components/Theme.js" as Theme

Item {
    id: root

    property int topInset: 48
    property bool active: false
    property bool autoPlay: true
    property bool autoPlayTriggered: false
    property string activeTab: "tork" // "tork" | "1337x" | "direct"
    property bool activeOnly: false   // Torrents with > 50 seeders

    // Category filter checkboxes (Movies, TV, Anime)
    property bool catMovies: true
    property bool catTV: true
    property bool catAnime: true

    property string toastMessage: ""
    property bool showToast: false

    signal playRequested(string url, string title)

    Connections {
        target: fluxTorrent
        function onTorrentReadyToPlay(url, title) {
            if (root.autoPlay && !root.autoPlayTriggered && url && url.length > 0) {
                root.autoPlayTriggered = true
                root.playRequested(url, title)
            }
        }
        function onMagnetCopied(magnet) {
            root.toastMessage = "Magnet link copied to clipboard"
            root.showToast = true
            toastTimer.restart()
        }
        function onDownloadStarted(title) {
            root.toastMessage = "Download queued: " + title
            root.showToast = true
            toastTimer.restart()
        }
        function onDownloadRequested(url, title) {
            if (typeof fluxDownloads !== "undefined" && fluxDownloads && url && url.length > 0) {
                fluxDownloads.downloadFile(url, title)
                root.toastMessage = "Download started in Downloads panel"
                root.showToast = true
                toastTimer.restart()
            }
        }
    }

    Timer {
        id: toastTimer
        interval: 2200
        onTriggered: root.showToast = false
    }

    // Full-page drag and drop for .torrent files
    DropArea {
        id: fullPageDropArea
        anchors.fill: parent
        onEntered: function(drag) {
            if (drag.hasUrls) {
                for (var i = 0; i < drag.urls.length; i++) {
                    var u = drag.urls[i].toString()
                    if (u.toLowerCase().endsWith(".torrent")) {
                        drag.acceptProposedAction()
                        return
                    }
                }
            }
        }
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                var urlStr = drop.urls[0].toString()
                root.handleInput(urlStr)
                drop.acceptProposedAction()
            }
        }
    }

    function handleInput(val) {
        var trimmed = (val || "").trim()
        if (trimmed.length > 0 && fluxTorrent) {
            root.autoPlayTriggered = false
            fluxTorrent.startTorrent(trimmed)
        }
    }

    function executeSearch(query) {
        var q = (query !== undefined ? query : searchInput.text).trim()
        if (!q.length || !fluxTorrent) return

        var cat = "all"
        if (root.catMovies && !root.catTV && !root.catAnime) cat = "movies"
        else if (!root.catMovies && root.catTV && !root.catAnime) cat = "tv"
        else if (!root.catMovies && !root.catTV && root.catAnime) cat = "anime"

        if (root.activeTab === "tork") {
            fluxTorrent.searchTork(q, cat)
        } else if (root.activeTab === "1337x") {
            fluxTorrent.search1337x(q, cat)
        }
    }

    function getTypeBadge(name, category) {
        var lower = (name || "").toLowerCase()
        var cat = (category || "").toLowerCase()

        if (lower.indexOf("2160p") !== -1 || lower.indexOf("4k") !== -1) {
            return { tag: "4K UHD", text: "WEB-DL", fg: "#e9d5ff", border: "#7e22ce", bg: "#231535" }
        }
        if (cat.indexOf("anime") !== -1) {
            return { tag: "ANIME", text: "SERIES", fg: "#fbcfe8", border: "#be185d", bg: "#301323" }
        }
        if (lower.indexOf("s0") !== -1 || lower.indexOf("season") !== -1 || cat.indexOf("tv") !== -1) {
            return { tag: "HD", text: "SEASONS", fg: "#fecdd3", border: "#be123c", bg: "#2f121d" }
        }
        if (lower.indexOf("web-dl") !== -1 || lower.indexOf("webrip") !== -1) {
            return { tag: "HD", text: "WEB-DL", fg: "#a5f3fc", border: "#0e7490", bg: "#10252f" }
        }
        if (lower.indexOf("1080p") !== -1) {
            return { tag: "FHD", text: "1080p", fg: "#bae6fd", border: "#0284c7", bg: "#102334" }
        }
        if (lower.indexOf("720p") !== -1) {
            return { tag: "HD", text: "720p", fg: "#c7d2fe", border: "#4338ca", bg: "#141733" }
        }
        return { tag: "HD", text: "MOVIE", fg: "#99f6e4", border: "#0f766e", bg: "#102827" }
    }

    function getProviderColor(prov) {
        var p = (prov || "").toUpperCase()
        if (p.indexOf("TPB") !== -1) return "#fbbf24"
        if (p.indexOf("YTS") !== -1) return "#34d399"
        if (p.indexOf("RARBG") !== -1) return "#c084fc"
        if (p.indexOf("NYAA") !== -1) return "#f472b6"
        if (p.indexOf("1337X") !== -1) return "#f87171"
        return "#a0aec0"
    }

    function filterList(sourceList) {
        if (!sourceList) return []
        var res = []
        for (var i = 0; i < sourceList.length; i++) {
            var item = sourceList[i]
            var seeds = item.seeds !== undefined ? item.seeds : (item.seeders !== undefined ? item.seeders : 0)

            // ACTIVE toggle filter: > 50 seeders
            if (root.activeOnly && seeds <= 50) {
                continue
            }

            var cat = (item.category || "").toLowerCase()
            var name = (item.name || "").toLowerCase()

            var matchesCat = false
            if (!root.catMovies && !root.catTV && !root.catAnime) {
                matchesCat = true
            } else {
                if (root.catMovies && (cat.indexOf("movie") !== -1 || name.indexOf("bluray") !== -1 || name.indexOf("1080p") !== -1 || cat === "general" || cat === "")) matchesCat = true
                if (root.catTV && (cat.indexOf("tv") !== -1 || name.indexOf("s0") !== -1 || name.indexOf("season") !== -1)) matchesCat = true
                if (root.catAnime && cat.indexOf("anime") !== -1) matchesCat = true
            }

            if (matchesCat) {
                var copy = Object.assign({}, item)
                copy.itemIndex = i
                copy.seedsVal = seeds
                copy.leechesVal = item.leeches !== undefined ? item.leeches : (item.leechers !== undefined ? item.leechers : 0)
                res.push(copy)
            }
        }
        return res
    }

    property var displayedItems: {
        if (!fluxTorrent) return []
        if (root.activeTab === "tork") {
            return filterList(fluxTorrent.torkResults)
        } else if (root.activeTab === "1337x") {
            return filterList(fluxTorrent.searchResults)
        }
        return []
    }

    // Outer Background matching FLUX design system (Library, Home, Bookmarks)
    Rectangle {
        anchors.fill: parent
        color: Theme.bg

        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: root.topInset + 10
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            anchors.bottomMargin: 16
            spacing: 16

            // ---- 1. Top Header Row ----
            RowLayout {
                Layout.fillWidth: true
                spacing: 20

                Text {
                    text: "Torrents"
                    color: Theme.text
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                }

                // Center Segmented Source Tabs [ Tork | 1337x | Direct Magnet ]
                RowLayout {
                    spacing: 0
                    Layout.alignment: Qt.AlignHCenter

                    Rectangle {
                        implicitWidth: torkTabText.implicitWidth + 40
                        height: 38
                        radius: 0
                        color: root.activeTab === "tork" ? Theme.accent : Theme.bgRaised
                        border.width: 1
                        border.color: root.activeTab === "tork" ? Theme.accent : Theme.border

                        Text {
                            id: torkTabText
                            anchors.centerIn: parent
                            text: "Tork"
                            color: root.activeTab === "tork" ? "#ffffff" : Theme.textDim
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeTab = "tork"
                                if (searchInput.text.trim().length > 0) root.executeSearch()
                            }
                        }
                    }

                    Rectangle {
                        implicitWidth: x1337TabText.implicitWidth + 40
                        height: 38
                        radius: 0
                        color: root.activeTab === "1337x" ? Theme.accent : Theme.bgRaised
                        border.width: 1
                        border.color: root.activeTab === "1337x" ? Theme.accent : Theme.border

                        Text {
                            id: x1337TabText
                            anchors.centerIn: parent
                            text: "1337x"
                            color: root.activeTab === "1337x" ? "#ffffff" : Theme.textDim
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.activeTab = "1337x"
                                if (searchInput.text.trim().length > 0) root.executeSearch()
                            }
                        }
                    }

                    Rectangle {
                        implicitWidth: directTabText.implicitWidth + 40
                        height: 38
                        radius: 0
                        color: root.activeTab === "direct" ? Theme.accent : Theme.bgRaised
                        border.width: 1
                        border.color: root.activeTab === "direct" ? Theme.accent : Theme.border

                        Text {
                            id: directTabText
                            anchors.centerIn: parent
                            text: "Direct Magnet"
                            color: root.activeTab === "direct" ? "#ffffff" : Theme.textDim
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeTab = "direct"
                        }
                    }
                }

                Item { Layout.fillWidth: true }
            }

            // ---- 2. Category Checkboxes Row (Scaled & Readable) ----
            RowLayout {
                visible: root.activeTab !== "direct"
                Layout.alignment: Qt.AlignHCenter
                spacing: 40

                // Movies Checkbox
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 20
                        height: 20
                        radius: 3
                        color: root.catMovies ? Theme.surface : Theme.bgRaised
                        border.width: 1
                        border.color: root.catMovies ? Theme.accent : Theme.border

                        Text {
                            visible: root.catMovies
                            anchors.centerIn: parent
                            text: "✓"
                            color: Theme.accent
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.catMovies = !root.catMovies
                        }
                    }
                    Text {
                        text: "Movies"
                        color: root.catMovies ? Theme.text : Theme.textDim
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.catMovies = !root.catMovies
                        }
                    }
                }

                // TV Checkbox
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 20
                        height: 20
                        radius: 3
                        color: root.catTV ? Theme.surface : Theme.bgRaised
                        border.width: 1
                        border.color: root.catTV ? Theme.accent : Theme.border

                        Text {
                            visible: root.catTV
                            anchors.centerIn: parent
                            text: "✓"
                            color: Theme.accent
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.catTV = !root.catTV
                        }
                    }
                    Text {
                        text: "TV"
                        color: root.catTV ? Theme.text : Theme.textDim
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.catTV = !root.catTV
                        }
                    }
                }

                // Anime Checkbox
                RowLayout {
                    spacing: 10
                    Rectangle {
                        width: 20
                        height: 20
                        radius: 3
                        color: root.catAnime ? Theme.surface : Theme.bgRaised
                        border.width: 1
                        border.color: root.catAnime ? Theme.accent : Theme.border

                        Text {
                            visible: root.catAnime
                            anchors.centerIn: parent
                            text: "✓"
                            color: Theme.accent
                            font.pixelSize: 14
                            font.bold: true
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.catAnime = !root.catAnime
                        }
                    }
                    Text {
                        text: "Anime"
                        color: root.catAnime ? Theme.text : Theme.textDim
                        font.pixelSize: 15
                        font.weight: Font.Medium
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.catAnime = !root.catAnime
                        }
                    }
                }
            }

            // ---- 3. Search Bar + Active Switch Row ----
            RowLayout {
                visible: root.activeTab !== "direct"
                Layout.fillWidth: true
                spacing: 20

                // Search by name input box
                Rectangle {
                    Layout.fillWidth: true
                    height: 44
                    radius: 4
                    color: Theme.surface
                    border.width: 1
                    border.color: searchInput.activeFocus ? Theme.accent : Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 12

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: Theme.text
                            font.pixelSize: 15
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            selectByMouse: true

                            Text {
                                anchors.fill: parent
                                text: "Search by name"
                                color: Theme.textMute
                                font.pixelSize: 15
                                verticalAlignment: Text.AlignVCenter
                                visible: !searchInput.text && !searchInput.activeFocus
                            }

                            onAccepted: root.executeSearch()
                        }

                        Text {
                            visible: searchInput.text.length > 0
                            text: "✕"
                            color: Theme.textMute
                            font.pixelSize: 15
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    searchInput.text = ""
                                    if (fluxTorrent) {
                                        if (root.activeTab === "tork") fluxTorrent.clearTorkResults()
                                        else fluxTorrent.clearSearchResults()
                                    }
                                }
                            }
                        }

                        // Search Icon
                        FluxIcon {
                            name: "search"
                            size: 16
                            color: Theme.accent
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.executeSearch()
                            }
                        }
                    }
                }

                // Active Switch (Pill toggle from Screenshot)
                RowLayout {
                    spacing: 10
                    Layout.alignment: Qt.AlignVCenter

                    Rectangle {
                        width: 44
                        height: 24
                        radius: 12
                        color: root.activeOnly ? Theme.accent : Theme.surface
                        border.width: 1
                        border.color: root.activeOnly ? Theme.accent : Theme.border

                        Rectangle {
                            x: root.activeOnly ? 22 : 2
                            y: 2
                            width: 18
                            height: 18
                            radius: 9
                            color: Theme.text

                            Behavior on x {
                                NumberAnimation { duration: 120 }
                            }
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeOnly = !root.activeOnly
                        }
                    }

                    Text {
                        text: "Active"
                        color: root.activeOnly ? Theme.accent : Theme.textDim
                        font.pixelSize: 15
                        font.weight: root.activeOnly ? Font.Bold : Font.Normal

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.activeOnly = !root.activeOnly
                        }
                    }
                }
            }

            // ---- Active Torrent Stream Status Bar ----
            Rectangle {
                visible: !!fluxTorrent && fluxTorrent.status !== "idle" && fluxTorrent.status !== "error"
                Layout.fillWidth: true
                height: 36
                radius: 4
                color: Theme.bgRaised
                border.width: 1
                border.color: Theme.border

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: (fluxTorrent && fluxTorrent.status === "ready") ? Theme.success : Theme.accent
                    }

                    Text {
                        text: fluxTorrent ? (fluxTorrent.torrentName + " — " + fluxTorrent.statusMessage) : ""
                        color: Theme.text
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: fluxTorrent ? fluxTorrent.isReadyToPlay : false
                        text: "[Play in FLUX]"
                        color: Theme.accent
                        font.pixelSize: 13
                        font.bold: true
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (fluxTorrent && fluxTorrent.streamUrl) {
                                    root.playRequested(fluxTorrent.streamUrl, fluxTorrent.torrentName)
                                }
                            }
                        }
                    }

                    Text {
                        text: "Stop"
                        color: Theme.danger
                        font.pixelSize: 12
                        font.bold: true
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: if (fluxTorrent) fluxTorrent.stopTorrent()
                        }
                    }
                }
            }

            // ---- Searching Status ----
            Text {
                visible: !!fluxTorrent && (fluxTorrent.isSearchingTork || fluxTorrent.isSearching1337x)
                text: "Searching..."
                color: Theme.textDim
                font.pixelSize: 13
                Layout.alignment: Qt.AlignHCenter
            }

            // ==============================================================
            // 4. MAIN TORRENTS TABLE VIEW (Generous, highly readable sizes)
            // ==============================================================
            Item {
                visible: root.activeTab !== "direct"
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // ---- TABLE HEADER ROW ----
                    Rectangle {
                        Layout.fillWidth: true
                        height: 42
                        color: "transparent"

                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: Theme.border
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 14

                            Text {
                                text: "Type"
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 80
                            }

                            Text {
                                text: "Name"
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "DL"
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: 108
                            }

                            Text {
                                text: "Uploader"
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 110
                            }

                            Text {
                                text: "Size"
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 92
                            }

                            Text {
                                text: "S"
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: 50
                            }

                            Text {
                                text: "L"
                                color: Theme.accent
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: 50
                            }
                        }
                    }

                    // ---- EMPTY STATE ----
                    Item {
                        visible: root.displayedItems.length === 0 && !(fluxTorrent && (fluxTorrent.isSearchingTork || fluxTorrent.isSearching1337x))
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        Text {
                            anchors.centerIn: parent
                            text: root.activeOnly ? "No active torrents found with > 50 seeders" : "No torrents found. Type a query above to search."
                            color: Theme.textMute
                            font.pixelSize: 15
                        }
                    }

                    // ---- TABLE BODY ----
                    ListView {
                        id: torrentsListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: root.displayedItems

                        delegate: Rectangle {
                            id: rowRect
                            width: torrentsListView.width
                            height: 62
                            color: rowHover.containsMouse ? Theme.surfaceHover : "transparent"

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: Theme.border
                            }

                            readonly property var typeInfo: root.getTypeBadge(modelData.name, modelData.category)

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 14

                                // Type badge (TorrentLeech double-deck graphic stamp)
                                Rectangle {
                                    Layout.preferredWidth: 80
                                    height: 36
                                    radius: 4
                                    color: typeInfo.bg
                                    border.width: 1
                                    border.color: typeInfo.border

                                    ColumnLayout {
                                        anchors.centerIn: parent
                                        spacing: 1

                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: typeInfo.tag
                                            color: typeInfo.fg
                                            opacity: 0.8
                                            font.pixelSize: 9
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            Layout.alignment: Qt.AlignHCenter
                                            text: typeInfo.text
                                            color: typeInfo.fg
                                            font.pixelSize: 12
                                            font.weight: Font.Bold
                                        }
                                    }
                                }

                                // Name Column (Clean Title without red icon + secondary subtitle)
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    Text {
                                        text: modelData.name || ""
                                        color: rowHover.containsMouse ? "#ffffff" : Theme.text
                                        font.pixelSize: 15
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Secondary date/category tag
                                    Text {
                                        text: (modelData.category || "Media") + (modelData.date ? (" · " + modelData.date) : "")
                                        color: Theme.textMute
                                        font.pixelSize: 12
                                    }
                                }

                                // DL & Stream Action Column (Vector icons, matching font size, not stretched)
                                RowLayout {
                                    Layout.preferredWidth: 108
                                    spacing: 8
                                    Layout.alignment: Qt.AlignHCenter

                                    // Stream Icon
                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: 6
                                        color: streamMouse.containsMouse ? Theme.surface : "transparent"

                                        FluxIcon {
                                            anchors.centerIn: parent
                                            name: "play"
                                            size: 16
                                            color: streamMouse.containsMouse ? Theme.accent : Theme.textDim
                                        }

                                        MouseArea {
                                            id: streamMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (fluxTorrent) {
                                                    root.autoPlayTriggered = false
                                                    if (root.activeTab === "tork") {
                                                        fluxTorrent.streamTorkResult(modelData.itemIndex)
                                                    } else {
                                                        fluxTorrent.streamSearchResult(modelData.itemIndex)
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Download Icon (Crisp vector download tray, perfectly matching font size, never stretched)
                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: 6
                                        color: dlMouse.containsMouse ? Theme.surface : "transparent"

                                        FluxIcon {
                                            anchors.centerIn: parent
                                            name: "download"
                                            size: 16
                                            color: dlMouse.containsMouse ? Theme.accent : Theme.textDim
                                        }

                                        MouseArea {
                                            id: dlMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (fluxTorrent) {
                                                    if (root.activeTab === "tork") {
                                                        fluxTorrent.downloadTorkResult(modelData.itemIndex)
                                                    } else {
                                                        fluxTorrent.downloadSearchResult(modelData.itemIndex)
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    // Copy Magnet Link Icon (Normal clipboard / duplicate copy icon)
                                    Rectangle {
                                        width: 32
                                        height: 32
                                        radius: 6
                                        color: magnetMouse.containsMouse ? Theme.surface : "transparent"

                                        FluxIcon {
                                            anchors.centerIn: parent
                                            name: "copy"
                                            size: 16
                                            color: magnetMouse.containsMouse ? "#ffffff" : Theme.textMute
                                        }

                                        MouseArea {
                                            id: magnetMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                if (fluxTorrent) {
                                                    if (root.activeTab === "tork") {
                                                        fluxTorrent.copyTorkMagnet(modelData.itemIndex)
                                                    } else {
                                                        fluxTorrent.copyMagnet(modelData.itemIndex)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                // Uploader / Provider Column (Pastel colored names like screenshot)
                                Text {
                                    text: modelData.provider || (root.activeTab === "1337x" ? "1337x" : "Tork")
                                    color: root.getProviderColor(modelData.provider || (root.activeTab === "1337x" ? "1337x" : "Tork"))
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: 110
                                }

                                // Size Column (Grayish-white like leechers was previously)
                                Text {
                                    text: modelData.size || ""
                                    color: Theme.textDim
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    Layout.preferredWidth: 92
                                }

                                // S Column (Seeders in green)
                                Text {
                                    text: modelData.seedsVal !== undefined ? modelData.seedsVal : "0"
                                    color: Theme.success
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.preferredWidth: 50
                                }

                                // L Column (Leechers in soft red)
                                Text {
                                    text: modelData.leechesVal !== undefined ? modelData.leechesVal : "0"
                                    color: "#f87171"
                                    font.pixelSize: 14
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.preferredWidth: 50
                                }
                            }

                            MouseArea {
                                id: rowHover
                                anchors.fill: parent
                                hoverEnabled: true
                                z: -1
                            }
                        }
                    }
                }
            }

            // ==============================================================
            // 5. DIRECT MAGNET / FILE TAB
            // ==============================================================
            Item {
                visible: root.activeTab === "direct"
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 40, 600)
                    spacing: 18

                    Text {
                        text: "Direct Torrent Stream"
                        color: Theme.text
                        font.pixelSize: 18
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignHCenter
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            height: 42
                            radius: 4
                            color: Theme.surface
                            border.width: 1
                            border.color: magnetInput.activeFocus ? Theme.accent : Theme.border

                            TextInput {
                                id: magnetInput
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                verticalAlignment: TextInput.AlignVCenter
                                color: Theme.text
                                font.pixelSize: 14
                                clip: true

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: "Paste magnet:?xt=urn:btih:... link"
                                    color: Theme.textMute
                                    font.pixelSize: 14
                                    visible: !magnetInput.text && !magnetInput.activeFocus
                                }

                                onAccepted: {
                                    root.handleInput(magnetInput.text)
                                    magnetInput.text = ""
                                }
                            }
                        }

                        Rectangle {
                            implicitWidth: 86
                            height: 42
                            radius: 4
                            color: directBtnMouse.containsMouse ? Theme.accentHover : Theme.accent

                            Text {
                                anchors.centerIn: parent
                                text: "Stream"
                                color: "#ffffff"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                            }

                            MouseArea {
                                id: directBtnMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.handleInput(magnetInput.text)
                                    magnetInput.text = ""
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 120
                        radius: 6
                        color: dropMouse.containsMouse ? Theme.surfaceHover : Theme.surface
                        border.width: 1
                        border.color: Theme.border

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            FluxIcon {
                                name: "download"
                                size: 28
                                color: Theme.accent
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Drop .torrent file here or click to browse"
                                color: Theme.textDim
                                font.pixelSize: 14
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }

                        MouseArea {
                            id: dropMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (fluxTorrent) {
                                    var f = fluxTorrent.browseTorrentFile()
                                    if (f && f.length > 0) root.handleInput(f)
                                }
                            }
                        }
                    }
                }
            }

            // ---- Toast Notification ----
            Rectangle {
                visible: root.showToast
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: toastMsgText.implicitWidth + 28
                height: 32
                radius: 4
                color: Theme.bgRaised
                border.width: 1
                border.color: Theme.borderHi

                Text {
                    id: toastMsgText
                    anchors.centerIn: parent
                    text: root.toastMessage
                    color: "#e2e8f0"
                    font.pixelSize: 12
                }
            }
        }
    }
}
