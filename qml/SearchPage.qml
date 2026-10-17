import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "components/Theme.js" as Theme

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
        } else if (url && url.indexOf("player.php") !== -1 && fluxSearch) {
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
                        items = items.filter(function(item) { return item.isFolder !== true })
                        if (fluxTeleparty && fluxTeleparty.active && fluxSync) {
                            return items.filter(function(item) {
                                return fluxSync.isStreamUrl(item.url)
                            })
                        }
                        return items
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
                visible: (fluxUser ? fluxUser.server2Available : true) && !!fluxSearch && fluxSearch.trending && fluxSearch.trending.length > 0
                height: visible ? (trendingLayout.y + trendingLayout.implicitHeight + 40) : 0
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
                        if (continueList.count > 0) return 20
                        var isTop = !(fluxUser && fluxUser.topPicksFirst)
                        return isTop ? (root.topInset + 16) : 20
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
                                ? ("Showing all " + fluxSearch.trending.length)
                                : ("Top " + Math.min(fluxSearch.trending.length, trendingSection.trendingCols * 2))
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
                        Layout.bottomMargin: 24
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
                height: topPicksLayout.y + topPicksLayout.implicitHeight + 48
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

                readonly property var topPicksList: [
                    {
                        "title": "The Shawshank Redemption (1994) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%281995%29%201080p%20%26%20Before/The%20Shawshank%20Redemption%20%281994%29%201080p/The%20Shawshank%20Redemption%20%281994%29%20RM4K%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%205.1%20ESub-afm72.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=15765",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/9cqNxx0GxF0bflZmeSMuL5tnGzr.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "The Batman (2022) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282022%29%201080p/The%20Batman%20%282022%29%201080p/The%20Batman%20%282022%29%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%207.1%20ESub-Pahe.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=27914",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/74xTEgt7R36Fpooo50r9T25onhq.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Interstellar (2014) 1080p BluRay x265 Dual Audio",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282014%29%201080p/Interstellar%20%282014%29%201080p%20%5BDual%20Audio%5D/Interstellar%20%282014%29%20IMAX%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-mkvC.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=6139",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/gEU2QniE6E77NI6lCU6MxlNBvIx.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Spider-Man - Brand New Day (2026) 1080p AMZN Dual Audio",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282026%29%201080p/Spider-Man%20-%20Brand%20New%20Day%20%282026%29%201080p%20AMZN%20%5BDual%20Audio%5D/Spider-Man%20Brand%20New%20Day%20%282026%29%20IMAX%201080p%20AMZN-WEB%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-HDHub.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=45231",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/bjiS5ipwxb9JFy3XRRN4OAilSeX.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Shutter Island (2010) 720p BluRay x264 Dual Audio",
                        "url": "http://172.16.50.7/DHAKA-FLIX-7/English%20Movies/%282010%29/Shutter%20Island%20%282010%29%20720p%20%5BDual%20Audio%5D/Shutter%20Island%20%282010%29%20720p%20BluRay%20x264%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-%20mkvC.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=2670",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/nrmXQ0zcZUL8jFLrakWc90IR8z9.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Fight Club (1999) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%281999%29%201080p/Fight%20Club%20%281999%29%201080p/Fight%20Club%20%281999%29%20RM%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%207.1%20MSubs-Tigole.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=15767",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/pB8BM7pdSp6B6Ih7QZ4DrQ3PmJK.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "The Prestige (2006) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282006%29%201080p/The%20Prestige%20%282006%29%201080p/The%20Prestige%20%282006%29%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%205.1%20ESub-MkvCage.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=2543",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/Ag2B2KHKQPukjH7WutmgnnSNurZ.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Inception (2010) 1080p BluRay HEVC",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282010%29%201080p/Inception%20%282010%29%201080p/Inception.2010.1080p.BluRay.10bit.HEVC.6CH.MkvCage.ws.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=2626",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/s3TBrRGB1iav7gFOCNx3H31MoES.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "The Godfather (1972) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%281995%29%201080p%20%26%20Before/The%20Godfather-Part%20I%20%281972%29%201080p/The%20Godfather%20%281972%29%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%205.1%20ESub-afm72.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=575",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/3bhkrj58Vtu7enYsRolD1fZdja1.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "The Dark Knight Rises (2012) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282012%29%201080p/The%20Dark%20Knight%20Rises%20%282012%29%201080p/The%20Dark%20Knight%20Rises%20%282012%29%201080p%20BluRay%20x265%20HEVC%20AAC%20%205.1%20MSubs-Tigole.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=1772",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/hr0L2aueqlP2BYUblTTjmtn0hw4.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Avengers: Infinity War (2018) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282018%29%201080p/Avengers-Infinity%20War%20%282018%29%201080p/Avengers-Infinity%20War%20%282018%29%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%207.1%20MSubs-Prof.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=10529",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/7WsyChQLEftFiDOVTGkv3hFpyyt.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Spider-Man: Into the Spider-Verse (2018) 1080p BluRay Dual Audio",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/Animation%20Movies%20%281080p%29/Spider-Man%20-%20Into%20the%20Spider-Verse%20%282018%29%201080p%20%5BDual%20Audio%5D/Spider-Man%20Into%20the%20Spider-Verse%20%282018%29%201080p%20BluRay%20x264%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-ExtraM.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=12533",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/iiZZdoQBEYBv6id8su7ImL0oCbD.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Spider-Man: Across the Spider-Verse (2023) 1080p BluRay x265 Dual Audio",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/Animation%20Movies%20%281080p%29/Spider-Man%20-%20Across%20the%20Spider-Verse%20%282023%29%201080p%20%5BDual%20Audio%5D/Spider-Man%20Across%20the%20Spider-Verse%20%282023%29%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-mkvC.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=31909",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/8Vt6mWEReuy4Of61Lnj5Xj704m8.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Oppenheimer (2023) 1080p BluRay x265 Dual Audio",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/Oppenheimer%20%282023%29%201080p%20%5BDual%20Audio%5D/Oppenheimer%20%282023%29%20IMAX%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-mkvC.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=32662",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/8Gxv8gSFCU0XGDykEGv7zR1n2ua.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "The Usual Suspects (1995) 1080p BluRay HEVC",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%281995%29%201080p%20%26%20Before/The%20Usual%20Suspects%20%281995%29%201080p/The.Usual.Suspects.1995.1080p.BluRay.10bit.HEVC.6CH.MkvCage.ws.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=6203",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/bUPmtQzrRhzqYySeiMpv7GurAfm.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Your Name (2016) 1080p BluRay x265 Multi Audio",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/Animation%20Movies%20%281080p%29/Your%20Name%20%282016%29%201080p%20%5BMulti%20Audio%5D/Your%20Name%20%282016%29%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BMulti%20Audio%5D%5BHindi%202.0%2BHindi%205.1%2BEnglish%205.1%2BJapanese%205.1%5D%20-TW4all.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=9639",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/yLglTwyFOUZt5fNKm0PWL1PK5gm.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "12th Fail (2023) 1080p DSNP WEBRip x264",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/Hindi%20Movies/%282023%29/12th%20Fail%20%282023%29%201080p/12th%20Fail%20%282023%29%20Hindi%201080p%20DSNP%20WEBRip%20x264%20DD5.1%20ESub%20-%20mkvCinemas.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=34596",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/hm4ou83RwLVwNj3HsmQuqC1qkWy.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Cyberpunk: Edgerunners S01E01 1080p NF WEBRip x265",
                        "url": "http://172.16.50.10/DHAKA-FLIX-10/Anime%20%26%20Cartoon%20TV%20Series/Anime-TV%20Series%20%E2%99%A5%20%20A%20%20%E2%80%94%20%20F/Cyberpunk-Edgerunners%20%28TV%20Cartoon%202022%E2%80%93%20%29%201080p%20%5BDual%20Audio%5D/Season%201/%5BRapta%5D%20Cyberpunk%20Edgerunners%20-%20S01E01%20%5BDual%20Audio%5D%5B1080p%20NF%20WEBRip%20x265%20HEVC%2010bit%20DD5.1%20MSubs%5D.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=45538",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/lqcDVZ8pyk08AVftMBildDR3QUK.jpg",
                        "extraInfo": "Series"
                    },
                    {
                        "title": "American Psycho (2000) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282000%29%201080p/American%20Psycho%20%282000%29%201080p/American%20Psycho%20%282000%29%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%205.1%20ESub-Vyndros.mkv",
                        "altUrl": "",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/tlo3T0xxZRJZiDUgM6KF84q2dI4.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Memento (2000) 720p BluRay x264 Dual Audio",
                        "url": "http://172.16.50.7/DHAKA-FLIX-7/English%20Movies/%282000%29/Memento%20%282000%29%20720p%20%5BDual%20Audio%5D/Memento%20%282000%29%20RM%20720p%20BluRay%20x264%20ESub%20%5BDual%20Audio%5D%5BHindi%202.0%2BEnglish%205.1%5D%20-mkvC.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=6534",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/yuNs09hvpHVU1cBTCAk9zxsL2oW.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "No Country for Old Men (2007) 1080p BluRay x265 Dual Audio",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282007%29%201080p/No%20Country%20for%20Old%20Men%20%282007%29%201080p%20%5BDual%20Audio%5D/No%20Country%20for%20Old%20Men%20%282007%29%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-mkvC.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=14659",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/6d5XOczc226jECq0LIX0siKtgHR.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Passengers (2016) 1080p BluRay 6CH",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282016%29%201080p/Passengers%20%282016%29%201080p/Passengers.2016.1080p.BluRay.6CH.ShAaNiG.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=8230",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/n5DpXC4ZxH4dnydcz7qav9iA4U.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Gravity (2013) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282013%29%201080p/Gravity%20%282013%29%20SE%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-Silence.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=472",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/kZ2nZw8D681aphje8NJi8EfbL1U.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "The Martian (2015) 1080p BluRay x265 Extended",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282015%29%201080p/The%20Martian%20%282015%29%20Extended%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%207.1%20MSubs-Tigole.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=8894",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/5BHuvQ6p9kfc091Z8RiFNhCwL4b.jpg",
                        "extraInfo": "Top Pick"
                    },
                    {
                        "title": "Catch Me If You Can (2002) 1080p BluRay x265",
                        "url": "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282002%29%201080p/Catch%20Me%20If%20You%20Can%20%282002%29%201080p/Catch%20Me%20If%20You%20Can%20%282002%29%201080p%20BluRay%20x265%20HEVC%2010bit%20AAC%205.1%20ESub-pav69.mkv",
                        "altUrl": "http://10.16.100.244/player.php?play=6",
                        "posterUrl": "https://image.tmdb.org/t/p/w500/ggC3Brchf4RqPitEh7cROFoeAWn.jpg",
                        "extraInfo": "Top Pick"
                    }
                ]

                Component.onCompleted: {
                    if (fluxUser) {
                        for (var i = 0; i < topPicksList.length; ++i) {
                            fluxUser.rememberPoster(topPicksList[i].url, topPicksList[i].posterUrl, topPicksList[i].title)
                            if (topPicksList[i].altUrl && topPicksList[i].altUrl.length > 0) {
                                fluxUser.rememberPoster(topPicksList[i].altUrl, topPicksList[i].posterUrl, topPicksList[i].title)
                            }
                        }
                    }
                }

                ColumnLayout {
                    id: topPicksLayout
                    x: root.pageMargin
                    y: {
                        if (continueList.count > 0) return 20
                        var isTop = (fluxUser && fluxUser.topPicksFirst) || (!trendingSection.visible)
                        return isTop ? (root.topInset + 16) : 20
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
                                        if (!fluxUser.server2Available && fluxUser.server1Available) return "Dhaka Flix"
                                        if (!fluxUser.server1Available && fluxUser.server2Available) return "ICC FTP"
                                        return fluxUser.activeServer === 2 ? "ICC FTP" : "Dhaka Flix"
                                    }
                                    return "Dhaka Flix"
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

                                readonly property bool preferIcc: {
                                    if (fluxUser) {
                                        if (!fluxUser.server2Available && fluxUser.server1Available) return false
                                        if (!fluxUser.server1Available && fluxUser.server2Available) return true
                                        return (fluxUser.activeServer === 2 && modelData.altUrl && modelData.altUrl.length > 0)
                                    }
                                    return false
                                }
                                readonly property string effectiveUrl: preferIcc ? modelData.altUrl : modelData.url
                                readonly property string fallbackUrl: preferIcc ? modelData.url : (modelData.altUrl || "")

                                width: topPicksSection.picksCardW
                                height: topPicksSection.picksCardH
                                title: modelData.title
                                playUrl: effectiveUrl
                                posterUrl: modelData.posterUrl || ""
                                isFolder: false
                                libraryName: preferIcc ? "ICC FTP" : "DHAKA FLIX"
                                progress: 0
                                showRemove: false
                                extraInfo: modelData.extraInfo || "Top Pick"
                                introDelay: Math.min(index, 12) * 30

                                onPlayRequested: function(targetUrl, itemTitle) {
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
                        Layout.bottomMargin: 32
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

                readonly property int activeServerIndex: (fluxUser && fluxUser.activeServer === 1) ? 1 : 2
                readonly property bool isWide: root.width >= 700

                implicitHeight: 42
                implicitWidth: searchSegmentsRow.width + 8
                radius: 21
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                // Active sliding pill
                Rectangle {
                    height: 34
                    radius: 17
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.accent
                    x: searchSegmentsRow.x + (searchServerSelector.activeServerIndex === 1 ? sseg1.x : sseg2.x)
                    width: searchServerSelector.activeServerIndex === 1 ? sseg1.width : sseg2.width

                    Behavior on x { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                    Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }
                }

                Row {
                    id: searchSegmentsRow
                    anchors.verticalCenter: parent.verticalCenter
                    x: 4
                    spacing: 4

                    Item {
                        id: sseg1
                        height: 34
                        width: sseg1Row.implicitWidth + 24

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

                    Item {
                        id: sseg2
                        height: 34
                        width: sseg2Row.implicitWidth + 24

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
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 2. Search bar (compact horizontal width, centered)
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 2
            Layout.bottomMargin: 2

            Item { Layout.fillWidth: true }

            RowLayout {
                id: searchBarInnerRow
                Layout.preferredWidth: Math.min(480, Math.max(300, Math.round(resultsView.width * 0.45)))
                Layout.maximumWidth: Math.min(480, Math.max(300, Math.round(resultsView.width * 0.45)))
                implicitWidth: Math.min(480, Math.max(300, Math.round(resultsView.width * 0.45)))
                spacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    radius: 22
                    color: Theme.surface
                    border.width: topSearchInput.activeFocus ? 2 : 1
                    border.color: topSearchInput.activeFocus ? Theme.accent : Theme.border

                    Behavior on border.color { ColorAnimation { duration: 180 } }

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

                            onAccepted: {
                                if (fluxSearch && text.trim().length > 0) {
                                    categoryFilter.isExpanded = false
                                    fluxSearch.search(text.trim())
                                }
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
                                topSearchInput.text = ""
                                if (fluxSearch) fluxSearch.clear()
                                categoryFilter.isExpanded = true
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
                        if (fluxSearch && topSearchInput.text.trim().length > 0) {
                            categoryFilter.isExpanded = false
                            fluxSearch.search(topSearchInput.text.trim())
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        // 3. Category filters (collapsible, automatically collapses on search)
        CategoryFilterSection {
            id: categoryFilter
            Layout.fillWidth: true
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
                text: "Search on " + ((fluxUser && fluxUser.activeServer === 1) ? "Server 1 (Dhaka Flix)" : "Server 2 (ICC FTP)")
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Bold
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "Select categories above or enter a title to discover movies & shows."
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
                text: (fluxSearch ? fluxSearch.selectedCategoriesSummary : "") + "  \u00B7  " + (fluxSearch ? fluxSearch.resultCount : 0) + " results"
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

        // Results grid
        GridView {
            id: resultsGrid

            readonly property int targetCardW: 172
            readonly property int columns: Math.max(2, Math.floor(width / targetCardW))

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !!fluxSearch && !fluxSearch.isSearching && fluxSearch.hasResults
            clip: true
            model: fluxSearch
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 1.5)
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 800

            ScrollBar.vertical: FluxScrollBar { }

            footer: Item {
                width: resultsGrid.width
                height: 32
            }

            delegate: SearchResultCard {
                // Reading `revision` makes the watch badges refresh when history changes
                readonly property var prog: {
                    var rev = fluxUser ? fluxUser.revision : 0
                    return fluxUser ? fluxUser.progressFor(model.playUrl) : ({})
                }

                width: resultsGrid.cellWidth
                height: resultsGrid.cellHeight
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
                    if (fluxBrowser) fluxBrowser.open(url, libName, "")
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
