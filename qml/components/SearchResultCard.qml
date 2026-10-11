import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "MediaFormatter.js" as Formatter
import "Theme.js" as Theme

// Streaming-service style tile for one file or folder.
// Sized by its parent (GridView / ListView cell); the visible card is inset by 7px so the
// hover-zoom has room to breathe.
Item {
    id: root

    property string title: ""
    property string parentPath: ""
    property bool isFolder: false
    property string formattedSize: ""
    property string extension: ""
    property string playUrl: ""
    property string libraryName: ""
    property bool isPlaying: false

    // Watch state
    property real progress: 0          // 0..1, shows a progress bar when > 0
    property bool watched: false
    property bool showRemove: false    // "x" button on hover (Continue Watching row)
    property string extraInfo: ""      // e.g. "23 min left"

    // Hover download button (files: download; folders: download the whole pack)
    property bool showDownload: false
    property bool dlFlash: false       // brief "added" confirmation

    // Entrance animation stagger (ms)
    property int introDelay: 0

    signal playRequested(string url, string title)
    signal folderRequested(string url, string libraryName)
    signal removeRequested(string url)
    signal downloadRequested(string url, string title, bool isPack)

    // Bookmark state (reading `revision` makes this re-evaluate whenever bookmarks change)
    readonly property bool bookmarked: {
        var rev = fluxBookmarks ? fluxBookmarks.revision : 0
        return fluxBookmarks ? fluxBookmarks.isBookmarked(root.playUrl) : false
    }

    // Parsed presentation values
    readonly property var parsed: Formatter.formatMedia(root.title, root.isFolder, root.formattedSize)
    readonly property var meta: Formatter.describe(root.title, root.isFolder, root.formattedSize)
    readonly property bool hovered: rowMouse.containsMouse || removeMouse.containsMouse || dlMouse.containsMouse
    readonly property string posterTop: Theme.posterTop(root.title)
    readonly property string posterBottom: Theme.posterBottom(root.title)

    // Big outlined mark on the poster: episode number > year > first letter
    readonly property string bigText: {
        if (root.isFolder) return ""
        if (root.meta.episode.length > 0) {
            var m = root.meta.episode.match(/E(\d+)/)
            return m ? ("E" + m[1]) : root.meta.episode
        }
        if (root.meta.year.length > 0) return root.meta.year
        return root.parsed.title.length > 0 ? root.parsed.title.charAt(0).toUpperCase() : ""
    }

    readonly property string infoLine: {
        if (root.isFolder) return "Folder"
        var parts = []
        if (root.extraInfo.length > 0) parts.push(root.extraInfo)
        if (root.meta.episode.length > 0) parts.push(root.meta.episode)
        else if (root.meta.year.length > 0) parts.push(root.meta.year)
        if (root.meta.source.length > 0) parts.push(root.meta.source)
        if (root.meta.codec.length > 0) parts.push(root.meta.codec)
        if (root.formattedSize.length > 0 && root.formattedSize !== "Folder") parts.push(root.formattedSize)
        return parts.join("  \u00B7  ")
    }

    // Raise the hovered tile above its neighbours while it is scaled up
    z: root.hovered ? 20 : 0

    // ---- Entrance animation: fade + rise, staggered per card --------------------------
    property bool shown: false

    opacity: root.shown ? 1.0 : 0.0
    transform: Translate {
        y: root.shown ? 0 : 18

        Behavior on y { NumberAnimation { duration: 520; easing.type: Easing.OutCubic } }
    }

    Behavior on opacity { NumberAnimation { duration: 420; easing.type: Easing.OutCubic } }

    Component.onCompleted: introTimer.start()

    Timer {
        id: introTimer
        interval: root.introDelay
        repeat: false
        onTriggered: root.shown = true
    }

    // Small label chip
    component Badge: Rectangle {
        property string label: ""
        property color labelColor: "#FFFFFF"

        implicitHeight: 20
        implicitWidth: badgeText.implicitWidth + 14
        radius: 5
        color: "#99000000"
        border.width: 1
        border.color: "#2EFFFFFF"

        Text {
            id: badgeText
            anchors.centerIn: parent
            text: parent.label
            color: parent.labelColor
            font.pixelSize: 10
            font.weight: Font.Bold
            font.letterSpacing: 0.6
        }
    }

    Rectangle {
        id: card

        anchors.fill: parent
        anchors.margins: 7
        radius: 12
        clip: true
        scale: root.hovered ? 1.045 : 1.0

        Behavior on scale { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: root.isFolder ? Theme.surfaceHi : root.posterTop }
            GradientStop { position: 1.0; color: root.isFolder ? Theme.surface : root.posterBottom }
        }

        // Typographic poster artwork
        PosterArt {
            anchors.fill: parent
            seed: root.title
            bigText: root.bigText
            isFolder: root.isFolder
            hovered: root.hovered
        }

        // Soft top gloss
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: parent.height * 0.5
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#16FFFFFF" }
                GradientStop { position: 1.0; color: "#00FFFFFF" }
            }
        }

        // Bottom scrim for legibility
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: parent.height * 0.8
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 0.55; color: "#99000000" }
                GradientStop { position: 1.0; color: "#EB000000" }
            }
        }

        // Title block
        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            anchors.bottomMargin: root.progress > 0 ? 16 : 12
            spacing: 3

            Text {
                Layout.fillWidth: true
                visible: root.libraryName.length > 0
                text: root.libraryName.toUpperCase()
                color: root.isPlaying ? Theme.accentHover : "#B3FFFFFF"
                font.pixelSize: 9
                font.weight: Font.Bold
                font.letterSpacing: 1.1
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                text: root.parsed.title
                color: "#FFFFFF"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Text {
                Layout.fillWidth: true
                visible: root.infoLine.length > 0
                text: root.infoLine
                color: "#B8FFFFFF"
                font.pixelSize: 11
                elide: Text.ElideRight
            }
        }

        // Bookmarked marker (top-left)
        Rectangle {
            visible: root.bookmarked
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            width: 22
            height: 22
            radius: 11
            color: Theme.accent

            Text {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -1
                text: "\u2605"
                color: "#FFFFFF"
                font.pixelSize: 12
            }
        }

        // Quality / watched badges (top-left, shifted right of the bookmark marker)
        RowLayout {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 10
            anchors.leftMargin: root.bookmarked ? 38 : 10
            spacing: 6
            visible: !root.isFolder

            Badge {
                visible: root.meta.resolution.length > 0
                label: root.meta.resolution
                labelColor: root.meta.resolution === "4K" ? Theme.warning : "#FFFFFF"
            }

            Badge {
                visible: root.meta.hdr.length > 0
                label: root.meta.hdr
                labelColor: Theme.warning
            }

            Badge {
                visible: root.watched
                label: "\u2713 WATCHED"
                labelColor: Theme.success
            }
        }

        // Extension (top-right) / folder tag / live "playing" chip
        Badge {
            visible: !root.isPlaying && !root.isFolder && !root.showRemove && root.extension.length > 0
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            label: root.extension.toUpperCase()
            labelColor: "#CCFFFFFF"
        }

        Badge {
            visible: root.isFolder
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            label: "FOLDER"
            labelColor: "#CCFFFFFF"
        }

        Rectangle {
            visible: root.isPlaying
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 10
            implicitHeight: 22
            implicitWidth: playingRow.implicitWidth + 16
            radius: 11
            color: Theme.accent

            RowLayout {
                id: playingRow
                anchors.centerIn: parent
                spacing: 6

                Equalizer {
                    color: "#FFFFFF"
                    running: root.isPlaying
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: "PLAYING"
                    color: "#FFFFFF"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                    font.letterSpacing: 0.8
                }
            }
        }

        // Hover dim
        Rectangle {
            anchors.fill: parent
            color: "#000000"
            opacity: root.hovered ? 0.32 : 0.0

            Behavior on opacity { NumberAnimation { duration: 220 } }
        }

        // Hover action: play button (files) / open pill (folders). Pure shapes, no
        // Canvas, so nothing gets created at hover time.
        Rectangle {
            visible: !root.isFolder
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -10
            width: 54
            height: 54
            radius: 27
            color: "#F2FFFFFF"
            opacity: root.hovered ? 1.0 : 0.0
            scale: root.hovered ? 1.0 : 0.6

            Behavior on opacity { NumberAnimation { duration: 200 } }
            Behavior on scale { NumberAnimation { duration: 280; easing.type: Easing.OutBack } }

            // Play triangle = right half of a rotated square
            Item {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: 2
                width: 11
                height: 22
                clip: true

                Rectangle {
                    x: -7.75
                    y: 3.25
                    width: 15.5
                    height: 15.5
                    rotation: 45
                    antialiasing: true
                    color: Theme.bg
                }
            }
        }

        Rectangle {
            visible: root.isFolder
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 4
            implicitHeight: 34
            implicitWidth: openRow.implicitWidth + 30
            radius: 17
            color: "#F2FFFFFF"
            opacity: root.hovered ? 1.0 : 0.0
            scale: root.hovered ? 1.0 : 0.85

            Behavior on opacity { NumberAnimation { duration: 200 } }
            Behavior on scale { NumberAnimation { duration: 280; easing.type: Easing.OutBack } }

            RowLayout {
                id: openRow
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: "Open"
                    color: Theme.bg
                    font.pixelSize: 13
                    font.weight: Font.Bold
                }

                Text {
                    text: "\u203A"
                    color: Theme.bg
                    font.pixelSize: 18
                    font.weight: Font.Bold
                }
            }
        }

        // Watch progress bar
        Rectangle {
            visible: root.progress > 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 4
            color: "#4DFFFFFF"

            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.progress))
                height: parent.height
                color: Theme.accent
            }
        }

        // Now-playing accent
        Rectangle {
            visible: root.isPlaying && root.progress <= 0
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 3
            color: Theme.accent
        }

        // Frame
        Rectangle {
            anchors.fill: parent
            radius: card.radius
            color: "transparent"
            border.width: root.isPlaying ? 2 : 1
            border.color: root.isPlaying ? Theme.accent
                                         : (root.hovered ? "#66FFFFFF" : "#1FFFFFFF")

            Behavior on border.color { ColorAnimation { duration: 180 } }
        }
    }

    MouseArea {
        id: rowMouse
        anchors.fill: parent
        anchors.margins: 7
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onClicked: function(mouse) {
            if (mouse.button === Qt.RightButton) {
                root.openContextMenu(mouse.x, mouse.y)
            } else {
                root.activate()
            }
        }
    }

    // ---- Actions shared by a left click and the right-click menu -------------------------
    function activate() {
        if (root.isFolder) {
            if (root.playUrl.length > 0) root.folderRequested(root.playUrl, root.libraryName)
        } else if (root.playUrl.length > 0) {
            root.playRequested(root.playUrl, root.parsed.title)
        }
    }

    function toggleBookmark() {
        if (!fluxBookmarks || root.playUrl.length === 0) return
        fluxBookmarks.toggle(root.playUrl, root.title, root.isFolder, root.formattedSize,
                             root.extension, root.libraryName, root.parentPath)
    }

    function requestDownload() {
        if (root.playUrl.length === 0) return
        root.downloadRequested(root.playUrl, root.title, root.isFolder)
        root.dlFlash = true
        dlFlashTimer.restart()
    }

    function openContextMenu(x, y) {
        if (root.playUrl.length === 0) return
        ctxLoader.active = true
        if (ctxLoader.item) ctxLoader.item.popup(rowMouse, x, y)
    }

    // Created on first right-click, so cards that are never right-clicked carry no extra cost
    Loader {
        id: ctxLoader
        active: false

        sourceComponent: CardMenu {
            isFolder: root.isFolder
            bookmarked: root.bookmarked
            canDownload: root.playUrl.length > 0

            onOpenRequested: root.activate()
            onBookmarkToggled: root.toggleBookmark()
            onDownloadRequested: root.requestDownload()
        }
    }

    // Remove from "Continue Watching" (declared after rowMouse so it sits on top)
    Rectangle {
        visible: root.showRemove
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 17
        anchors.topMargin: 17
        width: 26
        height: 26
        radius: 13
        color: removeMouse.containsMouse ? "#E6E50914" : "#B3000000"
        border.width: 1
        border.color: "#40FFFFFF"
        opacity: root.hovered ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 180 } }
        Behavior on color { ColorAnimation { duration: 120 } }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            text: "\u00D7"
            color: "#FFFFFF"
            font.pixelSize: 18
            font.weight: Font.Bold
        }

        MouseArea {
            id: removeMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.removeRequested(root.playUrl)
        }
    }

    // Download (declared after rowMouse so it sits on top). Plain shapes + a text glyph, so no
    // Canvas is created per card.
    Rectangle {
        visible: root.showDownload && root.playUrl.length > 0
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.rightMargin: 17
        anchors.topMargin: 43
        width: 30
        height: 30
        radius: 15
        color: root.dlFlash ? Theme.success : (dlMouse.containsMouse ? "#F2FFFFFF" : "#CC000000")
        border.width: 1
        border.color: "#40FFFFFF"
        opacity: (root.hovered || root.dlFlash) ? 1.0 : 0.0
        scale: dlMouse.containsMouse ? 1.1 : 1.0

        Behavior on opacity { NumberAnimation { duration: 180 } }
        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on scale { NumberAnimation { duration: 120 } }

        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -1
            text: root.dlFlash ? "\u2713" : "\u2193"
            color: (dlMouse.containsMouse && !root.dlFlash) ? Theme.bg : "#FFFFFF"
            font.pixelSize: 17
            font.weight: Font.Bold
        }

        MouseArea {
            id: dlMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onClicked: {
                root.downloadRequested(root.playUrl, root.title, root.isFolder)
                root.dlFlash = true
                dlFlashTimer.restart()
            }
        }

        Timer {
            id: dlFlashTimer
            interval: 1600
            onTriggered: root.dlFlash = false
        }

        FluxToolTip {
            visible: dlMouse.containsMouse
            text: root.dlFlash ? "Added to downloads"
                               : (root.isFolder ? "Download whole folder" : "Download")
            above: false
        }
    }

    FluxToolTip {
        visible: rowMouse.containsMouse && root.title.length > 30
        delay: 700
        text: root.title + (root.parentPath.length > 0 ? ("\n" + root.parentPath) : "")
    }
}
