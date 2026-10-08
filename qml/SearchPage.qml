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
    readonly property bool navSolid: root.hasSearched || homeFlick.contentY > 24
    readonly property real pageMargin: Math.max(32, Math.round((root.width - 1240) / 2))

    // =========================================================================
    // VIEW A: Home (before search): search bar + category columns
    // =========================================================================
    Flickable {
        id: homeFlick

        anchors.fill: parent
        visible: !root.hasSearched
        clip: true
        contentWidth: width
        contentHeight: homeColumn.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

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

                    Behavior on border.color { ColorAnimation { duration: 140 } }

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
    // VIEW B: Active results (search bar, filters, poster grid)
    // =========================================================================
    ColumnLayout {
        id: resultsView

        anchors.fill: parent
        anchors.topMargin: root.topInset + 10
        anchors.leftMargin: root.pageMargin
        anchors.rightMargin: root.pageMargin
        spacing: 16
        visible: root.hasSearched

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

                Behavior on border.color { ColorAnimation { duration: 140 } }

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

        // State: searching (skeleton tiles)
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

            Flow {
                id: skeletonFlow

                readonly property int cols: Math.max(1, Math.floor((width + 14) / (250 + 14)))
                readonly property real cellW: Math.floor((width - (cols - 1) * 14) / cols)

                Layout.fillWidth: true
                spacing: 14

                Repeater {
                    model: 12

                    delegate: Rectangle {
                        width: skeletonFlow.cellW
                        height: Math.round(skeletonFlow.cellW * 0.62)
                        radius: 12
                        color: Theme.surface

                        SequentialAnimation on opacity {
                            running: root.searching
                            loops: Animation.Infinite

                            NumberAnimation { from: 0.35; to: 0.85; duration: 800; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 0.85; to: 0.35; duration: 800; easing.type: Easing.InOutSine }
                        }
                    }
                }
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

                onPlayRequested: function(url, itemTitle) {
                    root.playMediaRequested(url, itemTitle)
                }
            }
        }
    }
}
