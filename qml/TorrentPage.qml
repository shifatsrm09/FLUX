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

    // Outer Background matching screenshot
    Rectangle {
        anchors.fill: parent
        color: "#1a222b"

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
                    color: "#e2e8f0"
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
                        color: root.activeTab === "tork" ? "#0d9488" : "#1e2934"
                        border.width: 1
                        border.color: "#2a3746"

                        Text {
                            id: torkTabText
                            anchors.centerIn: parent
                            text: "Tork"
                            color: root.activeTab === "tork" ? "#ffffff" : "#94a3b8"
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
                        color: root.activeTab === "1337x" ? "#0d9488" : "#1e2934"
                        border.width: 1
                        border.color: "#2a3746"

                        Text {
                            id: x1337TabText
                            anchors.centerIn: parent
                            text: "1337x"
                            color: root.activeTab === "1337x" ? "#ffffff" : "#94a3b8"
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
                        color: root.activeTab === "direct" ? "#0d9488" : "#1e2934"
                        border.width: 1
                        border.color: "#2a3746"

                        Text {
                            id: directTabText
                            anchors.centerIn: parent
                            text: "Direct Magnet"
                            color: root.activeTab === "direct" ? "#ffffff" : "#94a3b8"
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
                        color: root.catMovies ? "#222c37" : "#161c24"
                        border.width: 1
                        border.color: root.catMovies ? "#64748b" : "#334155"

                        Text {
                            visible: root.catMovies
                            anchors.centerIn: parent
                            text: "✓"
                            color: "#14b8a6"
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
                        color: root.catMovies ? "#e2e8f0" : "#808f9f"
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
                        color: root.catTV ? "#222c37" : "#161c24"
                        border.width: 1
                        border.color: root.catTV ? "#64748b" : "#334155"

                        Text {
                            visible: root.catTV
                            anchors.centerIn: parent
                            text: "✓"
                            color: "#14b8a6"
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
                        color: root.catTV ? "#e2e8f0" : "#808f9f"
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
                        color: root.catAnime ? "#222c37" : "#161c24"
                        border.width: 1
                        border.color: root.catAnime ? "#64748b" : "#334155"

                        Text {
                            visible: root.catAnime
                            anchors.centerIn: parent
                            text: "✓"
                            color: "#14b8a6"
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
                        color: root.catAnime ? "#e2e8f0" : "#808f9f"
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
                    radius: 3
                    color: "#161c23"
                    border.width: 1
                    border.color: searchInput.activeFocus ? "#475569" : "#242d38"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 12

                        TextInput {
                            id: searchInput
                            Layout.fillWidth: true
                            color: "#e2e8f0"
                            font.pixelSize: 15
                            verticalAlignment: TextInput.AlignVCenter
                            clip: true
                            selectByMouse: true

                            Text {
                                anchors.fill: parent
                                text: "Search by name"
                                color: "#64748b"
                                font.pixelSize: 15
                                verticalAlignment: Text.AlignVCenter
                                visible: !searchInput.text && !searchInput.activeFocus
                            }

                            onAccepted: root.executeSearch()
                        }

                        Text {
                            visible: searchInput.text.length > 0
                            text: "✕"
                            color: "#94a3b8"
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
                        Text {
                            text: "🔍"
                            font.pixelSize: 16
                            color: "#14b8a6"
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
                        color: root.activeOnly ? "#0d9488" : "#2d3748"

                        Rectangle {
                            x: root.activeOnly ? 22 : 2
                            y: 2
                            width: 20
                            height: 20
                            radius: 10
                            color: "#e2e8f0"

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
                        color: root.activeOnly ? "#2dd4bf" : "#a0aec0"
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
                color: "#161c24"
                border.width: 1
                border.color: "#242d38"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    spacing: 10

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: (fluxTorrent && fluxTorrent.status === "ready") ? "#22c55e" : "#0d9488"
                    }

                    Text {
                        text: fluxTorrent ? (fluxTorrent.torrentName + " — " + fluxTorrent.statusMessage) : ""
                        color: "#cbd5e1"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        visible: fluxTorrent ? fluxTorrent.isReadyToPlay : false
                        text: "[Play in FLUX]"
                        color: "#2dd4bf"
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
                        color: "#f87171"
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
                color: "#94a3b8"
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
                            color: "#242d38"
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 14

                            Text {
                                text: "Type"
                                color: "#14b8a6"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 80
                            }

                            Text {
                                text: "Name"
                                color: "#14b8a6"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "DL"
                                color: "#14b8a6"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: 104
                            }

                            Text {
                                text: "Uploader"
                                color: "#14b8a6"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 110
                            }

                            Text {
                                text: "Size"
                                color: "#14b8a6"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                Layout.preferredWidth: 92
                            }

                            Text {
                                text: "S"
                                color: "#14b8a6"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                Layout.preferredWidth: 50
                            }

                            Text {
                                text: "L"
                                color: "#14b8a6"
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
                            color: "#64748b"
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
                            color: rowHover.containsMouse ? "#202a35" : "transparent"

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: "#222c37"
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

                                // Name Column (Bookmark flag + Title + secondary subtitle)
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: "⚑"
                                            color: "#f43f5e"
                                            font.pixelSize: 15
                                        }

                                        Text {
                                            text: modelData.name || ""
                                            color: rowHover.containsMouse ? "#ffffff" : "#cbd5e1"
                                            font.pixelSize: 15
                                            font.weight: Font.Medium
                                            elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // Secondary date/category tag
                                    Text {
                                        text: (modelData.category || "Media") + (modelData.date ? (" · " + modelData.date) : "")
                                        color: "#64748b"
                                        font.pixelSize: 12
                                    }
                                }

                                // DL & Stream Action Column (Enlarged, easy-to-click icons)
                                RowLayout {
                                    Layout.preferredWidth: 104
                                    spacing: 10
                                    Layout.alignment: Qt.AlignHCenter

                                    // Stream Icon
                                    Rectangle {
                                        width: 30
                                        height: 30
                                        radius: 4
                                        color: streamMouse.containsMouse ? "#1e2d3d" : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "▶"
                                            color: streamMouse.containsMouse ? "#2dd4bf" : "#94a3b8"
                                            font.pixelSize: 16
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

                                    // Download Icon (DL tray in screenshot)
                                    Rectangle {
                                        width: 30
                                        height: 30
                                        radius: 4
                                        color: dlMouse.containsMouse ? "#1e2d3d" : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "⭳"
                                            color: dlMouse.containsMouse ? "#38bdf8" : "#94a3b8"
                                            font.pixelSize: 19
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

                                    // Magnet Link
                                    Rectangle {
                                        width: 30
                                        height: 30
                                        radius: 4
                                        color: magnetMouse.containsMouse ? "#1e2d3d" : "transparent"

                                        Text {
                                            anchors.centerIn: parent
                                            text: "⚯"
                                            color: magnetMouse.containsMouse ? "#ffffff" : "#64748b"
                                            font.pixelSize: 18
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

                                // Size Column (Teal like in screenshot)
                                Text {
                                    text: modelData.size || ""
                                    color: "#2dd4bf"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    Layout.preferredWidth: 92
                                }

                                // S Column (Seeders in green)
                                Text {
                                    text: modelData.seedsVal !== undefined ? modelData.seedsVal : "0"
                                    color: "#22c55e"
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                    horizontalAlignment: Text.AlignHCenter
                                    Layout.preferredWidth: 50
                                }

                                // L Column (Leechers in gray)
                                Text {
                                    text: modelData.leechesVal !== undefined ? modelData.leechesVal : "0"
                                    color: "#94a3b8"
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
                        color: "#e2e8f0"
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
                            radius: 3
                            color: "#161c23"
                            border.width: 1
                            border.color: magnetInput.activeFocus ? "#475569" : "#242d38"

                            TextInput {
                                id: magnetInput
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                verticalAlignment: TextInput.AlignVCenter
                                color: "#e2e8f0"
                                font.pixelSize: 14
                                clip: true

                                Text {
                                    anchors.fill: parent
                                    verticalAlignment: Text.AlignVCenter
                                    text: "Paste magnet:?xt=urn:btih:... link"
                                    color: "#64748b"
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
                            radius: 3
                            color: "#0d9488"

                            Text {
                                anchors.centerIn: parent
                                text: "Stream"
                                color: "#ffffff"
                                font.pixelSize: 14
                                font.weight: Font.Medium
                            }

                            MouseArea {
                                anchors.fill: parent
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
                        radius: 4
                        color: dropMouse.containsMouse ? "#1c2530" : "#161c23"
                        border.width: 1
                        border.color: "#242d38"

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            Text {
                                text: "⭳"
                                color: "#14b8a6"
                                font.pixelSize: 24
                                Layout.alignment: Qt.AlignHCenter
                            }

                            Text {
                                text: "Drop .torrent file here or click to browse"
                                color: "#94a3b8"
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
                color: "#1e2934"
                border.width: 1
                border.color: "#334155"

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
