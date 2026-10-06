import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root

    signal playMediaRequested(string url, string title)

    readonly property bool hasSearched: (fluxSearch && (fluxSearch.hasResults || fluxSearch.isSearching || fluxSearch.query.length > 0))

    Item {
        anchors.fill: parent

        // =====================================================================
        // VIEW A: Centered Initial State (Before Search)
        // =====================================================================
        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 48, 640)
            spacing: 24
            visible: !root.hasSearched

            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Search your library"
                    color: "#F5F5F5"
                    font.pixelSize: 34
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.5
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Stream directly across high-speed BDIX media mirrors"
                    color: "#8F96A3"
                    font.pixelSize: 14
                }
            }

            // Search Box & Action
            Rectangle {
                Layout.fillWidth: true
                height: 52
                radius: 8
                color: "#101218"
                border.color: initialInput.activeFocus ? "#38BDF8" : "#1A1D26"
                border.width: 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 8
                    spacing: 12

                    TextField {
                        id: initialInput
                        Layout.fillWidth: true
                        placeholderText: "Search movies, shows, episodes..."
                        placeholderTextColor: "#555C6A"
                        font.pixelSize: 14
                        color: "#F5F5F5"
                        selectedTextColor: "#FFFFFF"
                        selectionColor: "#0284C7"
                        background: null

                        onAccepted: {
                            if (fluxSearch && text.trim().length > 0) {
                                fluxSearch.search(text.trim())
                            }
                        }
                    }

                    Button {
                        id: initialSearchBtn
                        implicitWidth: 80
                        implicitHeight: 36
                        enabled: !(fluxSearch && fluxSearch.isSearching)

                        background: Rectangle {
                            radius: 6
                            color: initialSearchBtn.down ? "#0284C7" : (initialSearchBtn.hovered ? "#0EA5E9" : "#171A21")
                            border.color: initialSearchBtn.hovered ? "#0EA5E9" : "#232734"
                            border.width: 1
                        }

                        contentItem: Text {
                            text: "Search"
                            color: initialSearchBtn.hovered ? "#FFFFFF" : "#E2E8F0"
                            font.pixelSize: 13
                            font.weight: Font.Medium
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: {
                            if (fluxSearch && initialInput.text.trim().length > 0) {
                                fluxSearch.search(initialInput.text.trim())
                            }
                        }
                    }
                }
            }

            // Category Navigation Below Search
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                Text {
                    text: "Category:"
                    color: "#5E6676"
                    font.pixelSize: 12
                }

                CategorySelector {
                    libraryModel: fluxLibrary
                    selectedIndex: fluxSearch ? fluxSearch.selectedLibraryIndex : 0
                    selectedName: fluxSearch ? fluxSearch.selectedLibraryName : ""
                    selectedGroup: fluxSearch ? fluxSearch.selectedLibraryGroup : ""
                    onCategorySelected: function(index, id, name) {
                        if (fluxSearch) {
                            fluxSearch.selectLibrary(index)
                        }
                    }
                }
            }
        }

        // =====================================================================
        // VIEW B: Active Results State (Top Search + Results List)
        // =====================================================================
        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 24
            anchors.bottomMargin: 16
            anchors.leftMargin: Math.max(24, (parent.width - 920) / 2)
            anchors.rightMargin: Math.max(24, (parent.width - 920) / 2)
            spacing: 20
            visible: root.hasSearched

            // Top Compact Search Bar
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // Search Field
                Rectangle {
                    Layout.fillWidth: true
                    height: 44
                    radius: 6
                    color: "#101218"
                    border.color: topSearchInput.activeFocus ? "#38BDF8" : "#1A1D26"
                    border.width: 1

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 8
                        spacing: 10

                        TextField {
                            id: topSearchInput
                            Layout.fillWidth: true
                            text: fluxSearch ? fluxSearch.query : ""
                            placeholderText: "Search movies, shows, episodes..."
                            placeholderTextColor: "#555C6A"
                            font.pixelSize: 13
                            color: "#F5F5F5"
                            selectedTextColor: "#FFFFFF"
                            selectionColor: "#0284C7"
                            background: null

                            onAccepted: {
                                if (fluxSearch && text.trim().length > 0) {
                                    fluxSearch.search(text.trim())
                                }
                            }
                        }

                        Button {
                            id: topClearBtn
                            visible: topSearchInput.text.length > 0
                            implicitWidth: 28
                            implicitHeight: 28
                            background: Rectangle {
                                radius: 14
                                color: topClearBtn.hovered ? "#1E222D" : "transparent"
                            }
                            contentItem: Text {
                                text: "✕"
                                color: "#8F96A3"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                topSearchInput.text = ""
                                if (fluxSearch) fluxSearch.clear()
                            }
                        }
                    }
                }

                // Category Selector
                CategorySelector {
                    libraryModel: fluxLibrary
                    selectedIndex: fluxSearch ? fluxSearch.selectedLibraryIndex : 0
                    selectedName: fluxSearch ? fluxSearch.selectedLibraryName : ""
                    selectedGroup: fluxSearch ? fluxSearch.selectedLibraryGroup : ""
                    onCategorySelected: function(index, id, name) {
                        if (fluxSearch) {
                            fluxSearch.selectLibrary(index)
                            if (topSearchInput.text.trim().length > 0) {
                                fluxSearch.search(topSearchInput.text.trim())
                            }
                        }
                    }
                }

                // Search Submit Button
                Button {
                    id: topSearchSubmitBtn
                    implicitWidth: 76
                    implicitHeight: 44
                    enabled: !(fluxSearch && fluxSearch.isSearching)

                    background: Rectangle {
                        radius: 6
                        color: topSearchSubmitBtn.down ? "#0284C7" : (topSearchSubmitBtn.hovered ? "#0EA5E9" : "#171A21")
                        border.color: topSearchSubmitBtn.hovered ? "#0EA5E9" : "#232734"
                        border.width: 1
                    }

                    contentItem: Text {
                        text: "Search"
                        color: topSearchSubmitBtn.hovered ? "#FFFFFF" : "#E2E8F0"
                        font.pixelSize: 13
                        font.weight: Font.Medium
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: {
                        if (fluxSearch && topSearchInput.text.trim().length > 0) {
                            fluxSearch.search(topSearchInput.text.trim())
                        }
                    }
                }
            }

            // Results Heading
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                visible: fluxSearch && !fluxSearch.isSearching && fluxSearch.hasResults

                Text {
                    text: fluxSearch ? ("\"" + fluxSearch.query + "\"") : ""
                    color: "#F5F5F5"
                    font.pixelSize: 22
                    font.weight: Font.DemiBold
                    font.letterSpacing: -0.3
                    elide: Text.ElideRight
                }

                Text {
                    text: (fluxSearch ? fluxSearch.selectedLibraryName : "") + " · " + (fluxSearch ? fluxSearch.resultCount : 0) + " results"
                    color: "#8F96A3"
                    font.pixelSize: 13
                }
            }

            // State: Searching Feedback
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 20
                spacing: 12
                visible: fluxSearch && fluxSearch.isSearching

                BusyIndicator {
                    implicitWidth: 16
                    implicitHeight: 16
                    running: true
                }

                Text {
                    text: "Searching " + (fluxSearch ? fluxSearch.selectedLibraryName : "") + "..."
                    color: "#8F96A3"
                    font.pixelSize: 14
                }
            }

            // State: No Results
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 60
                spacing: 8
                visible: fluxSearch && !fluxSearch.isSearching && !fluxSearch.hasResults && !fluxSearch.hasError && fluxSearch.query.length > 0

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "No results found"
                    color: "#F5F5F5"
                    font.pixelSize: 18
                    font.weight: Font.Medium
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Try a different title or choose another category from the dropdown."
                    color: "#8F96A3"
                    font.pixelSize: 13
                }
            }

            // State: Error
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 60
                spacing: 8
                visible: fluxSearch && fluxSearch.hasError

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Unable to reach this library"
                    color: "#F87171"
                    font.pixelSize: 16
                    font.weight: Font.Medium
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Check your connection to the BDIX network."
                    color: "#8F96A3"
                    font.pixelSize: 13
                }
            }

            // Results List
            ScrollView {
                id: resultsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                visible: fluxSearch && !fluxSearch.isSearching && fluxSearch.hasResults

                ListView {
                    id: resultsList
                    width: resultsScroll.width
                    model: fluxSearch
                    spacing: 2

                    delegate: SearchResultCard {
                        width: resultsList.width
                        title: model.title
                        parentPath: model.parentPath
                        isFolder: model.isFolder
                        formattedSize: model.formattedSize
                        extension: model.extension
                        playUrl: model.playUrl
                        libraryName: model.libraryName
                        isPlaying: (fluxPlayer && fluxPlayer.url === model.playUrl && fluxPlayer.isPlaying)

                        onPlayRequested: function(url, itemTitle) {
                            root.playMediaRequested(url, itemTitle)
                        }
                    }
                }
            }
        }
    }
}
