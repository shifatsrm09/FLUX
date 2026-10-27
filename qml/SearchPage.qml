import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "components/Theme.js" as Theme
import "components/TopPicksData.js" as TopPicksData

Item {
    id: root

    // Space reserved at the top for the overlaid nav bar
    property real topInset: 0

    signal playMediaRequested(var url, var title, var altUrl)

    function watchedTimeLabel(milliseconds) {
        var totalSeconds = Math.floor(milliseconds / 1000)
        var totalMinutes = Math.floor(milliseconds / 60000)
        var hours = Math.floor(totalMinutes / 60)
        var minutes = totalMinutes % 60
        if (hours > 0) return "Watched " + hours + "h " + minutes + "m"
        if (totalMinutes === 0) return "Watched " + totalSeconds + " sec"
        return "Watched " + totalMinutes + " min"
    }

    // Downloads: a single file goes to Individuals, a folder (pack) goes to Series
    function startDownload(url, title, isPack) {
        if (!fluxDownloads) return
        if (isPack) {
            fluxDownloads.downloadPack(url, title)
        } else if (url && (url.indexOf("player.php") !== -1 || url.indexOf("/api/posts/") !== -1) && fluxSearch) {
            fluxSearch.resolvePlayUrl(url, function(directUrl) {
                if (directUrl && directUrl.length > 0) {
                    fluxDownloads.downloadFile(directUrl, title)
                } else {
                    fluxDownloads.downloadFile(url, title)
                }
            })
        } else {
            fluxDownloads.downloadFile(url, title)
        }
    }

    readonly property bool hasSearched: (!!fluxSearch && (fluxSearch.hasResults || fluxSearch.isSearching || fluxSearch.query.length > 0))
    readonly property bool searching: (!!fluxSearch && fluxSearch.isSearching)
    readonly property bool browsing: (!!fluxBrowser && fluxBrowser.active)

    // Navigation view states
    property bool bookmarksOpen: false
    property bool searchOpen: false
    property bool showAllTrending: false
    property bool showAllTopPicks: false

    // Exactly one of the four views is shown at a time
    readonly property bool showBookmarks: root.bookmarksOpen && !root.browsing
    readonly property bool showSearch: (root.searchOpen || root.hasSearched) && !root.browsing && !root.bookmarksOpen
    readonly property bool showHome: !root.showSearch && !root.browsing && !root.bookmarksOpen
    readonly property bool showResults: root.showSearch
    readonly property bool showBrowser: root.browsing

    readonly property bool navSolid: root.showSearch || root.browsing || root.bookmarksOpen || homeFlick.contentY > 24
    readonly property real pageMargin: Math.max(32, Math.round((root.width - 1240) / 2))

    onShowSearchChanged: {
        if (showSearch) {
            topSearchInput.forceActiveFocus()
        }
    }

    // Shimmering placeholder tiles shown while a listing loads
    component SkeletonFlow: Flow {
        id: skel

        property bool active: false

        readonly property int cols: Math.max(2, Math.floor((width + 14) / (172 + 14)))
        readonly property real cellW: Math.floor((width - (cols - 1) * 14) / cols)

        spacing: 14

        Repeater {
            model: 12

            delegate: Rectangle {
                width: skel.cellW
                height: Math.round(skel.cellW * 1.5)
                radius: 12
                color: Theme.surface

                SequentialAnimation on opacity {
                    running: skel.active
                    loops: Animation.Infinite

                    NumberAnimation { from: 0.35; to: 0.85; duration: 800; easing.type: Easing.InOutSine }
                    NumberAnimation { from: 0.85; to: 0.35; duration: 800; easing.type: Easing.InOutSine }
                }
            }
        }
    }

    // =========================================================================
    // VIEW A: Home: search bar, Continue Watching, category columns
    // =========================================================================
    Flickable {
        id: homeFlick

        anchors.fill: parent
        opacity: root.showHome ? 1.0 : 0.0
        visible: opacity > 0.0
        clip: true
        contentWidth: width
        contentHeight: homeColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        transform: Translate {
            y: root.showHome ? 0 : 14

            Behavior on y { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
        }

        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        ScrollBar.vertical: FluxScrollBar { }

        Item {
            id: homeColumn
            width: homeFlick.width
            implicitHeight: (continueSection.visible ? continueSection.height : 0)
                            + (trendingSection.visible ? trendingSection.height : 0)
                            + topPicksSection.height
                            + 48

            readonly property real availableWidth: Math.max(300, width - root.pageMargin * 2)
            readonly property int targetCardW: 172
            readonly property int cols: Math.max(2, Math.floor((availableWidth + 14) / (targetCardW + 14)))
            readonly property real cardW: Math.floor((availableWidth - (cols - 1) * 14) / cols)
            readonly property real cardH: Math.round(cardW * 1.5)
            readonly property real continueOffset: continueSection.visible ? continueSection.height : 0

            // ---- Continue Watching -------------------------------------------------------
            Item {
                id: continueSection

                y: 0
                width: homeColumn.width
                height: continueList.count > 0 ? (root.topInset + 16 + continueHeader.height + 6 + continueList.height + 24) : 0
                visible: continueList.count > 0

                RowLayout {
                    id: continueHeader

                    x: root.pageMargin
                    y: root.topInset + 16
                    width: parent.width - root.pageMargin * 2
                    height: 40

                    Text {
                        text: "Continue Watching"
                        color: Theme.text
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        font.letterSpacing: -0.3
                        Layout.fillWidth: true
                    }

                    IconButton {
                        iconName: "chevronLeft"
                        iconSize: 18
                        implicitWidth: 36
                        implicitHeight: 36
                        filled: true
                        tip: "Scroll left"
                        tipAbove: false
                        visible: continueList.contentWidth > continueList.width

                        onClicked: continueList.scrollBy(-continueList.width * 0.8)
                    }

                    IconButton {
                        iconName: "chevronRight"
                        iconSize: 18
                        implicitWidth: 36
                        implicitHeight: 36
                        filled: true
                        tip: "Scroll right"
                        tipAbove: false
                        visible: continueList.contentWidth > continueList.width

                        onClicked: continueList.scrollBy(continueList.width * 0.8)
                    }
                }

                ListView {
                    id: continueList

                    x: root.pageMargin - 7
                    y: continueHeader.y + continueHeader.height + 6
                    width: parent.width - root.pageMargin * 2 + 14
                    height: homeColumn.cardH
                    orientation: ListView.Horizontal
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: {
                        var items = fluxUser ? fluxUser.continueWatching : []
                        return items.filter(function(item) { return item.isFolder !== true })
                    }

                    function scrollBy(dx) {
                        scrollAnim.stop()
                        scrollAnim.from = continueList.contentX
                        scrollAnim.to = Math.max(0, Math.min(continueList.contentWidth - continueList.width,
                                                             continueList.contentX + dx))
                        scrollAnim.start()
                    }

                    NumberAnimation {
                        id: scrollAnim
                        target: continueList
                        property: "contentX"
                        duration: 420
                        easing.type: Easing.OutCubic
                    }

                    delegate: SearchResultCard {
                        required property var modelData
                        required property int index

                        width: homeColumn.cardW
                        height: homeColumn.cardH
                        title: modelData.title
                        playUrl: modelData.url
                        posterUrl: modelData.posterUrl || ""
                        isFolder: modelData.isFolder === true
                        progress: modelData.fraction
                        watched: modelData.watched === true
                        showRemove: modelData.isFolder !== true
                        extraInfo: (modelData.positionMs > 0) ? root.watchedTimeLabel(modelData.positionMs) : (modelData.watched === true ? "Completed" : "")
                        introDelay: Math.min(index, 8) * 45

                        onPlayRequested: function(url, itemTitle) {
                            root.playMediaRequested(url, itemTitle, "")
                        }

                        onFolderRequested: function(url, libName) {
                            if (fluxBrowser) fluxBrowser.open(url, libName, "")
                        }

                        onRemoveRequested: function(url) {
                            if (fluxUser) fluxUser.removeFromHistory(url)
                        }

                        onDownloadRequested: function(url, itemTitle, isPack) {
                            root.startDownload(url, itemTitle, isPack)
                        }
                    }
                }
            }

            // ---- Trending Right Now ----------------------------------------------------
            Item {
                id: trendingSection

                width: homeColumn.width
                visible: (fluxUser ? (fluxUser.server2Enabled || fluxUser.allServersDown) : true) && !!fluxSearch && fluxSearch.trending && fluxSearch.trending.length > 0
                height: visible ? (trendingLayout.y + trendingLayout.implicitHeight + 16) : 0
                implicitHeight: height

                y: {
                    if (fluxUser && fluxUser.topPicksFirst) {
                        return homeColumn.continueOffset + topPicksSection.height
                    } else {
                        return homeColumn.continueOffset
                    }
                }
                Behavior on y {
                    NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
                }

                readonly property real availableWidth: homeColumn.availableWidth
                readonly property int trendingCols: homeColumn.cols
                readonly property real trendingCardW: homeColumn.cardW
                readonly property real trendingCardH: homeColumn.cardH

                ColumnLayout {
                    id: trendingLayout
                    x: root.pageMargin
                    y: {
                        if (continueList.count > 0) return 16
                        var isTop = !(fluxUser && fluxUser.topPicksFirst)
                        return isTop ? (root.topInset + 16) : 12
                    }
                    width: trendingSection.availableWidth
                    spacing: 16

                    // Header row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: "Trending Right Now"
                            color: Theme.text
                            font.pixelSize: 22
                            font.weight: Font.Bold
                            font.letterSpacing: -0.3
                        }

                        Rectangle {
                            height: 20
                            radius: 10
                            color: Theme.accentSoft
                            border.width: 1
                            border.color: Theme.accentRing
                            implicitWidth: badgeText.implicitWidth + 14

                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: "ICC FTP"
                                color: Theme.accent
                                font.pixelSize: 11
                                font.weight: Font.Bold
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            visible: !!fluxSearch && fluxSearch.trending && fluxSearch.trending.length > 0
                            text: root.showAllTrending
                                ? ("Showing all " + ((fluxSearch && fluxSearch.trending) ? fluxSearch.trending.length : 0))
                                : ("Top " + Math.min(((fluxSearch && fluxSearch.trending) ? fluxSearch.trending.length : 0), trendingSection.trendingCols * 2))
                            color: Theme.textDim
                            font.pixelSize: 12
                        }

                        // Swap icon button sharp right
                        Rectangle {
                            id: trendingSwapBtn
                            width: 30
                            height: 30
                            radius: 8
                            color: trendingSwapMouse.containsMouse ? Theme.surfaceHover : "transparent"
                            border.width: 1
                            border.color: trendingSwapMouse.containsMouse ? Theme.border : "transparent"

                            FluxIcon {
                                anchors.centerIn: parent
                                name: "swap"
                                size: 16
                                color: trendingSwapMouse.containsMouse ? Theme.text : Theme.textDim
                            }

                            MouseArea {
                                id: trendingSwapMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (fluxUser) {
                                        fluxUser.topPicksFirst = !fluxUser.topPicksFirst
                                    }
                                }
                            }
                        }
                    }

                    // Skeleton loader while fetching
                    Grid {
                        Layout.fillWidth: true
                        columns: trendingSection.trendingCols
                        columnSpacing: 14
                        rowSpacing: 14
                        visible: (fluxSearch && fluxSearch.isTrendingLoading && (!fluxSearch.trending || fluxSearch.trending.length === 0))

                        Repeater {
                            model: trendingSection.trendingCols * 2
                            delegate: Rectangle {
                                width: trendingSection.trendingCardW
                                height: trendingSection.trendingCardH
                                radius: 12
                                color: Theme.surface

                                SequentialAnimation on opacity {
                                    loops: Animation.Infinite
                                    NumberAnimation { from: 0.35; to: 0.85; duration: 800; easing.type: Easing.InOutSine }
                                    NumberAnimation { from: 0.85; to: 0.35; duration: 800; easing.type: Easing.InOutSine }
                                }
                            }
                        }
                    }

                    // Grid of tall rectangular cards (fitting width, 2 rows by default or all when expanded)
                    Grid {
                        id: trendingGrid
                        Layout.fillWidth: true
                        columns: trendingSection.trendingCols
                        columnSpacing: 14
                        rowSpacing: 14
                        visible: !(fluxSearch && fluxSearch.isTrendingLoading && (!fluxSearch.trending || fluxSearch.trending.length === 0))

                        Repeater {
                            model: {
                                var all = (fluxSearch && fluxSearch.trending) ? fluxSearch.trending : []
                                if (!root.showAllTrending) {
                                    return all.slice(0, Math.min(all.length, trendingSection.trendingCols * 2))
                                }
                                return all
                            }

                            delegate: SearchResultCard {
                                required property var modelData
                                required property int index

                                width: trendingSection.trendingCardW
                                height: trendingSection.trendingCardH
                                title: modelData.title
                                playUrl: modelData.url
                                posterUrl: modelData.posterUrl || ""
                                isFolder: modelData.isFolder === true
                                progress: 0
                                showRemove: false
                                extraInfo: modelData.extraInfo || (modelData.isFolder ? "Series" : "Trending")
                                introDelay: Math.min(index, 12) * 30

                                onPlayRequested: function(url, itemTitle) {
                                    root.playMediaRequested(url, itemTitle, "")
                                }

                                onFolderRequested: function(url, libName) {
                                    if (fluxBrowser) fluxBrowser.open(url, libName, "")
                                }

                                onDownloadRequested: function(url, itemTitle, isPack) {
                                    root.startDownload(url, itemTitle, isPack)
                                }
                            }
                        }
                    }

                    // "Show More" / "Show Less" button
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        Layout.bottomMargin: 8
                        visible: (fluxSearch && fluxSearch.trending && fluxSearch.trending.length > trendingSection.trendingCols * 2)

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            id: showMoreBtn
                            implicitHeight: 40
                            implicitWidth: showMoreRow.implicitWidth + 36
                            radius: 20
                            color: showMoreMouse.containsMouse ? Theme.surfaceHi : Theme.surface
                            border.width: 1
                            border.color: showMoreMouse.containsMouse ? Theme.accent : Theme.border

                            Behavior on color { ColorAnimation { duration: 120 } }
                            Behavior on border.color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                id: showMoreRow
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: root.showAllTrending ? "Show Less" : "Show More"
                                    color: showMoreMouse.containsMouse ? "#FFFFFF" : Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }

                                FluxIcon {
                                    name: root.showAllTrending ? "chevronUp" : "chevronDown"
                                    size: 14
                                    color: showMoreMouse.containsMouse ? Theme.accent : Theme.textDim
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }

                            MouseArea {
                                id: showMoreMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.showAllTrending = !root.showAllTrending
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }

            // ---- Top Picks -------------------------------------------------------------
            Item {
                id: topPicksSection

                width: homeColumn.width
                height: topPicksLayout.y + topPicksLayout.implicitHeight + 16
                implicitHeight: height

                y: {
                    if (fluxUser && fluxUser.topPicksFirst) {
                        return homeColumn.continueOffset
                    } else {
                        return homeColumn.continueOffset + (trendingSection.visible ? trendingSection.height : 0)
                    }
                }
                Behavior on y {
                    NumberAnimation { duration: 320; easing.type: Easing.OutCubic }
                }

                readonly property real availableWidth: homeColumn.availableWidth
                readonly property int picksCols: homeColumn.cols
                readonly property real picksCardW: homeColumn.cardW
                readonly property real picksCardH: homeColumn.cardH

                readonly property var rawTopPicks: TopPicksData.topPicks

                property var topPicksList: {
                    var arr = rawTopPicks.slice()
                    for (var i = arr.length - 1; i > 0; i--) {
                        var j = Math.floor(Math.random() * (i + 1))
                        var temp = arr[i]
                        arr[i] = arr[j]
                        arr[j] = temp
                    }
                    return arr
                }

                Component.onCompleted: {
                    if (fluxUser) {
                        for (var i = 0; i < topPicksList.length; ++i) {
                            fluxUser.rememberPoster(topPicksList[i].url, topPicksList[i].posterUrl, topPicksList[i].title)
                            if (topPicksList[i].altUrl && topPicksList[i].altUrl.length > 0) {
                                fluxUser.rememberPoster(topPicksList[i].altUrl, topPicksList[i].posterUrl, topPicksList[i].title)
                            }
                            if (topPicksList[i].circleUrl && topPicksList[i].circleUrl.length > 0) {
                                fluxUser.rememberPoster(topPicksList[i].circleUrl, topPicksList[i].posterUrl, topPicksList[i].title)
                            }
                        }
                    }
                }

                ColumnLayout {
                    id: topPicksLayout
                    x: root.pageMargin
                    y: {
                        if (continueList.count > 0) return 16
                        var isTop = (fluxUser && fluxUser.topPicksFirst) || (!trendingSection.visible)
                        return isTop ? (root.topInset + 16) : 12
                    }
                    width: topPicksSection.availableWidth
                    spacing: 16

                    // Header row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text {
                            text: "Top Picks"
                            color: Theme.text
                            font.pixelSize: 22
                            font.weight: Font.Bold
                            font.letterSpacing: -0.3
                        }

                        Rectangle {
                            height: 20
                            radius: 10
                            color: Theme.accentSoft
                            border.width: 1
                            border.color: Theme.accentRing
                            implicitWidth: topPicksBadgeText.implicitWidth + 14

                            Text {
                                id: topPicksBadgeText
                                anchors.centerIn: parent
                                text: {
                                    if (fluxUser) {
                                        var cur = fluxUser.activeServer
                                        var s1Ok = fluxUser.server1Enabled && fluxUser.server1Available
                                        var s2Ok = fluxUser.server2Enabled && fluxUser.server2Available
                                        var s3Ok = fluxUser.server3Enabled && fluxUser.server3Available

                                        if (cur === 1 && s1Ok) return "Dhaka Flix"
                                        if (cur === 2 && s2Ok) return "ICC FTP"
                                        if (cur === 3 && s3Ok) return "Circle FTP"

                                        if (s1Ok) return "Dhaka Flix"
                                        if (s2Ok) return "ICC FTP"
                                        if (s3Ok) return "Circle FTP"
                                    }
                                    return "Unavailable"
                                }
                                color: Theme.accent
                                font.pixelSize: 11
                                font.weight: Font.Bold
                            }
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: root.showAllTopPicks
                                ? ("Showing all " + topPicksSection.topPicksList.length)
                                : ("Top " + Math.min(topPicksSection.topPicksList.length, topPicksSection.picksCols * 2) + " of " + topPicksSection.topPicksList.length)
                            color: Theme.textDim
                            font.pixelSize: 12
                        }

                        // Swap icon button sharp right (visible only when trendingSection is visible)
                        Rectangle {
                            id: topPicksSwapBtn
                            width: 30
                            height: 30
                            radius: 8
                            visible: trendingSection.visible
                            color: topPicksSwapMouse.containsMouse ? Theme.surfaceHover : "transparent"
                            border.width: 1
                            border.color: topPicksSwapMouse.containsMouse ? Theme.border : "transparent"

                            FluxIcon {
                                anchors.centerIn: parent
                                name: "swap"
                                size: 16
                                color: topPicksSwapMouse.containsMouse ? Theme.text : Theme.textDim
                            }

                            MouseArea {
                                id: topPicksSwapMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (fluxUser) {
                                        fluxUser.topPicksFirst = !fluxUser.topPicksFirst
                                    }
                                }
                            }
                        }
                    }

                    // Grid of cards (showing 2 rows by default or all when expanded)
                    Grid {
                        id: topPicksGrid
                        Layout.fillWidth: true
                        columns: topPicksSection.picksCols
                        columnSpacing: 14
                        rowSpacing: 14

                        Repeater {
                            model: {
                                if (!root.showAllTopPicks) {
                                    return topPicksSection.topPicksList.slice(0, Math.min(topPicksSection.topPicksList.length, topPicksSection.picksCols * 2))
                                }
                                return topPicksSection.topPicksList
                            }

                            delegate: SearchResultCard {
                                required property var modelData
                                required property int index

                                readonly property int chosenServer: {
                                    if (fluxUser) {
                                        var s1On = fluxUser.server1Enabled && fluxUser.server1Available
                                        var s2On = fluxUser.server2Enabled && fluxUser.server2Available
                                        var s3On = fluxUser.server3Enabled && fluxUser.server3Available
                                        var cur = fluxUser.activeServer

                                        if (cur === 1 && s1On && modelData.url && modelData.url.length > 0) return 1
                                        if (cur === 2 && s2On && modelData.altUrl && modelData.altUrl.length > 0) return 2
                                        if (cur === 3 && s3On && modelData.circleUrl && modelData.circleUrl.length > 0) return 3

                                        if (s1On && modelData.url && modelData.url.length > 0) return 1
                                        if (s2On && modelData.altUrl && modelData.altUrl.length > 0) return 2
                                        if (s3On && modelData.circleUrl && modelData.circleUrl.length > 0) return 3
                                    }
                                    return 0
                                }
                                readonly property string effectiveUrl: {
                                    if (chosenServer === 1) return modelData.url || ""
                                    if (chosenServer === 2) return modelData.altUrl || ""
                                    if (chosenServer === 3) return modelData.circleUrl || ""
                                    return ""
                                }
                                readonly property string fallbackUrl: {
                                    if (fluxUser) {
                                        var s1On = fluxUser.server1Enabled && fluxUser.server1Available
                                        var s2On = fluxUser.server2Enabled && fluxUser.server2Available
                                        var s3On = fluxUser.server3Enabled && fluxUser.server3Available

                                        if (chosenServer === 1) {
                                            if (s2On && modelData.altUrl) return modelData.altUrl
                                            if (s3On && modelData.circleUrl) return modelData.circleUrl
                                        } else if (chosenServer === 2) {
                                            if (s1On && modelData.url) return modelData.url
                                            if (s3On && modelData.circleUrl) return modelData.circleUrl
                                        } else if (chosenServer === 3) {
                                            if (s1On && modelData.url) return modelData.url
                                            if (s2On && modelData.altUrl) return modelData.altUrl
                                        }
                                    }
                                    return ""
                                }

                                width: topPicksSection.picksCardW
                                height: topPicksSection.picksCardH
                                title: modelData.title
                                playUrl: effectiveUrl
                                posterUrl: modelData.posterUrl || ""
                                isFolder: false
                                libraryName: chosenServer === 3 ? "CIRCLE FTP" : (chosenServer === 2 ? "ICC FTP" : (chosenServer === 1 ? "DHAKA FLIX" : "UNAVAILABLE"))
                                progress: 0
                                showRemove: false
                                extraInfo: modelData.extraInfo || "Top Pick"
                                introDelay: Math.min(index, 12) * 30

                                onPlayRequested: function(targetUrl, itemTitle) {
                                    if (!targetUrl || targetUrl.length === 0) {
                                        if (window && window.showNotification) {
                                            window.showNotification("Media unavailable on enabled servers. Check Media Server Settings.")
                                        }
                                        return
                                    }
                                    root.playMediaRequested(targetUrl, itemTitle, fallbackUrl)
                                }

                                onFolderRequested: function(url, libName) {
                                    if (fluxBrowser) fluxBrowser.open(url, libName, "")
                                }

                                onDownloadRequested: function(targetUrl, itemTitle, isPack) {
                                    root.startDownload(targetUrl, itemTitle, isPack)
                                }
                            }
                        }
                    }

                    // "Show More" / "Show Less" button
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: 8
                        Layout.bottomMargin: 8
                        visible: topPicksSection.topPicksList.length > topPicksSection.picksCols * 2

                        Item { Layout.fillWidth: true }

                        Rectangle {
                            id: showMoreTopPicksBtn
                            implicitHeight: 40
                            implicitWidth: showMoreTopPicksRow.implicitWidth + 36
                            radius: 20
                            color: showMoreTopPicksMouse.containsMouse ? Theme.surfaceHi : Theme.surface
                            border.width: 1
                            border.color: showMoreTopPicksMouse.containsMouse ? Theme.accent : Theme.border

                            Behavior on color { ColorAnimation { duration: 120 } }
                            Behavior on border.color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                id: showMoreTopPicksRow
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: root.showAllTopPicks ? "Show Less" : ("Show More (" + (topPicksSection.topPicksList.length - topPicksSection.picksCols * 2) + " more)")
                                    color: showMoreTopPicksMouse.containsMouse ? "#FFFFFF" : Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                }

                                FluxIcon {
                                    name: root.showAllTopPicks ? "chevronUp" : "chevronDown"
                                    size: 14
                                    color: showMoreTopPicksMouse.containsMouse ? Theme.accent : Theme.textDim
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }

                            MouseArea {
                                id: showMoreTopPicksMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.showAllTopPicks = !root.showAllTopPicks
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
    }

    // =========================================================================
    // VIEW B: Search results
    // =========================================================================
    ColumnLayout {
        id: resultsView

        anchors.fill: parent
        anchors.topMargin: root.topInset + 10
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        spacing: 16
        opacity: root.showResults ? 1.0 : 0.0
        visible: opacity > 0.0

        transform: Translate {
            y: root.showResults ? 0 : 14

            Behavior on y { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
        }

        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        // 1. Server selection in the center horizontally
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            Layout.bottomMargin: 2

            Item { Layout.fillWidth: true }

            Rectangle {
                id: searchServerSelector

                readonly property int activeServerIndex: {
                    if (!fluxUser) return 1
                    var cur = fluxUser.activeServer
                    var s1Ok = fluxUser.server1Enabled && fluxUser.server1Available
                    var s2Ok = fluxUser.server2Enabled && fluxUser.server2Available
                    var s3Ok = fluxUser.server3Enabled && fluxUser.server3Available
                    if (cur === 1 && s1Ok) return 1
                    if (cur === 2 && s2Ok) return 2
                    if (cur === 3 && s3Ok) return 3
                    if (s1Ok) return 1
                    if (s2Ok) return 2
                    if (s3Ok) return 3
                    return 1
                }
                readonly property bool isWide: root.width >= 750
                readonly property int enabledCount: fluxUser ? fluxUser.enabledServerCount : 3

                visible: enabledCount > 0
                implicitHeight: 42
                implicitWidth: Math.max(80, searchSegmentsRow.width + 8)
                radius: 21
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                readonly property var currentActiveSegment: {
                    if (activeServerIndex === 1 && sseg1.visible) return sseg1
                    if (activeServerIndex === 2 && sseg2.visible) return sseg2
                    if (activeServerIndex === 3 && sseg3.visible) return sseg3
                    if (sseg1.visible) return sseg1
                    if (sseg2.visible) return sseg2
                    if (sseg3.visible) return sseg3
                    return null
                }

                // Active sliding pill
                Rectangle {
                    height: 34
                    radius: 17
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.accent
                    visible: searchServerSelector.currentActiveSegment !== null
                    x: searchServerSelector.currentActiveSegment ? (searchSegmentsRow.x + searchServerSelector.currentActiveSegment.x) : 0
                    width: searchServerSelector.currentActiveSegment ? searchServerSelector.currentActiveSegment.width : 0

                    Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                }

                Row {
                    id: searchSegmentsRow
                    anchors.verticalCenter: parent.verticalCenter
                    x: 4
                    spacing: 4

                    // Server 1: Dhaka Flix
                    Item {
                        id: sseg1
                        visible: fluxUser ? (fluxUser.server1Enabled && fluxUser.server1Available) : true
                        height: 34
                        width: visible ? (sseg1Row.implicitWidth + 24) : 0

                        Rectangle {
                            anchors.fill: parent
                            radius: 17
                            color: "#1AFFFFFF"
                            visible: sseg1Mouse.containsMouse && searchServerSelector.activeServerIndex !== 1
                        }

                        Row {
                            id: sseg1Row
                            anchors.centerIn: parent
                            spacing: 6

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                anchors.verticalCenter: parent.verticalCenter
                                color: "#FFFFFF"
                                opacity: (searchServerSelector.activeServerIndex === 1) ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 140 } }
                            }

                            Text {
                                text: searchServerSelector.isWide ? "Server 1 (Dhaka Flix)" : "Server 1"
                                color: searchServerSelector.activeServerIndex === 1 ? "#FFFFFF"
                                     : (sseg1Mouse.containsMouse ? "#FFFFFF" : Theme.textDim)
                                font.pixelSize: 13
                                font.weight: searchServerSelector.activeServerIndex === 1 ? Font.Bold : Font.Normal
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }
                        }

                        MouseArea {
                            id: sseg1Mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (fluxUser && fluxUser.activeServer !== 1) {
                                    if (fluxSearch) fluxSearch.clearSuggestions()
                                    categoryFilter.isExpanded = false
                                    var textQuery = topSearchInput.text.trim()
                                    fluxUser.activeServer = 1
                                    if (fluxBrowser && fluxBrowser.active) fluxBrowser.close()
                                    if (textQuery.length > 0 && fluxSearch && fluxSearch.query !== textQuery) {
                                        fluxSearch.search(textQuery)
                                    }
                                }
                            }
                        }

                        FluxToolTip {
                            visible: sseg1Mouse.containsMouse
                            delay: 400
                            text: "Server 1 (Dhaka Flix) \u2014 BDIX directory indexer"
                        }
                    }

                    // Server 2: ICC FTP
                    Item {
                        id: sseg2
                        visible: fluxUser ? (fluxUser.server2Enabled && fluxUser.server2Available) : true
                        height: 34
                        width: visible ? (sseg2Row.implicitWidth + 24) : 0

                        Rectangle {
                            anchors.fill: parent
                            radius: 17
                            color: "#1AFFFFFF"
                            visible: sseg2Mouse.containsMouse && searchServerSelector.activeServerIndex !== 2
                        }

                        Row {
                            id: sseg2Row
                            anchors.centerIn: parent
                            spacing: 6

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                anchors.verticalCenter: parent.verticalCenter
                                color: "#FFFFFF"
                                opacity: (searchServerSelector.activeServerIndex === 2) ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 140 } }
                            }

                            Text {
                                text: searchServerSelector.isWide ? "Server 2 (ICC FTP)" : "Server 2"
                                color: searchServerSelector.activeServerIndex === 2 ? "#FFFFFF"
                                     : (sseg2Mouse.containsMouse ? "#FFFFFF" : Theme.textDim)
                                font.pixelSize: 13
                                font.weight: searchServerSelector.activeServerIndex === 2 ? Font.Bold : Font.Normal
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }
                        }

                        MouseArea {
                            id: sseg2Mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (fluxUser && fluxUser.activeServer !== 2) {
                                    if (fluxSearch) fluxSearch.clearSuggestions()
                                    categoryFilter.isExpanded = false
                                    var textQuery = topSearchInput.text.trim()
                                    fluxUser.activeServer = 2
                                    if (fluxBrowser && fluxBrowser.active) fluxBrowser.close()
                                    if (textQuery.length > 0 && fluxSearch && fluxSearch.query !== textQuery) {
                                        fluxSearch.search(textQuery)
                                    }
                                }
                            }
                        }

                        FluxToolTip {
                            visible: sseg2Mouse.containsMouse
                            delay: 400
                            text: "Server 2 (ICC FTP) \u2014 Direct high-speed streaming"
                        }
                    }

                    // Server 3: Circle FTP
                    Item {
                        id: sseg3
                        visible: fluxUser ? (fluxUser.server3Enabled && fluxUser.server3Available) : true
                        height: 34
                        width: visible ? (sseg3Row.implicitWidth + 24) : 0

                        Rectangle {
                            anchors.fill: parent
                            radius: 17
                            color: "#1AFFFFFF"
                            visible: sseg3Mouse.containsMouse && searchServerSelector.activeServerIndex !== 3
                        }

                        Row {
                            id: sseg3Row
                            anchors.centerIn: parent
                            spacing: 6

                            Rectangle {
                                width: 6
                                height: 6
                                radius: 3
                                anchors.verticalCenter: parent.verticalCenter
                                color: "#FFFFFF"
                                opacity: (searchServerSelector.activeServerIndex === 3) ? 1.0 : 0.0
                                Behavior on opacity { NumberAnimation { duration: 140 } }
                            }

                            Text {
                                text: searchServerSelector.isWide ? "Server 3 (Circle FTP)" : "Server 3"
                                color: searchServerSelector.activeServerIndex === 3 ? "#FFFFFF"
                                     : (sseg3Mouse.containsMouse ? "#FFFFFF" : Theme.textDim)
                                font.pixelSize: 13
                                font.weight: searchServerSelector.activeServerIndex === 3 ? Font.Bold : Font.Normal
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }
                        }

                        MouseArea {
                            id: sseg3Mouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (fluxUser && fluxUser.activeServer !== 3) {
                                    if (fluxSearch) fluxSearch.clearSuggestions()
                                    categoryFilter.isExpanded = false
                                    var textQuery = topSearchInput.text.trim()
                                    fluxUser.activeServer = 3
                                    if (fluxBrowser && fluxBrowser.active) fluxBrowser.close()
                                    if (textQuery.length > 0 && fluxSearch && fluxSearch.query !== textQuery) {
                                        fluxSearch.search(textQuery)
                                    }
                                }
                            }
                        }

                        FluxToolTip {
                            visible: sseg3Mouse.containsMouse
                            delay: 400
                            text: "Server 3 (Circle FTP) \u2014 Direct high-speed streaming"
                        }
                    }
                }
            }

            // Notice when all servers are turned off
            Rectangle {
                visible: searchServerSelector.enabledCount === 0
                implicitHeight: 38
                implicitWidth: noServersRow.implicitWidth + 28
                radius: 19
                color: "#2E1A1D"
                border.width: 1
                border.color: "#FF4D5E"

                RowLayout {
                    id: noServersRow
                    anchors.centerIn: parent
                    spacing: 10

                    FluxIcon {
                        name: "alert"
                        size: 14
                        color: "#FF4D5E"
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        text: "All media servers are currently turned off. Turn on servers in Settings."
                        color: "#FFD0D4"
                        font.pixelSize: 12
                        Layout.alignment: Qt.AlignVCenter
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 2. Search bar (compact horizontal width, centered)
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 2
            Layout.bottomMargin: 2
            z: 100

            Item { Layout.fillWidth: true }

            RowLayout {
                id: searchBarInnerRow
                Layout.preferredWidth: Math.min(480, Math.max(300, Math.round(resultsView.width * 0.45)))
                Layout.maximumWidth: Math.min(480, Math.max(300, Math.round(resultsView.width * 0.45)))
                implicitWidth: Math.min(480, Math.max(300, Math.round(resultsView.width * 0.45)))
                spacing: 10

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44

                    Rectangle {
                        id: searchInputBox
                        anchors.fill: parent
                        radius: 22
                        color: Theme.surface
                        border.width: topSearchInput.activeFocus ? 2 : 1
                        border.color: topSearchInput.activeFocus ? Theme.accent : Theme.border

                        Behavior on border.color { ColorAnimation { duration: 180 } }

                        property bool suppressSuggestions: false

                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: {
                                if (fluxUser && fluxUser.activeServer === 3 && topSearchInput.text.trim().length > 0) {
                                    if (!suggestPopup.opened) {
                                        if (fluxSearch && fluxSearch.suggestions.length > 0) {
                                            suggestPopup.open()
                                        } else if (fluxSearch) {
                                            suggestDebounceTimer.restart()
                                        }
                                    }
                                }
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 8
                            spacing: 10

                            FluxIcon {
                                name: "search"
                                size: 18
                                color: topSearchInput.activeFocus ? Theme.text : Theme.textMute
                                Layout.alignment: Qt.AlignVCenter
                            }

                            Timer {
                                id: suggestDebounceTimer
                                interval: 180
                                repeat: false
                                onTriggered: {
                                    if (fluxUser && fluxUser.activeServer === 3 && fluxSearch) {
                                        var q = topSearchInput.text.trim()
                                        if (q.length > 0) {
                                            fluxSearch.requestSuggestions(q)
                                        } else {
                                            fluxSearch.clearSuggestions()
                                            suggestPopup.close()
                                        }
                                    }
                                }
                            }

                            TextField {
                                id: topSearchInput
                                Layout.fillWidth: true
                                text: fluxSearch ? fluxSearch.query : ""
                                placeholderText: "Search movies, shows, episodes..."
                                placeholderTextColor: Theme.textMute
                                font.pixelSize: 13
                                color: Theme.text
                                selectedTextColor: "#FFFFFF"
                                selectionColor: Theme.accent
                                background: null

                                TapHandler {
                                    acceptedButtons: Qt.LeftButton
                                    onTapped: {
                                        if (fluxUser && fluxUser.activeServer === 3 && topSearchInput.text.trim().length > 0) {
                                            if (!suggestPopup.opened) {
                                                if (fluxSearch && fluxSearch.suggestions.length > 0) {
                                                    suggestPopup.open()
                                                } else if (fluxSearch) {
                                                    suggestDebounceTimer.restart()
                                                }
                                            }
                                        }
                                    }
                                }

                                onTextChanged: {
                                    // Search suggestions ONLY for Circle FTP (Server 3)
                                    if (fluxUser && fluxUser.activeServer === 3 && !searchInputBox.suppressSuggestions) {
                                        if (text.trim().length === 0) {
                                            suggestDebounceTimer.stop()
                                            suggestPopup.close()
                                            if (fluxSearch) fluxSearch.clearSuggestions()
                                        } else if (activeFocus) {
                                            suggestDebounceTimer.restart()
                                        }
                                    }
                                }

                                onActiveFocusChanged: {
                                    if (activeFocus && fluxUser && fluxUser.activeServer === 3 && text.trim().length > 0 && !searchInputBox.suppressSuggestions) {
                                        if (fluxSearch && fluxSearch.suggestions.length > 0) {
                                            suggestPopup.open()
                                        } else if (fluxSearch) {
                                            suggestDebounceTimer.restart()
                                        }
                                    }
                                }

                                onAccepted: {
                                    suggestDebounceTimer.stop()
                                    searchInputBox.suppressSuggestions = true
                                    suggestPopup.close()
                                    if (fluxSearch) fluxSearch.clearSuggestions()
                                    if (fluxSearch && text.trim().length > 0) {
                                        categoryFilter.isExpanded = false
                                        fluxSearch.search(text.trim())
                                    }
                                    Qt.callLater(() => { searchInputBox.suppressSuggestions = false })
                                }
                            }

                            IconButton {
                                id: topClearBtn
                                visible: topSearchInput.text.length > 0
                                iconName: "close"
                                iconSize: 14
                                iconColor: Theme.textDim
                                implicitWidth: 28
                                implicitHeight: 28
                                tip: "Clear"
                                tipAbove: false

                                onClicked: {
                                    suggestDebounceTimer.stop()
                                    suggestPopup.close()
                                    topSearchInput.text = ""
                                    if (fluxSearch) {
                                        fluxSearch.clearSuggestions()
                                        fluxSearch.clear()
                                    }
                                    categoryFilter.isExpanded = true
                                }
                            }
                        }
                    }

                    Connections {
                        target: (typeof fluxSearch !== "undefined" && fluxSearch) ? fluxSearch : null
                        function onSuggestionsChanged() {
                            if (fluxUser && fluxUser.activeServer === 3
                                    && topSearchInput.text.trim().length > 0
                                    && fluxSearch.suggestions.length > 0
                                    && topSearchInput.activeFocus
                                    && !searchInputBox.suppressSuggestions) {
                                suggestPopup.open()
                            } else if (fluxSearch && fluxSearch.suggestions.length === 0) {
                                suggestPopup.close()
                            }
                        }
                    }

                    Connections {
                        target: (typeof fluxUser !== "undefined" && fluxUser) ? fluxUser : null
                        function onActiveServerChanged() {
                            if (fluxUser.activeServer !== 3) {
                                suggestDebounceTimer.stop()
                                suggestPopup.close()
                                if (fluxSearch) fluxSearch.clearSuggestions()
                            }
                        }
                    }

                    // Auto-suggestions overlay dropdown (Circle FTP Server 3 ONLY)
                    Popup {
                        id: suggestPopup
                        parent: searchInputBox
                        x: 0
                        y: searchInputBox.height + 4
                        width: searchInputBox.width
                        implicitHeight: Math.min(300, suggestCol.implicitHeight + 12)
                        height: implicitHeight
                        padding: 6
                        modal: false
                        focus: false
                        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

                        enter: Transition {
                            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 120; easing.type: Easing.OutQuad }
                            NumberAnimation { property: "scale"; from: 0.98; to: 1.0; duration: 120; easing.type: Easing.OutQuad }
                        }
                        exit: Transition {
                            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 100; easing.type: Easing.InQuad }
                        }

                        background: Rectangle {
                            color: "#181d26"
                            radius: 12
                            border.width: 1
                            border.color: "#303947"
                        }

                        contentItem: Flickable {
                            id: suggestFlick
                            clip: true
                            contentWidth: width
                            contentHeight: suggestCol.implicitHeight
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: FluxScrollBar { }

                            Column {
                                id: suggestCol
                                width: suggestFlick.width
                                spacing: 2

                                Repeater {
                                    model: (fluxUser && fluxUser.activeServer === 3 && fluxSearch) ? fluxSearch.suggestions : []
                                    delegate: Rectangle {
                                        id: sugDelegate
                                        width: suggestCol.width
                                        height: 34
                                        radius: 6
                                        color: sugMouse.containsMouse ? "#26FFFFFF" : "transparent"

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            spacing: 10

                                            FluxIcon {
                                                name: "search"
                                                size: 14
                                                color: sugMouse.containsMouse ? Theme.accent : Theme.textMute
                                            }

                                            Text {
                                                text: modelData
                                                color: sugMouse.containsMouse ? "#FFFFFF" : Theme.text
                                                font.pixelSize: 13
                                                font.weight: sugMouse.containsMouse ? Font.DemiBold : Font.Normal
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }

                                        MouseArea {
                                            id: sugMouse
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: {
                                                suggestDebounceTimer.stop()
                                                searchInputBox.suppressSuggestions = true
                                                suggestPopup.close()
                                                var chosen = modelData
                                                topSearchInput.text = chosen
                                                if (fluxSearch) {
                                                    fluxSearch.clearSuggestions()
                                                    if (typeof categoryFilter !== "undefined" && categoryFilter) categoryFilter.isExpanded = false
                                                    fluxSearch.search(chosen)
                                                }
                                                Qt.callLater(() => { searchInputBox.suppressSuggestions = false })
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                FluxButton {
                    id: topSearchSubmitBtn
                    text: "Search"
                    implicitHeight: 44
                    enabled: !(fluxSearch && fluxSearch.isSearching)

                    onClicked: {
                        suggestDebounceTimer.stop()
                        searchInputBox.suppressSuggestions = true
                        suggestPopup.close()
                        if (fluxSearch && topSearchInput.text.trim().length > 0) {
                            fluxSearch.clearSuggestions()
                            categoryFilter.isExpanded = false
                            fluxSearch.search(topSearchInput.text.trim())
                        }
                        Qt.callLater(() => { searchInputBox.suppressSuggestions = false })
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 3. Category filters (collapsible, automatically collapses on search)
        CategoryFilterSection {
            id: categoryFilter
            Layout.fillWidth: true
            visible: !fluxUser || fluxUser.activeServer === 1
            libraryModel: fluxLibrary
            searchManager: fluxSearch
            collapsible: true
            isExpanded: true
        }

        // State: idle / welcome prompt when no search has been performed
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 40
            spacing: 12
            visible: !!fluxSearch && !fluxSearch.isSearching && !fluxSearch.hasResults && !fluxSearch.hasError && fluxSearch.query.length === 0

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 64
                implicitHeight: 64
                radius: 32
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                FluxIcon {
                    anchors.centerIn: parent
                    name: "search"
                    size: 28
                    color: Theme.accent
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Search on " + ((fluxUser && fluxUser.activeServer === 1) ? "Server 1 (Dhaka Flix)" : ((fluxUser && fluxUser.activeServer === 3) ? "Server 3 (Circle FTP)" : "Server 2 (ICC FTP)"))
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Bold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: (fluxUser && fluxUser.activeServer === 2)
                      ? "Enter a title to discover movies & shows."
                      : "Select categories above or enter a title to discover movies & shows."
                color: Theme.textDim
                font.pixelSize: 14
            }
        }

        // Results heading
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            visible: !!fluxSearch && !fluxSearch.isSearching && fluxSearch.hasResults

            Text {
                Layout.fillWidth: true
                text: fluxSearch ? ("Results for \u201C" + fluxSearch.query + "\u201D") : ""
                color: Theme.text
                font.pixelSize: 26
                font.weight: Font.Bold
                font.letterSpacing: -0.4
                elide: Text.ElideRight
            }

            Text {
                text: {
                    if (!fluxSearch) return ""
                    var summary = fluxSearch.selectedCategoriesSummary
                    var total = fluxSearch.resultCount
                    var fCount = fluxSearch.folderCount
                    var mCount = fluxSearch.mediaCount
                    var countStr = total + " results"
                    if (fCount > 0 && mCount > 0) {
                        countStr += " (" + fCount + (fCount === 1 ? " folder, " : " folders, ") + mCount + (mCount === 1 ? " video" : " videos") + ")"
                    }
                    return (summary.length > 0 ? (summary + "  \u00B7  ") : "") + countStr
                }
                color: Theme.textDim
                font.pixelSize: 13
            }
        }

        // State: searching
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 18
            visible: root.searching

            RowLayout {
                spacing: 12

                FluxSpinner {
                    size: 22
                    thickness: 3
                    running: root.searching
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: "Searching " + (fluxSearch ? fluxSearch.selectedCategoriesSummary : "") + "..."
                    color: Theme.textDim
                    font.pixelSize: 14
                }
            }

            SkeletonFlow {
                Layout.fillWidth: true
                active: root.searching
            }
        }

        // State: no results
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 70
            spacing: 10
            visible: !!fluxSearch && !fluxSearch.isSearching && !fluxSearch.hasResults && !fluxSearch.hasError && fluxSearch.query.length > 0

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
                    name: "search"
                    size: 30
                    color: Theme.textMute
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: "No results found"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Try a different title or select other categories above."
                color: Theme.textDim
                font.pixelSize: 14
            }
        }

        // State: error
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 70
            spacing: 10
            visible: !!fluxSearch && fluxSearch.hasError

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 72
                implicitHeight: 72
                radius: 36
                color: "#26FF5D5D"
                border.width: 1
                border.color: "#66FF5D5D"

                FluxIcon {
                    anchors.centerIn: parent
                    name: "alert"
                    size: 30
                    color: Theme.danger
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: "Unable to reach this library"
                color: Theme.danger
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Check your connection to the BDIX network."
                color: Theme.textDim
                font.pixelSize: 14
            }
        }

        // Sectioned Virtualized Search Results View: Folders first, then Movies/Episodes
        ListView {
            id: resultsListView

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !!fluxSearch && !fluxSearch.isSearching && fluxSearch.hasResults
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 800

            readonly property int targetCardW: 172
            readonly property int columns: Math.max(2, Math.floor((width + 14) / (targetCardW + 14)))
            readonly property real cardW: Math.floor((width - (columns - 1) * 14) / columns)
            readonly property real cardH: Math.round(cardW * 1.5)

            property int lastRevision: -1

            ScrollBar.vertical: FluxScrollBar { }

            model: {
                var rev = fluxSearch ? fluxSearch.resultsRevision : 0
                var searching = fluxSearch ? fluxSearch.isSearching : false
                var hasRes = fluxSearch ? fluxSearch.hasResults : false
                if (!fluxSearch || searching || !hasRes) return []
                return fluxSearch.getSectionedGridRows(resultsListView.columns)
            }

            onModelChanged: {
                var rev = fluxSearch ? fluxSearch.resultsRevision : 0
                if (rev !== lastRevision) {
                    lastRevision = rev
                    resultsListView.positionViewAtBeginning()
                }
            }

            footer: Item {
                width: resultsListView.width
                height: 36
            }

            delegate: Item {
                id: rowDelegate
                required property var modelData
                required property int index

                readonly property var rowInfo: modelData

                width: resultsListView.width
                height: rowInfo.isHeader ? (rowInfo.isFolder ? 46 : 66) : (resultsListView.cardH + 14)

                // SECTION HEADER
                RowLayout {
                    visible: rowInfo.isHeader === true
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    spacing: 10

                    Rectangle {
                        width: 32
                        height: 32
                        radius: 8
                        color: Theme.accentMuted

                        FluxIcon {
                            anchors.centerIn: parent
                            name: rowInfo.icon || "folder"
                            size: (rowInfo.icon === "folder") ? 18 : 16
                            color: Theme.accent
                        }
                    }

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

                            readonly property var prog: {
                                var rev = fluxUser ? fluxUser.revision : 0
                                return fluxUser ? fluxUser.progressFor(modelData.playUrl) : ({})
                            }

                            width: resultsListView.cardW
                            height: resultsListView.cardH
                            showDownload: true
                            onDownloadRequested: function(url, itemTitle, isPack) { root.startDownload(url, itemTitle, isPack) }
                            title: modelData.title || ""
                            parentPath: modelData.parentPath || ""
                            isFolder: modelData.isFolder === true
                            formattedSize: modelData.formattedSize || ""
                            extension: modelData.extension || ""
                            playUrl: modelData.playUrl || ""
                            libraryName: modelData.libraryName || ""
                            posterUrl: modelData.posterUrl || ""
                            isPlaying: (!!fluxPlayer && fluxPlayer.url === modelData.playUrl && fluxPlayer.isPlaying)
                            progress: (prog.fraction !== undefined) ? prog.fraction : 0
                            watched: (prog.watched === true)
                            introDelay: -1

                            onPlayRequested: function(url, itemTitle) {
                                root.playMediaRequested(url, itemTitle, "")
                            }

                            onFolderRequested: function(url, libName) {
                                if (fluxBrowser) fluxBrowser.open(url, libName, "")
                            }
                        }
                    }
                }
            }
        }
    }

    // =========================================================================
    // VIEW D: Bookmarks: the same card grid as search results
    // =========================================================================
    ColumnLayout {
        id: bookmarksView

        anchors.fill: parent
        anchors.topMargin: root.topInset + 10
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        spacing: 16
        opacity: root.showBookmarks ? 1.0 : 0.0
        visible: opacity > 0.0

        transform: Translate {
            y: root.showBookmarks ? 0 : 14

            Behavior on y { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
        }

        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        // Heading
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: "Bookmarks"
                color: Theme.text
                font.pixelSize: 26
                font.weight: Font.Bold
                font.letterSpacing: -0.4
            }

            Text {
                visible: !!fluxBookmarks && fluxBookmarks.count > 0
                text: fluxBookmarks ? (fluxBookmarks.count + (fluxBookmarks.count === 1 ? " item" : " items")
                                       + "  \u00B7  Right-click a card for more options") : ""
                color: Theme.textDim
                font.pixelSize: 13
            }
        }

        // Empty state
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 70
            spacing: 10
            visible: !fluxBookmarks || fluxBookmarks.count === 0

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 72
                implicitHeight: 72
                radius: 36
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                Text {
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: -2
                    text: "\u2606"
                    color: Theme.textMute
                    font.pixelSize: 32
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: "No bookmarks yet"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Right-click any movie or folder and choose Bookmark to save it here."
                color: Theme.textDim
                font.pixelSize: 14
            }
        }

        GridView {
            id: bookmarksGrid

            readonly property int targetCardW: 172
            readonly property int columns: Math.max(2, Math.floor(width / targetCardW))

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !!fluxBookmarks && fluxBookmarks.count > 0
            clip: true
            model: fluxBookmarks
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 1.5)
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 800

            ScrollBar.vertical: FluxScrollBar { }

            footer: Item {
                width: bookmarksGrid.width
                height: 32
            }

            delegate: SearchResultCard {
                readonly property var prog: {
                    var rev = fluxUser ? fluxUser.revision : 0
                    return fluxUser ? fluxUser.progressFor(model.playUrl) : ({})
                }

                width: bookmarksGrid.cellWidth
                height: bookmarksGrid.cellHeight
                showDownload: !model.isLocal
                title: model.title
                parentPath: model.parentPath
                isFolder: model.isFolder
                formattedSize: model.formattedSize
                extension: model.extension
                playUrl: model.playUrl
                localPath: model.localPath || ""
                libraryName: model.libraryName
                posterUrl: model.posterUrl || ""
                isPlaying: (!!fluxPlayer && fluxPlayer.url === model.playUrl && fluxPlayer.isPlaying)
                progress: (prog.fraction !== undefined) ? prog.fraction : 0
                watched: (prog.watched === true)
                introDelay: (index % 4) * 40

                onDownloadRequested: function(url, itemTitle, isPack) { root.startDownload(url, itemTitle, isPack) }

                onPlayRequested: function(url, itemTitle) {
                    root.playMediaRequested(url, itemTitle, "")
                }

                onFolderRequested: function(url, libName) {
                    if (model.isLocal && window && window.showLibrary) {
                        window.showLibrary(url)
                    } else if (fluxBrowser) {
                        fluxBrowser.open(url, libName, "")
                    }
                }
            }
        }
    }

    // =========================================================================
    // VIEW C: Folder browser (breadcrumbs + contents)
    // =========================================================================
    ColumnLayout {
        id: browserView

        anchors.fill: parent
        anchors.topMargin: root.topInset + 10
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        spacing: 16
        opacity: root.showBrowser ? 1.0 : 0.0
        visible: opacity > 0.0

        transform: Translate {
            y: root.showBrowser ? 0 : 14

            Behavior on y { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }
        }

        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }

        Breadcrumbs {
            Layout.fillWidth: true
            crumbs: fluxBrowser ? fluxBrowser.breadcrumbs : []
            rootLabel: root.bookmarksOpen ? "Bookmarks" : (root.hasSearched ? "Results" : "Home")

            onBackClicked: {
                if (fluxBrowser) fluxBrowser.goBack()
            }

            onRootClicked: {
                if (fluxBrowser) fluxBrowser.close()
            }

            onCrumbClicked: function(index) {
                if (fluxBrowser) fluxBrowser.goTo(index)
            }
        }

        // Folder heading
        RowLayout {
            Layout.fillWidth: true
            spacing: 16
            visible: !!fluxBrowser && fluxBrowser.active

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                Layout.fillWidth: true
                text: fluxBrowser ? fluxBrowser.folderName : ""
                color: Theme.text
                font.pixelSize: 26
                font.weight: Font.Bold
                font.letterSpacing: -0.4
                elide: Text.ElideRight
            }

            Text {
                visible: !!fluxBrowser && !fluxBrowser.isLoading && !fluxBrowser.hasError
                text: fluxBrowser ? (fluxBrowser.itemCount + (fluxBrowser.itemCount === 1 ? " item" : " items")) : ""
                color: Theme.textDim
                font.pixelSize: 13
            }
        }

        // Download everything in this folder (videos in subfolders included) as a pack
        FluxButton {
            text: "Download all"
            iconName: "download"
            iconSize: 18
            variant: "secondary"
            Layout.alignment: Qt.AlignVCenter
            visible: !!fluxBrowser && !fluxBrowser.isLoading && !fluxBrowser.hasError && fluxBrowser.itemCount > 0

            onClicked: root.startDownload(fluxBrowser.currentUrl, fluxBrowser.folderName, true)
        }
        }

        // State: loading
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            visible: !!fluxBrowser && fluxBrowser.isLoading
            spacing: 18

            SkeletonFlow {
                Layout.fillWidth: true
                active: !!fluxBrowser && fluxBrowser.isLoading
            }
        }

        // State: error
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 60
            spacing: 10
            visible: !!fluxBrowser && fluxBrowser.hasError

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 72
                implicitHeight: 72
                radius: 36
                color: "#26FF5D5D"
                border.width: 1
                border.color: "#66FF5D5D"

                FluxIcon {
                    anchors.centerIn: parent
                    name: "alert"
                    size: 30
                    color: Theme.danger
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: fluxBrowser ? fluxBrowser.errorMessage : ""
                color: Theme.textDim
                font.pixelSize: 14
            }

            FluxButton {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 6
                text: "Try again"
                iconName: "refresh"
                iconSize: 18

                onClicked: {
                    if (fluxBrowser) fluxBrowser.reload()
                }
            }
        }

        // State: empty folder
        ColumnLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 60
            spacing: 10
            visible: !!fluxBrowser && fluxBrowser.active && !fluxBrowser.isLoading && !fluxBrowser.hasError && fluxBrowser.itemCount === 0

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
                    name: "folder"
                    size: 30
                    color: Theme.textMute
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 8
                text: "Nothing to play here"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.DemiBold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "This folder has no video files or subfolders."
                color: Theme.textDim
                font.pixelSize: 14
            }
        }

        // Folder contents
        GridView {
            id: browserGrid

            readonly property int targetCardW: 172
            readonly property int columns: Math.max(2, Math.floor(width / targetCardW))

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !!fluxBrowser && !fluxBrowser.isLoading && !fluxBrowser.hasError && fluxBrowser.itemCount > 0
            clip: true
            model: fluxBrowser
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 1.5)
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 800

            ScrollBar.vertical: FluxScrollBar { }

            footer: Item {
                width: browserGrid.width
                height: 32
            }

            delegate: SearchResultCard {
                readonly property var prog: {
                    var rev = fluxUser ? fluxUser.revision : 0
                    return fluxUser ? fluxUser.progressFor(model.playUrl) : ({})
                }

                width: browserGrid.cellWidth
                height: browserGrid.cellHeight
                showDownload: true
                onDownloadRequested: function(url, itemTitle, isPack) { root.startDownload(url, itemTitle, isPack) }
                title: model.title
                parentPath: model.parentPath
                isFolder: model.isFolder
                formattedSize: model.formattedSize
                extension: model.extension
                playUrl: model.playUrl
                libraryName: model.libraryName
                posterUrl: model.posterUrl || ""
                isPlaying: (!!fluxPlayer && fluxPlayer.url === model.playUrl && fluxPlayer.isPlaying)
                progress: (prog.fraction !== undefined) ? prog.fraction : 0
                watched: (prog.watched === true)
                introDelay: (index % 4) * 40

                onPlayRequested: function(url, itemTitle) {
                    root.playMediaRequested(url, itemTitle, "")
                }

                onFolderRequested: function(url, libName) {
                    if (fluxBrowser) fluxBrowser.open(url, libName, "")
                }
            }
        }
    }
}
