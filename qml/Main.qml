import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "components"

ApplicationWindow {
    id: window

    visible: true
    width: 1280
    height: 840
    minimumWidth: 960
    minimumHeight: 640
    title: "FLUX — Native Media Player"
    color: "#080c14"

    property bool isFullscreen: false
    property bool showSidebar: true
    property int currentTab: 0 // 0 = Search, 1 = Test Catalog

    function toggleFullscreen() {
        isFullscreen = !isFullscreen
        if (isFullscreen) {
            window.showFullScreen()
        } else {
            window.showNormal()
        }
    }

    function executeSearch() {
        if (searchField.text.trim().length > 0) {
            currentTab = 0
            fluxSearch.search(searchField.text.trim())
        }
    }

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (isFullscreen) toggleFullscreen()
        }
    }

    Shortcut {
        sequence: "Space"
        onActivated: {
            if (fluxPlayer) fluxPlayer.togglePlay()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: isFullscreen ? 0 : 12
        spacing: 10

        // =====================================================================
        // Top Header (Hidden in fullscreen)
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            visible: !isFullscreen
            spacing: 12

            Text {
                text: "FLUX"
                font.pixelSize: 22
                font.bold: true
                color: "#ffffff"
                font.letterSpacing: 2
            }

            Rectangle {
                implicitWidth: badgeText.implicitWidth + 14
                implicitHeight: 22
                radius: 11
                color: "#1e3a8a"

                Text {
                    id: badgeText
                    anchors.centerIn: parent
                    text: "libVLC Native Direct Streamer • v0.1.0"
                    color: "#93c5fd"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            Button {
                id: toggleSidebarBtn
                implicitHeight: 26
                background: Rectangle {
                    color: toggleSidebarBtn.hovered ? "#1e293b" : "#0f172a"
                    border.color: "#334155"
                    border.width: 1
                    radius: 4
                }
                contentItem: Text {
                    text: showSidebar ? "◀ Hide Library" : "▶ Show Library"
                    color: "#38bdf8"
                    font.pixelSize: 11
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: showSidebar = !showSidebar
            }

            Item { Layout.fillWidth: true }

            Text {
                text: "Target: 172.16.50.14 (HTTP Byte Ranges)"
                color: "#64748b"
                font.pixelSize: 11
                font.family: "Consolas, monospace"
            }
        }

        // =====================================================================
        // Main Workspace (Sidebar + Player Viewport)
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // -----------------------------------------------------------------
            // Left Sidebar: Search & Test Catalog (Hidden in fullscreen)
            // -----------------------------------------------------------------
            Rectangle {
                id: sidebarPanel
                visible: !isFullscreen && showSidebar
                Layout.preferredWidth: 390
                Layout.fillHeight: true
                radius: 8
                color: "#0b1220"
                border.color: "#1e293b"
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    // Navigation Tabs: Search vs Test Catalog
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        // Tab 0: Search
                        Button {
                            id: tabSearchBtn
                            Layout.fillWidth: true
                            implicitHeight: 34
                            background: Rectangle {
                                color: currentTab === 0 ? "#1e293b" : "#0f172a"
                                border.color: currentTab === 0 ? "#3b82f6" : "#1e293b"
                                border.width: currentTab === 0 ? 2 : 1
                                radius: 6
                            }
                            contentItem: RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    text: "🔍  Search"
                                    color: currentTab === 0 ? "#ffffff" : "#94a3b8"
                                    font.pixelSize: 12
                                    font.bold: currentTab === 0
                                }
                                Rectangle {
                                    visible: fluxSearch && fluxSearch.hasResults
                                    implicitWidth: countText.implicitWidth + 8
                                    implicitHeight: 16
                                    radius: 8
                                    color: "#2563eb"
                                    Text {
                                        id: countText
                                        anchors.centerIn: parent
                                        text: fluxSearch ? fluxSearch.resultCount : 0
                                        color: "#ffffff"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }
                            onClicked: currentTab = 0
                        }

                        // Tab 1: Test Catalog
                        Button {
                            id: tabCatalogBtn
                            Layout.fillWidth: true
                            implicitHeight: 34
                            background: Rectangle {
                                color: currentTab === 1 ? "#1e293b" : "#0f172a"
                                border.color: currentTab === 1 ? "#3b82f6" : "#1e293b"
                                border.width: currentTab === 1 ? 2 : 1
                                radius: 6
                            }
                            contentItem: RowLayout {
                                anchors.centerIn: parent
                                spacing: 6
                                Text {
                                    text: "🎬  Test Catalog"
                                    color: currentTab === 1 ? "#ffffff" : "#94a3b8"
                                    font.pixelSize: 12
                                    font.bold: currentTab === 1
                                }
                                Rectangle {
                                    implicitWidth: 18
                                    implicitHeight: 16
                                    radius: 8
                                    color: "#334155"
                                    Text {
                                        anchors.centerIn: parent
                                        text: "3"
                                        color: "#94a3b8"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }
                            }
                            onClicked: currentTab = 1
                        }
                    }

                    // ---------------------------------------------------------
                    // VIEW 0: Search Interface
                    // ---------------------------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: currentTab === 0
                        spacing: 8

                        // Search Input Bar
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            TextField {
                                id: searchField
                                Layout.fillWidth: true
                                implicitHeight: 38
                                placeholderText: "Search media (e.g. kingdom, batman)..."
                                font.pixelSize: 12
                                color: "#f8fafc"
                                selectedTextColor: "#ffffff"
                                selectionColor: "#2563eb"

                                background: Rectangle {
                                    color: "#0f172a"
                                    border.color: searchField.activeFocus ? "#3b82f6" : "#1e293b"
                                    border.width: searchField.activeFocus ? 2 : 1
                                    radius: 6
                                }

                                onAccepted: executeSearch()
                            }

                            Button {
                                id: clearSearchBtn
                                visible: searchField.text.length > 0 || (fluxSearch && fluxSearch.hasResults)
                                implicitWidth: 32
                                implicitHeight: 38
                                background: Rectangle {
                                    color: clearSearchBtn.hovered ? "#334155" : "#1e293b"
                                    radius: 6
                                }
                                contentItem: Text {
                                    text: "✕"
                                    color: "#94a3b8"
                                    font.pixelSize: 13
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: {
                                    searchField.text = ""
                                    if (fluxSearch) fluxSearch.clear()
                                }
                            }

                            Button {
                                id: searchSubmitBtn
                                implicitWidth: 80
                                implicitHeight: 38
                                enabled: !(fluxSearch && fluxSearch.isSearching)
                                background: Rectangle {
                                    color: searchSubmitBtn.down ? "#1d4ed8" : (searchSubmitBtn.hovered ? "#2563eb" : "#3b82f6")
                                    radius: 6
                                }
                                contentItem: Text {
                                    text: "Search"
                                    color: "#ffffff"
                                    font.pixelSize: 12
                                    font.bold: true
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                onClicked: executeSearch()
                            }
                        }

                        // Search Status / Feedback Banner
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: statusLayout.implicitHeight + 10
                            radius: 4
                            color: {
                                if (fluxSearch && fluxSearch.hasError) return "#450a0a"
                                if (fluxSearch && fluxSearch.isSearching) return "#1e293b"
                                if (fluxSearch && fluxSearch.hasResults) return "#0f1f38"
                                return "#0d1527"
                            }
                            border.color: {
                                if (fluxSearch && fluxSearch.hasError) return "#dc2626"
                                if (fluxSearch && fluxSearch.isSearching) return "#3b82f6"
                                if (fluxSearch && fluxSearch.hasResults) return "#1e3a8a"
                                return "#1e293b"
                            }
                            border.width: 1

                            RowLayout {
                                id: statusLayout
                                anchors.fill: parent
                                anchors.margins: 6
                                spacing: 8

                                BusyIndicator {
                                    implicitWidth: 18
                                    implicitHeight: 18
                                    running: fluxSearch && fluxSearch.isSearching
                                    visible: fluxSearch && fluxSearch.isSearching
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: {
                                        if (fluxSearch && fluxSearch.isSearching) return "Searching 172.16.50.14..."
                                        if (fluxSearch && fluxSearch.hasError) return fluxSearch.errorMessage
                                        if (fluxSearch && fluxSearch.hasResults) return fluxSearch.statusMessage
                                        if (fluxSearch && fluxSearch.query.length > 0) return "No results found for \"" + fluxSearch.query + "\""
                                        return "Search 172.16.50.14 by pressing Enter or clicking Search"
                                    }
                                    color: {
                                        if (fluxSearch && fluxSearch.hasError) return "#fca5a5"
                                        if (fluxSearch && fluxSearch.isSearching) return "#60a5fa"
                                        if (fluxSearch && fluxSearch.hasResults) return "#93c5fd"
                                        return "#64748b"
                                    }
                                    font.pixelSize: 11
                                    font.family: "Consolas, monospace"
                                    elide: Text.ElideRight
                                }
                            }
                        }

                        // Search Results List
                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            ListView {
                                id: searchListView
                                width: parent.width
                                model: fluxSearch
                                spacing: 6

                                delegate: SearchResultItem {
                                    width: searchListView.width
                                    title: model.title
                                    parentPath: model.parentPath
                                    isFolder: model.isFolder
                                    formattedSize: model.formattedSize
                                    extension: model.extension
                                    playUrl: model.playUrl
                                    isPlaying: (fluxPlayer && fluxPlayer.url === model.playUrl)

                                    onPlayRequested: function(url) {
                                        urlInput.text = url
                                        if (fluxPlayer) fluxPlayer.play(url)
                                    }
                                }

                                // Empty State Placeholder
                                ColumnLayout {
                                    anchors.centerIn: parent
                                    visible: fluxSearch && !fluxSearch.isSearching && !fluxSearch.hasResults && fluxSearch.query.length === 0
                                    spacing: 8

                                    Text {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "🔎"
                                        font.pixelSize: 28
                                    }

                                    Text {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "Search media on BDIX server"
                                        color: "#64748b"
                                        font.pixelSize: 12
                                        font.bold: true
                                    }

                                    Text {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: "e.g. \"kingdom\", \"batman\", \"avengers\""
                                        color: "#475569"
                                        font.pixelSize: 11
                                        font.family: "Consolas, monospace"
                                    }
                                }
                            }
                        }
                    }

                    // ---------------------------------------------------------
                    // VIEW 1: Hardcoded Test Catalog
                    // ---------------------------------------------------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: currentTab === 1
                        spacing: 8

                        Text {
                            text: "PRE-VERIFIED TEST MEDIA"
                            color: "#94a3b8"
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 1
                        }

                        ScrollView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true

                            ListView {
                                id: catalogListView
                                width: parent.width
                                model: testMediaModel
                                spacing: 8

                                delegate: MediaCard {
                                    width: catalogListView.width
                                    title: model.title
                                    year: model.year
                                    badge: model.badge
                                    description: model.description
                                    url: model.url
                                    isSelected: (fluxPlayer && fluxPlayer.url === model.url)
                                    onClicked: {
                                        urlInput.text = model.url
                                        if (fluxPlayer) fluxPlayer.play(model.url)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // -----------------------------------------------------------------
            // Right Area: Direct URL Bar + Video Viewport + Diagnostics
            // -----------------------------------------------------------------
            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 10

                // Custom URL Input Row (Hidden in fullscreen)
                RowLayout {
                    Layout.fillWidth: true
                    visible: !isFullscreen
                    spacing: 8

                    TextField {
                        id: urlInput
                        Layout.fillWidth: true
                        implicitHeight: 40
                        placeholderText: "Direct media URL: http://172.16.50.14/... (MKV / MP4)"
                        text: fluxPlayer && fluxPlayer.url.length > 0 ? fluxPlayer.url : "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/Ant-Man%20and%20the%20Wasp-Quantumania%20%282023%29%201080p%20DSNP/Ant-Man%20and%20the%20Wasp%20Quantumania%20%282023%29%201080p%20DSNP-WEB%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-PSA.mkv"
                        font.pixelSize: 11
                        font.family: "Consolas, monospace"
                        color: "#f8fafc"
                        selectedTextColor: "#ffffff"
                        selectionColor: "#2563eb"

                        background: Rectangle {
                            color: "#0f172a"
                            border.color: urlInput.activeFocus ? "#3b82f6" : "#1e293b"
                            border.width: urlInput.activeFocus ? 2 : 1
                            radius: 6
                        }

                        onAccepted: {
                            if (fluxPlayer && text.trim().length > 0) {
                                fluxPlayer.play(text.trim())
                            }
                        }
                    }

                    Button {
                        id: playUrlBtn
                        implicitWidth: 90
                        implicitHeight: 40
                        background: Rectangle {
                            color: playUrlBtn.down ? "#1d4ed8" : (playUrlBtn.hovered ? "#2563eb" : "#3b82f6")
                            radius: 6
                        }
                        contentItem: Text {
                            text: "▶  PLAY"
                            color: "#ffffff"
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (fluxPlayer && urlInput.text.trim().length > 0) {
                                fluxPlayer.play(urlInput.text.trim())
                            }
                        }
                    }

                    Button {
                        id: stopUrlBtn
                        implicitWidth: 80
                        implicitHeight: 40
                        background: Rectangle {
                            color: stopUrlBtn.down ? "#334155" : (stopUrlBtn.hovered ? "#1e293b" : "#0f172a")
                            border.color: "#334155"
                            border.width: 1
                            radius: 6
                        }
                        contentItem: Text {
                            text: "■  STOP"
                            color: "#94a3b8"
                            font.pixelSize: 12
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (fluxPlayer) fluxPlayer.stop()
                        }
                    }
                }

                // Video Viewport
                PlayerView {
                    id: playerView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    player: fluxPlayer
                    onToggleFullscreenRequested: window.toggleFullscreen()
                }

                // Diagnostics Drawer (Hidden in fullscreen)
                DiagnosticPanel {
                    Layout.fillWidth: true
                    visible: !isFullscreen
                    player: fluxPlayer
                    logger: fluxLogger
                }
            }
        }
    }
}
