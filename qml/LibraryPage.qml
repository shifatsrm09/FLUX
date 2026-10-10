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
    readonly property var entries: {
        var revision = fluxDownloads ? fluxDownloads.offlineRevision : 0
        var tick = root.refreshTick
        if (!root.active || !fluxDownloads) return []
        if (root.searching) return fluxDownloads.offlineSearch(root.query)
        return fluxDownloads.offlineList(root.rel)
    }

    // Parent folder; the Series / Individuals containers are never shown, so going up from a
    // pack returns to the top level
    function parentRel(path) {
        var i = path.lastIndexOf("/")
        if (i < 0) return ""
        var parent = path.substring(0, i)
        return (parent === "Series" || parent === "Individuals") ? "" : parent
    }

    function crumbText(path) {
        if (path.length === 0) return "Library"
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
                    // Top level (or search) = the download location itself; deeper = that folder
                    if (root.rel.length === 0 || root.searching) {
                        fluxDownloads.openRoot()
                    } else {
                        fluxDownloads.openPath(fluxDownloads.downloadLocation + "\\"
                                               + root.rel.split("/").join("\\"))
                    }
                }
            }
        }

        // ---- Listing ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 14
            color: Theme.bgRaised
            border.width: 1
            border.color: Theme.border
            clip: true

            ListView {
                id: libraryList

                anchors.fill: parent
                anchors.margins: 1
                clip: true
                model: root.entries
                boundsBehavior: Flickable.StopAtBounds

                // Section headings ("TV Series & Packs" / "Movies") only on the top level
                section.property: (root.rel.length === 0 && !root.searching) ? "section" : ""
                section.criteria: ViewSection.FullString
                section.delegate: Item {
                    required property string section

                    width: ListView.view.width
                    height: 40

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 8
                        text: parent.section
                        color: Theme.textMute
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        font.letterSpacing: 1.2
                        font.capitalization: Font.AllUppercase
                    }
                }

                ScrollBar.vertical: FluxScrollBar { }

                delegate: Item {
                    id: orow

                    required property var modelData

                    readonly property bool isVideo: modelData.kind === "video"
                    readonly property var meta: isVideo ? Formatter.describe(modelData.name, false, modelData.detail) : ({})
                    readonly property var parsed: isVideo ? Formatter.formatMedia(modelData.name, false, modelData.detail) : ({})
                    // Reading `revision` refreshes the watch badge when history changes
                    readonly property var prog: {
                        var rev = fluxUser ? fluxUser.revision : 0
                        return (isVideo && fluxUser) ? fluxUser.progressFor(modelData.url) : ({})
                    }
                    readonly property real fraction: prog.fraction !== undefined ? prog.fraction : 0
                    readonly property bool watched: prog.watched === true

                    readonly property string subtitle: {
                        if (!isVideo) return modelData.detail
                        var parts = []
                        // In search results, say which pack an episode belongs to
                        if (root.searching && modelData.folder && modelData.folder.length > 0) {
                            parts.push(modelData.folder.split("/").join(" › "))
                        }
                        if (meta.episode && meta.episode.length > 0) parts.push(meta.episode)
                        if (meta.resolution && meta.resolution.length > 0) parts.push(meta.resolution)
                        parts.push(modelData.detail)
                        if (watched) parts.push("Watched")
                        return parts.join("  ·  ")
                    }

                    width: ListView.view.width
                    height: 66

                    Rectangle {
                        anchors.fill: parent
                        color: (orowMouse.containsMouse || openFolderBtn.hovered) ? "#14FFFFFF" : "transparent"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 12
                        spacing: 14

                        Rectangle {
                            implicitWidth: 40
                            implicitHeight: 40
                            radius: 10
                            color: orow.isVideo ? Theme.posterTop(modelData.name) : Theme.surfaceHi
                            Layout.alignment: Qt.AlignVCenter

                            FluxIcon {
                                anchors.centerIn: parent
                                name: orow.isVideo ? "play" : "folder"
                                size: 18
                                color: orow.isVideo ? "#FFFFFF" : Theme.textDim
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 2

                            Text {
                                text: orow.isVideo ? orow.parsed.title : modelData.name
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Text {
                                text: orow.subtitle
                                color: Theme.textDim
                                font.pixelSize: 12
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Watch progress
                            Rectangle {
                                visible: orow.fraction > 0 && !orow.watched
                                Layout.fillWidth: true
                                implicitHeight: 3
                                radius: 1.5
                                color: "#2EFFFFFF"

                                Rectangle {
                                    width: parent.width * Math.max(0, Math.min(1, orow.fraction))
                                    height: parent.height
                                    radius: 1.5
                                    color: Theme.accent
                                }
                            }
                        }

                        IconButton {
                            id: openFolderBtn
                            visible: orow.isVideo
                            iconName: "folder"
                            iconSize: 15
                            iconColor: Theme.textDim
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Show in folder"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.openPath(modelData.path)
                        }

                        FluxIcon {
                            visible: !orow.isVideo
                            name: "chevronRight"
                            size: 16
                            color: Theme.textMute
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    // Row click. The right edge is left free so the folder button stays clickable.
                    MouseArea {
                        id: orowMouse
                        anchors.fill: parent
                        anchors.rightMargin: orow.isVideo ? 56 : 0
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            if (orow.isVideo) {
                                root.playRequested(modelData.url, orow.parsed.title)
                            } else {
                                // Open the pack; leave search mode so its contents show
                                var target = modelData.rel
                                root.clearSearch()
                                root.rel = target
                            }
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 20
                        height: 1
                        color: Theme.border
                    }
                }

                // Empty state
                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 60
                    spacing: 8
                    visible: libraryList.count === 0

                    FluxIcon {
                        name: root.searching ? "search" : "folder"
                        size: 34
                        color: Theme.textMute
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: root.searching ? "No matches in your library"
                                             : (root.rel.length === 0 ? "Nothing downloaded yet" : "No videos in this folder")
                        color: Theme.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: root.searching ? "Try fewer or different words."
                                             : "Downloaded movies and series show up here and play without an internet connection."
                        color: Theme.textMute
                        font.pixelSize: 13
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }
}
