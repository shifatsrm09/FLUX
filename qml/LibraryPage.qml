import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "components/MediaFormatter.js" as Formatter
import "components/Theme.js" as Theme

// Library: everything already downloaded to disk. Plays without any network.
// Top level shows the downloaded series / packs as folders, then the individual movies.
// Episodes live inside their pack folder. A separate search box searches the whole library.
Item {
    id: root

    // Space reserved at the top for the overlaid nav bar
    property real topInset: 0

    // True while this page is on screen (the disk is only read then)
    property bool active: false

    // Folder being browsed, relative to the download location ("" = top level)
    property string rel: ""
    // Bumped to force a re-read of the disk
    property int refreshTick: 0
    // Debounced search text (what is actually searched)
    property string query: ""

    readonly property bool searching: root.query.trim().length > 0
    readonly property real pageMargin: Math.max(32, Math.round((root.width - 1240) / 2))

    // Ask the app to play a file (a file:// URL)
    signal playRequested(string url, string title)

    // Re-read when downloads finish, the location changes, the page opens, a search is typed
    // or Refresh is pressed
    readonly property var rawEntries: {
        var revision = fluxDownloads ? fluxDownloads.offlineRevision : 0
        var tick = root.refreshTick
        if (!root.active || !fluxDownloads) return []
        return root.searching ? fluxDownloads.offlineSearch(root.query)
                              : fluxDownloads.offlineList(root.rel)
    }

    readonly property var folderEntries: {
        var raw = root.rawEntries
        var folders = []
        for (var i = 0; i < raw.length; ++i) {
            if (raw[i].kind === "folder") folders.push(raw[i])
        }
        return folders
    }

    readonly property var videoEntries: {
        var raw = root.rawEntries
        var videos = []
        for (var i = 0; i < raw.length; ++i) {
            if (raw[i].kind !== "folder") videos.push(raw[i])
        }
        return videos
    }

    readonly property var entries: root.folderEntries.concat(root.videoEntries)

    function getSectionedRows(cols) {
        var folders = root.folderEntries
        var videos = root.videoEntries
        var rows = []
        var cCount = Math.max(1, cols)

        var first = true
        if (folders.length > 0) {
            rows.push({
                isHeader: true,
                isFolder: true,
                isFirstHeader: first,
                title: "Folders",
                icon: "folder",
                countText: folders.length + (folders.length === 1 ? " folder" : " folders")
            })
            first = false

            for (var i = 0; i < folders.length; i += cCount) {
                var chunk = []
                for (var c = 0; c < cCount && (i + c) < folders.length; ++c) {
                    chunk.push(folders[i + c])
                }
                rows.push({
                    isHeader: false,
                    isFolder: true,
                    isFirstHeader: false,
                    items: chunk
                })
            }
        }

        if (videos.length > 0) {
            rows.push({
                isHeader: true,
                isFolder: false,
                isFirstHeader: first,
                title: "Movies & Episodes",
                icon: "play",
                countText: videos.length + (videos.length === 1 ? " item" : " items")
            })
            first = false

            for (var j = 0; j < videos.length; j += cCount) {
                var chunkV = []
                for (var k = 0; k < cCount && (j + k) < videos.length; ++k) {
                    chunkV.push(videos[j + k])
                }
                rows.push({
                    isHeader: false,
                    isFolder: false,
                    isFirstHeader: false,
                    items: chunkV
                })
            }
        }

        return rows
    }

    // Parent folder (the manager knows about the Series / Individuals containers and added targets)
    function parentRel(path) {
        return fluxDownloads ? fluxDownloads.offlineParent(path) : ""
    }

    function crumbText(path) {
        if (path.length === 0) return "Library"
        // Inside an added target: show the last few folders of its path
        if (path.charAt(0) === "@") {
            var segs = path.substring(1).split("/").filter(function(s) { return s.length > 0 })
            return "Library  ›  " + (segs.length > 3 ? "…  ›  " : "") + segs.slice(-3).join("  ›  ")
        }
        var parts = path.split("/")
        if (parts[0] === "Series" || parts[0] === "Individuals") parts.shift()
        return "Library  ›  " + parts.join("  ›  ")
    }

    function clearSearch() {
        searchField.text = ""
        root.query = ""
    }

    // Escape / Back: clear the search first, then step out one folder. Returns false at the top.
    function goUp() {
        if (searchField.text.length > 0) {
            root.clearSearch()
            return true
        }
        if (root.rel.length === 0) return false
        root.rel = root.parentRel(root.rel)
        return true
    }

    // A new download location means a different library
    Connections {
        target: fluxDownloads

        function onLocationChanged() {
            root.rel = ""
            root.clearSearch()
        }
    }

    Timer {
        id: searchDebounce
        interval: 200
        onTriggered: root.query = searchField.text
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: root.topInset + 10
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        anchors.bottomMargin: 24
        spacing: 16

        // ---- Heading + library search ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: "Library"
                    color: Theme.text
                    font.pixelSize: 26
                    font.weight: Font.Bold
                    font.letterSpacing: -0.4
                }

                Text {
                    text: "Everything you've downloaded. Plays without an internet connection."
                    color: Theme.textDim
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }

            // Search the offline library (separate from the main search)
            Rectangle {
                Layout.preferredWidth: 340
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: 42
                radius: 21
                color: searchField.activeFocus ? "#26FFFFFF" : "#1AFFFFFF"
                border.width: 1
                border.color: searchField.activeFocus ? Theme.accentRing : Theme.border

                Behavior on color { ColorAnimation { duration: 120 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 6
                    spacing: 8

                    FluxIcon {
                        name: "search"
                        size: 18
                        color: Theme.textDim
                        Layout.alignment: Qt.AlignVCenter
                    }

                    TextField {
                        id: searchField

                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignVCenter
                        placeholderText: "Search your library"
                        placeholderTextColor: Theme.textMute
                        color: Theme.text
                        font.pixelSize: 14
                        selectByMouse: true
                        leftPadding: 0
                        rightPadding: 0
                        background: null

                        onTextChanged: searchDebounce.restart()
                        onAccepted: {
                            searchDebounce.stop()
                            root.query = searchField.text
                        }
                    }

                    IconButton {
                        visible: searchField.text.length > 0
                        iconName: "close"
                        iconSize: 14
                        implicitWidth: 30
                        implicitHeight: 30
                        tip: "Clear search"
                        tipAbove: false
                        Layout.alignment: Qt.AlignVCenter
                        onClicked: {
                            root.clearSearch()
                            searchField.forceActiveFocus()
                        }
                    }
                }
            }
        }

        // ---- Location bar: back + breadcrumb + refresh + open in Explorer ----
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            IconButton {
                iconName: "back"
                iconSize: 16
                implicitWidth: 34
                implicitHeight: 34
                filled: true
                visible: root.rel.length > 0 || root.searching
                tip: "Back"
                tipAbove: false
                onClicked: root.goUp()
            }

            Text {
                text: root.searching
                      ? (root.entries.length === 1 ? "1 result for \"" + root.query.trim() + "\""
                                                   : root.entries.length + " results for \"" + root.query.trim() + "\"")
                      : root.crumbText(root.rel)
                color: Theme.textDim
                font.pixelSize: 13
                font.weight: Font.DemiBold
                elide: Text.ElideLeft
                Layout.fillWidth: true
            }

            IconButton {
                iconName: "refresh"
                iconSize: 15
                implicitWidth: 34
                implicitHeight: 34
                tip: "Refresh"
                tipAbove: false
                onClicked: root.refreshTick++
            }

            IconButton {
                iconName: "folder"
                iconSize: 16
                implicitWidth: 34
                implicitHeight: 34
                tip: "Open this folder in Explorer"
                tipAbove: false
                onClicked: {
                    if (!fluxDownloads) return
                    // Top level (or search) = the download location itself; an added target
                    // ("@<path>") opens that folder; otherwise a sub folder of the downloads
                    if (root.rel.length === 0 || root.searching) {
                        fluxDownloads.openRoot()
                    } else if (root.rel.charAt(0) === "@") {
                        fluxDownloads.openPath(root.rel.substring(1))
                    } else {
                        fluxDownloads.openPath(fluxDownloads.downloadLocation + "\\"
                                               + root.rel.split("/").join("\\"))
                    }
                }
            }
        }

        // ---- Empty state ----
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 70
            spacing: 10
            visible: root.entries.length === 0

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 72
                implicitHeight: 72
                radius: 36
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                FluxIcon {
                    anchors.centerIn: parent
                    name: root.searching ? "search" : "folder"
                    size: 30
                    color: Theme.textMute
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: root.searching ? "No matches in your library"
                                     : (root.rel.length === 0 ? "Nothing downloaded yet" : "No videos in this folder")
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: root.searching ? "Try fewer or different words."
                                     : "Downloaded movies and series show up here and play without an internet connection."
                color: Theme.textDim
                font.pixelSize: 14
            }
        }

        // ---- Sectioned Virtualized Library Results View: Folders first, then Movies/Episodes ----
        ListView {
            id: libraryListView

            readonly property int targetCardW: 172
            readonly property int columns: Math.max(2, Math.floor((width + 14) / (targetCardW + 14)))
            readonly property real cardW: Math.floor((width - (columns - 1) * 14) / columns)
            readonly property real cardH: Math.round(cardW * 1.5)

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.entries.length > 0
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 800

            ScrollBar.vertical: FluxScrollBar { }

            model: root.getSectionedRows(libraryListView.columns)

            footer: Item {
                width: libraryListView.width
                height: 36
            }

            delegate: Item {
                id: rowDelegate
                required property var modelData
                required property int index

                readonly property var rowInfo: modelData

                width: libraryListView.width
                height: rowInfo.isHeader ? (rowInfo.isFirstHeader ? 46 : 66) : (libraryListView.cardH + 14)

                // SECTION HEADER
                RowLayout {
                    visible: rowInfo.isHeader === true
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    spacing: 10

                    Text {
                        text: rowInfo.title || ""
                        color: Theme.text
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        font.letterSpacing: -0.2
                    }

                    Rectangle {
                        height: 22
                        implicitWidth: countBadgeText.implicitWidth + 16
                        radius: 11
                        color: Theme.surfaceHi
                        border.width: 1
                        border.color: Theme.border

                        Text {
                            id: countBadgeText
                            anchors.centerIn: parent
                            text: rowInfo.countText || ""
                            color: Theme.textDim
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }

                    Item { Layout.fillWidth: true }
                }

                // CARDS ROW
                Row {
                    visible: rowInfo.isHeader !== true
                    anchors.left: parent.left
                    anchors.top: parent.top
                    spacing: 14

                    Repeater {
                        model: rowInfo.items || []

                        delegate: SearchResultCard {
                            required property var modelData
                            required property int index

                            readonly property bool isVideo: modelData.kind === "video"
                            readonly property var prog: {
                                var rev = fluxUser ? fluxUser.revision : 0
                                return (isVideo && fluxUser) ? fluxUser.progressFor(modelData.url) : ({})
                            }

                            width: libraryListView.cardW
                            height: libraryListView.cardH
                            showDownload: false
                            title: modelData.name || ""
                            parentPath: modelData.path || ""
                            isFolder: !isVideo
                            formattedSize: modelData.detail || ""
                            extension: modelData.extension || ""
                            playUrl: isVideo ? (modelData.url || "") : (modelData.rel || "")
                            localPath: modelData.path || ""
                            libraryName: {
                                if (root.searching && modelData.folder && modelData.folder.length > 0) {
                                    return modelData.folder.split("/").join(" \u203A ")
                                }
                                return modelData.section || ""
                            }
                            isPlaying: (isVideo && !!fluxPlayer && fluxPlayer.url === modelData.url && fluxPlayer.isPlaying)
                            progress: (prog.fraction !== undefined) ? prog.fraction : 0
                            watched: (prog.watched === true)
                            introDelay: (index % 4) * 40

                            onPlayRequested: function(url, itemTitle) {
                                root.playRequested(modelData.url, itemTitle)
                            }

                            onFolderRequested: function(url, libName) {
                                var target = modelData.rel
                                root.clearSearch()
                                root.rel = target
                            }
                        }
                    }
                }
            }
        }
    }
}
