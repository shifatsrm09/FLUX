import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "MediaFormatter.js" as Formatter
import "Theme.js" as Theme

// Downloads popup (opened from the download button at the bottom-right).
//   Tab "Downloads"       : live queue (progress, cancel, retry ...)
//   Tab "Offline library" : everything already on disk; plays without any network
Popup {
    id: panel

    // Ask the app to play a file (a file:// URL when offline)
    signal playRequested(string url, string title)

    // Distance from the window edge (matches the dock)
    property real edgeMargin: 22
    property real bottomReserve: 82

    // 0 = downloads queue, 1 = offline library
    property int tab: 0
    // Folder being browsed in the offline library, relative to the download location ("" = top)
    property string offlineRel: ""
    // Bumped to force the offline list to re-read the disk
    property int refreshTick: 0

    readonly property real maxHeight: (parent ? parent.height : 600) - bottomReserve - 70

    // Offline entries: re-read when downloads finish, the location changes, or the tab opens
    readonly property var offlineEntries: {
        var rev = fluxDownloads ? fluxDownloads.offlineRevision : 0
        var tick = panel.refreshTick
        if (!panel.opened || panel.tab !== 1 || !fluxDownloads) return []
        return fluxDownloads.offlineList(panel.offlineRel)
    }

    function parentRel(rel) {
        var i = rel.lastIndexOf("/")
        return i < 0 ? "" : rel.substring(0, i)
    }

    function crumbText(rel) {
        if (rel.length === 0) return "Offline library"
        return "Offline library  \u203A  " + rel.split("/").join("  \u203A  ")
    }

    function showOffline() {
        tab = 1
        offlineRel = ""
        refreshTick++
    }

    parent: Overlay.overlay
    x: parent ? parent.width - width - edgeMargin : 0
    y: parent ? parent.height - height - bottomReserve : 0
    width: Math.min(460, (parent ? parent.width : 460) - 2 * edgeMargin)
    height: panel.tab === 1 ? Math.min(560, maxHeight)
                            : Math.min(Math.max(250, 176 + list.contentHeight), maxHeight)
    modal: false
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onAboutToShow: refreshTick++

    // A new download location means a different library
    Connections {
        target: fluxDownloads

        function onLocationChanged() { panel.offlineRel = "" }
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 }
        NumberAnimation { property: "y"; from: panel.y + 12; to: panel.y; duration: 200; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 }
    }

    background: Rectangle {
        radius: 18
        color: Theme.bgRaised
        border.width: 1
        border.color: Theme.borderHi
    }

    // Segmented tab button
    component TabButton: Rectangle {
        id: tabBtn

        property string label: ""
        property bool selected: false
        property int badge: 0
        signal clicked()

        implicitHeight: 32
        implicitWidth: tabRow.implicitWidth + 28
        radius: 16
        color: selected ? "#33FFFFFF" : (tabMouse.containsMouse ? "#1FFFFFFF" : "transparent")

        Behavior on color { ColorAnimation { duration: 120 } }

        Row {
            id: tabRow
            anchors.centerIn: parent
            spacing: 6

            Text {
                text: tabBtn.label
                color: tabBtn.selected ? Theme.text : Theme.textDim
                font.pixelSize: 13
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }

            Rectangle {
                visible: tabBtn.badge > 0
                width: Math.max(18, badgeLabel.implicitWidth + 10)
                height: 18
                radius: 9
                color: Theme.accent
                anchors.verticalCenter: parent.verticalCenter

                Text {
                    id: badgeLabel
                    anchors.centerIn: parent
                    text: tabBtn.badge
                    color: "#FFFFFF"
                    font.pixelSize: 10
                    font.weight: Font.Bold
                }
            }
        }

        MouseArea {
            id: tabMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tabBtn.clicked()
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Header ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 12
            Layout.topMargin: 16
            Layout.bottomMargin: 10
            spacing: 8

            FluxIcon {
                name: "download"
                size: 20
                color: Theme.text
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: "Downloads"
                color: Theme.text
                font.pixelSize: 17
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            IconButton {
                iconName: "close"
                iconSize: 16
                implicitWidth: 32
                implicitHeight: 32
                tip: "Close"
                tipAbove: false
                onClicked: panel.close()
            }
        }

        // ---- Tabs ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 14
            Layout.rightMargin: 14
            Layout.bottomMargin: 10
            spacing: 4

            TabButton {
                label: "Downloads"
                badge: fluxDownloads ? fluxDownloads.activeCount : 0
                selected: panel.tab === 0
                onClicked: panel.tab = 0
            }

            TabButton {
                label: "Offline library"
                selected: panel.tab === 1
                onClicked: panel.showOffline()
            }

            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        // =====================================================================
        // TAB 0: download queue
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: panel.tab === 0

            ListView {
                id: list

                anchors.fill: parent
                clip: true
                model: fluxDownloads
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: FluxScrollBar { }

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
                }
                displaced: Transition {
                    NumberAnimation { properties: "y"; duration: 200; easing.type: Easing.OutCubic }
                }

                delegate: Item {
                    id: row

                    required property int jobId
                    required property string jobTitle
                    required property string kind
                    required property string category
                    required property string status
                    required property real progress
                    required property string detail
                    required property bool isActive

                    readonly property bool isFailed: status === "failed"
                    readonly property bool isDone: status === "done"
                    readonly property bool canRetry: status === "failed" || status === "cancelled"

                    width: ListView.view.width
                    height: 78

                    Rectangle {
                        anchors.fill: parent
                        color: rowHover.hovered ? "#12FFFFFF" : "transparent"
                    }

                    HoverHandler { id: rowHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 12
                        spacing: 12

                        // Kind badge
                        Rectangle {
                            implicitWidth: 38
                            implicitHeight: 38
                            radius: 10
                            color: row.isFailed ? "#26FF5D5D" : (row.isDone ? "#2646D369" : Theme.surfaceHi)
                            Layout.alignment: Qt.AlignVCenter

                            FluxIcon {
                                anchors.centerIn: parent
                                name: row.isFailed ? "alert" : (row.isDone ? "check" : (row.kind === "pack" ? "folder" : "download"))
                                size: 18
                                color: row.isFailed ? Theme.danger : (row.isDone ? Theme.success : Theme.textDim)
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            spacing: 3

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                Text {
                                    text: row.jobTitle
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: row.category
                                    color: Theme.textMute
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    font.letterSpacing: 0.8
                                }
                            }

                            Text {
                                text: row.detail
                                color: row.isFailed ? Theme.danger : Theme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Progress bar
                            Rectangle {
                                id: track

                                Layout.fillWidth: true
                                implicitHeight: 4
                                radius: 2
                                color: "#2EFFFFFF"
                                visible: row.status !== "cancelled"

                                Rectangle {
                                    width: row.status === "scanning" ? 0 : parent.width * Math.max(0, Math.min(1, row.progress))
                                    height: parent.height
                                    radius: 2
                                    color: row.isFailed ? Theme.danger : (row.isDone ? Theme.success : Theme.accent)

                                    Behavior on width { NumberAnimation { duration: 180 } }
                                }

                                // Indeterminate shimmer while scanning a folder
                                Rectangle {
                                    visible: row.status === "scanning"
                                    width: parent.width * 0.3
                                    height: parent.height
                                    radius: 2
                                    color: Theme.accent

                                    SequentialAnimation on x {
                                        running: row.status === "scanning"
                                        loops: Animation.Infinite

                                        NumberAnimation { from: 0; to: track.width * 0.7; duration: 900; easing.type: Easing.InOutSine }
                                        NumberAnimation { from: track.width * 0.7; to: 0; duration: 900; easing.type: Easing.InOutSine }
                                    }
                                }
                            }
                        }

                        // Actions
                        IconButton {
                            visible: row.isActive
                            iconName: "close"
                            iconSize: 16
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Cancel"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.cancel(row.jobId)
                        }

                        IconButton {
                            visible: row.canRetry
                            iconName: "refresh"
                            iconSize: 16
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Retry (resumes where it stopped)"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.retry(row.jobId)
                        }

                        IconButton {
                            visible: row.isDone
                            iconName: "folder"
                            iconSize: 16
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Show in folder"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.openFolder(row.jobId)
                        }

                        IconButton {
                            visible: !row.isActive
                            iconName: "close"
                            iconSize: 14
                            iconColor: Theme.textDim
                            implicitWidth: 30
                            implicitHeight: 30
                            tip: "Remove from list"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.remove(row.jobId)
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
                    visible: list.count === 0

                    FluxIcon {
                        name: "download"
                        size: 30
                        color: Theme.textMute
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "No downloads yet"
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Hover over a movie, episode or folder and click its download button."
                        color: Theme.textMute
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }

                    FluxButton {
                        text: "Open offline library"
                        variant: "secondary"
                        implicitHeight: 36
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 6
                        onClicked: panel.showOffline()
                    }
                }
            }
        }

        // =====================================================================
        // TAB 1: offline library (plays from disk, no network needed)
        // =====================================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: panel.tab === 1
            spacing: 0

            // Location bar: back + breadcrumb + refresh
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 14
                Layout.rightMargin: 14
                Layout.topMargin: 10
                Layout.bottomMargin: 6
                spacing: 6

                IconButton {
                    iconName: "back"
                    iconSize: 16
                    implicitWidth: 32
                    implicitHeight: 32
                    filled: true
                    visible: panel.offlineRel.length > 0
                    tip: "Back"
                    tipAbove: false
                    onClicked: panel.offlineRel = panel.parentRel(panel.offlineRel)
                }

                Text {
                    text: panel.crumbText(panel.offlineRel)
                    color: Theme.textDim
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    elide: Text.ElideLeft
                    Layout.fillWidth: true
                    Layout.leftMargin: panel.offlineRel.length > 0 ? 0 : 6
                }

                IconButton {
                    iconName: "refresh"
                    iconSize: 15
                    implicitWidth: 32
                    implicitHeight: 32
                    tip: "Refresh"
                    tipAbove: false
                    onClicked: panel.refreshTick++
                }

                IconButton {
                    iconName: "folder"
                    iconSize: 16
                    implicitWidth: 32
                    implicitHeight: 32
                    tip: "Open this folder in Explorer"
                    tipAbove: false
                    onClicked: {
                        if (!fluxDownloads) return
                        // Top level = the download location itself; deeper = that sub folder
                        if (panel.offlineRel.length === 0) {
                            fluxDownloads.openRoot()
                        } else {
                            fluxDownloads.openPath(fluxDownloads.downloadLocation + "\\"
                                                   + panel.offlineRel.split("/").join("\\"))
                        }
                    }
                }
            }

            ListView {
                id: offlineList

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: panel.offlineEntries
                boundsBehavior: Flickable.StopAtBounds

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
                        if (meta.episode && meta.episode.length > 0) parts.push(meta.episode)
                        if (meta.resolution && meta.resolution.length > 0) parts.push(meta.resolution)
                        parts.push(modelData.detail)
                        if (watched) parts.push("Watched")
                        return parts.join("  \u00B7  ")
                    }

                    width: ListView.view.width
                    height: 62

                    Rectangle {
                        anchors.fill: parent
                        color: (orowMouse.containsMouse || openFolderBtn.hovered) ? "#14FFFFFF" : "transparent"
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 10
                        spacing: 12

                        Rectangle {
                            implicitWidth: 38
                            implicitHeight: 38
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
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideMiddle
                                Layout.fillWidth: true
                            }

                            Text {
                                text: orow.subtitle
                                color: Theme.textDim
                                font.pixelSize: 11
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
                            implicitWidth: 32
                            implicitHeight: 32
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
                        anchors.rightMargin: orow.isVideo ? 52 : 0
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor

                        onClicked: {
                            if (orow.isVideo) {
                                panel.playRequested(modelData.url, orow.parsed.title)
                            } else {
                                panel.offlineRel = modelData.rel
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
                    visible: offlineList.count === 0

                    FluxIcon {
                        name: "folder"
                        size: 30
                        color: Theme.textMute
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: panel.offlineRel.length === 0 ? "Nothing downloaded yet" : "No videos in this folder"
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Downloaded movies and series show up here and play without an internet connection."
                        color: Theme.textMute
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        // ---- Footer ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 8

            FluxButton {
                text: "Open downloads folder"
                variant: "secondary"
                implicitHeight: 38
                onClicked: if (fluxDownloads) fluxDownloads.openRoot()
            }

            Item { Layout.fillWidth: true }

            FluxButton {
                text: "Clear finished"
                variant: "ghost"
                implicitHeight: 38
                visible: panel.tab === 0 && !!fluxDownloads && fluxDownloads.hasFinished
                onClicked: fluxDownloads.clearFinished()
            }
        }
    }
}
