import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "components/Theme.js" as Theme

Item {
    id: root

    // Space reserved at the top for the overlaid nav bar
    property real topInset: 0

    signal playMediaRequested(string url, string title)

    readonly property bool hasSearched: (!!fluxSearch && (fluxSearch.hasResults || fluxSearch.isSearching || fluxSearch.query.length > 0))
    readonly property bool searching: (!!fluxSearch && fluxSearch.isSearching)
    readonly property bool browsing: (!!fluxBrowser && fluxBrowser.active)

    // Exactly one of the three views is shown at a time
    readonly property bool showHome: !root.hasSearched && !root.browsing
    readonly property bool showResults: root.hasSearched && !root.browsing
    readonly property bool showBrowser: root.browsing

    readonly property bool navSolid: root.hasSearched || root.browsing || homeFlick.contentY > 24
    readonly property real pageMargin: Math.max(32, Math.round((root.width - 1240) / 2))

    // Shimmering placeholder tiles shown while a listing loads
    component SkeletonFlow: Flow {
        id: skel

        property bool active: false

        readonly property int cols: Math.max(1, Math.floor((width + 14) / (250 + 14)))
        readonly property real cellW: Math.floor((width - (cols - 1) * 14) / cols)

        spacing: 14

        Repeater {
            model: 12

            delegate: Rectangle {
                width: skel.cellW
                height: Math.round(skel.cellW * 0.62)
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

        Column {
            id: homeColumn
            width: homeFlick.width
            spacing: 0

            // ---- Search ---------------------------------------------------------------
            Item {
                id: searchArea

                width: homeColumn.width
                height: root.topInset + 150
                clip: true

                // One subtle ambient glow behind the search box
                GlowOrb {
                    width: 900
                    height: 900
                    x: searchArea.width / 2 - 450
                    y: -520
                    color: Theme.accent
                    strength: 0.16
                }

                Rectangle {
                    id: heroSearch

                    width: Math.min(searchArea.width - 64, 720)
                    height: 62
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: root.topInset + 40
                    radius: 31
                    color: "#D915151A"
                    border.width: initialInput.activeFocus ? 2 : 1
                    border.color: initialInput.activeFocus ? Theme.accent : "#38FFFFFF"

                    Behavior on border.color { ColorAnimation { duration: 180 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 24
                        anchors.rightMargin: 9
                        spacing: 14

                        FluxIcon {
                            name: "search"
                            size: 22
                            color: initialInput.activeFocus ? Theme.text : Theme.textMute
                            Layout.alignment: Qt.AlignVCenter
                        }

                        TextField {
                            id: initialInput
                            Layout.fillWidth: true
                            placeholderText: "Search movies, shows, episodes..."
                            placeholderTextColor: Theme.textMute
                            font.pixelSize: 16
                            color: Theme.text
                            selectedTextColor: "#FFFFFF"
                            selectionColor: Theme.accent
                            background: null

                            onAccepted: {
                                if (fluxSearch && text.trim().length > 0) {
                                    fluxSearch.search(text.trim())
                                }
                            }
                        }

                        FluxButton {
                            id: initialSearchBtn
                            text: "Search"
                            implicitHeight: 44
                            enabled: !(fluxSearch && fluxSearch.isSearching)

                            onClicked: {
                                if (fluxSearch && initialInput.text.trim().length > 0) {
                                    fluxSearch.search(initialInput.text.trim())
                                }
                            }
                        }
                    }
                }
            }

            // ---- Continue Watching -------------------------------------------------------
            Item {
                id: continueSection

                width: homeColumn.width
                height: continueList.count > 0 ? (continueHeader.height + continueList.height + 36) : 0
                visible: continueList.count > 0

                RowLayout {
                    id: continueHeader

                    x: root.pageMargin
                    y: 0
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
                    y: continueHeader.height + 4
                    width: parent.width - root.pageMargin * 2 + 14
                    height: 176
                    orientation: ListView.Horizontal
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: fluxUser ? fluxUser.continueWatching : []

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

                        width: 262
                        height: 176
                        title: modelData.title
                        playUrl: modelData.url
                        progress: modelData.fraction
                        showRemove: true
                        extraInfo: Math.max(1, Math.round((modelData.durationMs - modelData.positionMs) / 60000)) + " min left"
                        introDelay: Math.min(index, 8) * 45

                        onPlayRequested: function(url, itemTitle) {
                            root.playMediaRequested(url, itemTitle)
                        }

                        onRemoveRequested: function(url) {
                            if (fluxUser) fluxUser.removeFromHistory(url)
                        }
                    }
                }
            }

            // ---- Categories -------------------------------------------------------------
            Item {
                id: browse

                width: homeColumn.width
                height: browseColumn.implicitHeight + 80

                ColumnLayout {
                    id: browseColumn

                    x: root.pageMargin
                    y: 0
                    width: parent.width - root.pageMargin * 2
                    spacing: 0

                    CategoryFilterSection {
                        Layout.fillWidth: true
                        libraryModel: fluxLibrary
                        searchManager: fluxSearch
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

        // Compact search bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                radius: 24
                color: Theme.surface
                border.width: topSearchInput.activeFocus ? 2 : 1
                border.color: topSearchInput.activeFocus ? Theme.accent : Theme.border

                Behavior on border.color { ColorAnimation { duration: 180 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 8
                    spacing: 12

                    FluxIcon {
                        name: "search"
                        size: 20
                        color: topSearchInput.activeFocus ? Theme.text : Theme.textMute
                        Layout.alignment: Qt.AlignVCenter
                    }

                    TextField {
                        id: topSearchInput
                        Layout.fillWidth: true
                        text: fluxSearch ? fluxSearch.query : ""
                        placeholderText: "Search movies, shows, episodes..."
                        placeholderTextColor: Theme.textMute
                        font.pixelSize: 14
                        color: Theme.text
                        selectedTextColor: "#FFFFFF"
                        selectionColor: Theme.accent
                        background: null

                        onAccepted: {
                            if (fluxSearch && text.trim().length > 0) {
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
                        implicitWidth: 30
                        implicitHeight: 30
                        tip: "Clear"
                        tipAbove: false

                        onClicked: {
                            topSearchInput.text = ""
                            if (fluxSearch) fluxSearch.clear()
                        }
                    }
                }
            }

            FluxButton {
                id: topSearchSubmitBtn
                text: "Search"
                implicitHeight: 48
                enabled: !(fluxSearch && fluxSearch.isSearching)

                onClicked: {
                    if (fluxSearch && topSearchInput.text.trim().length > 0) {
                        fluxSearch.search(topSearchInput.text.trim())
                    }
                }
            }
        }

        // Category filters
        CategoryFilterSection {
            Layout.fillWidth: true
            libraryModel: fluxLibrary
            searchManager: fluxSearch
            collapsible: true
            isExpanded: false

            onSelectionChanged: {
                if (fluxSearch && topSearchInput.text.trim().length > 0) {
                    fluxSearch.search(topSearchInput.text.trim())
                }
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

            readonly property int columns: Math.max(1, Math.floor(width / 250))

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !!fluxSearch && !fluxSearch.isSearching && fluxSearch.hasResults
            clip: true
            model: fluxSearch
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 0.62) + 14
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
                title: model.title
                parentPath: model.parentPath
                isFolder: model.isFolder
                formattedSize: model.formattedSize
                extension: model.extension
                playUrl: model.playUrl
                libraryName: model.libraryName
                isPlaying: (!!fluxPlayer && fluxPlayer.url === model.playUrl && fluxPlayer.isPlaying)
                progress: (prog.fraction !== undefined) ? prog.fraction : 0
                watched: (prog.watched === true)
                introDelay: (index % 4) * 40

                onPlayRequested: function(url, itemTitle) {
                    root.playMediaRequested(url, itemTitle)
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
            rootLabel: root.hasSearched ? "Results" : "Home"

            onBackClicked: {
                if (fluxBrowser) fluxBrowser.goUp()
            }

            onRootClicked: {
                if (fluxBrowser) fluxBrowser.close()
            }

            onCrumbClicked: function(index) {
                if (fluxBrowser) fluxBrowser.goTo(index)
            }
        }

        // Folder heading
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2
            visible: !!fluxBrowser && fluxBrowser.active

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

            readonly property int columns: Math.max(1, Math.floor(width / 250))

            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !!fluxBrowser && !fluxBrowser.isLoading && !fluxBrowser.hasError && fluxBrowser.itemCount > 0
            clip: true
            model: fluxBrowser
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 0.62) + 14
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
                title: model.title
                parentPath: model.parentPath
                isFolder: model.isFolder
                formattedSize: model.formattedSize
                extension: model.extension
                playUrl: model.playUrl
                libraryName: model.libraryName
                isPlaying: (!!fluxPlayer && fluxPlayer.url === model.playUrl && fluxPlayer.isPlaying)
                progress: (prog.fraction !== undefined) ? prog.fraction : 0
                watched: (prog.watched === true)
                introDelay: (index % 4) * 40

                onPlayRequested: function(url, itemTitle) {
                    root.playMediaRequested(url, itemTitle)
                }

                onFolderRequested: function(url, libName) {
                    if (fluxBrowser) fluxBrowser.open(url, libName, "")
                }
            }
        }
    }
}
