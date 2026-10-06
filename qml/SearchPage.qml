import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"

Item {
    id: root

    signal playMediaRequested(string url, string title)

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 18

        // =====================================================================
        // Hero Section
        // =====================================================================
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "Search your library"
                font.pixelSize: 26
                font.bold: true
                color: "#ffffff"
                font.letterSpacing: -0.5
            }

            Text {
                text: "Direct high-speed streaming across DhakaFlix BDIX media mirrors"
                font.pixelSize: 13
                color: "#64748b"
            }
        }

        // =====================================================================
        // Search & Category Bar
        // =====================================================================
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // Category Selector
            CategorySelector {
                id: categorySelector
                Layout.preferredWidth: 260
                Layout.minimumWidth: 220
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

            // Search Field Container
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 44
                radius: 8
                color: "#0d1527"
                border.color: searchInput.activeFocus ? "#3b82f6" : "#1e293b"
                border.width: searchInput.activeFocus ? 2 : 1

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: "🔍"
                        font.pixelSize: 13
                        opacity: 0.6
                    }

                    TextField {
                        id: searchInput
                        Layout.fillWidth: true
                        placeholderText: "Search movies, shows, episodes..."
                        font.pixelSize: 13
                        color: "#ffffff"
                        selectedTextColor: "#ffffff"
                        selectionColor: "#2563eb"
                        background: null

                        onAccepted: {
                            if (fluxSearch && text.trim().length > 0) {
                                fluxSearch.search(text.trim())
                            }
                        }
                    }

                    Button {
                        id: clearBtn
                        visible: searchInput.text.length > 0
                        implicitWidth: 28
                        implicitHeight: 28
                        background: Rectangle {
                            radius: 14
                            color: clearBtn.hovered ? "#334155" : "transparent"
                        }
                        contentItem: Text {
                            text: "✕"
                            color: "#94a3b8"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            searchInput.text = ""
                            if (fluxSearch) fluxSearch.clear()
                        }
                    }
                }
            }

            // Submit Button
            Button {
                id: searchSubmitBtn
                implicitWidth: 100
                implicitHeight: 44
                enabled: !(fluxSearch && fluxSearch.isSearching)

                background: Rectangle {
                    radius: 8
                    color: searchSubmitBtn.down ? "#1d4ed8" : (searchSubmitBtn.hovered ? "#2563eb" : "#3b82f6")
                }

                contentItem: RowLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        text: "Search"
                        color: "#ffffff"
                        font.pixelSize: 13
                        font.bold: true
                    }
                }

                onClicked: {
                    if (fluxSearch && searchInput.text.trim().length > 0) {
                        fluxSearch.search(searchInput.text.trim())
                    }
                }
            }
        }

        // =====================================================================
        // Results Status Header
        // =====================================================================
        Item {
            Layout.fillWidth: true
            implicitHeight: 28
            visible: fluxSearch && (fluxSearch.hasResults || fluxSearch.isSearching || fluxSearch.hasError || (fluxSearch.query.length > 0 && !fluxSearch.hasResults))

            RowLayout {
                anchors.fill: parent
                spacing: 8

                BusyIndicator {
                    implicitWidth: 18
                    implicitHeight: 18
                    running: fluxSearch && fluxSearch.isSearching
                    visible: fluxSearch && fluxSearch.isSearching
                }

                Text {
                    id: statusHeaderTitle
                    text: {
                        if (!fluxSearch) return ""
                        if (fluxSearch.isSearching) return "Searching " + fluxSearch.selectedLibraryName + "..."
                        if (fluxSearch.hasError) return "Search Failed"
                        if (fluxSearch.hasResults) return "Results for \"" + fluxSearch.query + "\""
                        if (fluxSearch.query.length > 0) return "No results for \"" + fluxSearch.query + "\""
                        return ""
                    }
                    font.pixelSize: 14
                    font.bold: true
                    color: (fluxSearch && fluxSearch.hasError) ? "#f87171" : "#f8fafc"
                }

                Text {
                    text: "•"
                    color: "#334155"
                    visible: fluxSearch && fluxSearch.hasResults
                }

                Text {
                    visible: fluxSearch && fluxSearch.hasResults
                    text: (fluxSearch ? fluxSearch.selectedLibraryName : "") + " · " + (fluxSearch ? fluxSearch.resultCount : 0) + " items found"
                    font.pixelSize: 12
                    color: "#94a3b8"
                }
            }
        }

        // =====================================================================
        // Main Content Area: Results List or Feedback States
        // =====================================================================
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // State: Initial Welcome
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 12
                visible: fluxSearch && !fluxSearch.isSearching && !fluxSearch.hasResults && !fluxSearch.hasError && fluxSearch.query.length === 0

                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    implicitWidth: 64
                    implicitHeight: 64
                    radius: 32
                    color: "#0f172a"
                    border.color: "#1e293b"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "🎬"
                        font.pixelSize: 26
                    }
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Search your library"
                    color: "#e2e8f0"
                    font.pixelSize: 16
                    font.bold: true
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Select a category above and search across 17 DhakaFlix library roots"
                    color: "#64748b"
                    font.pixelSize: 13
                }
            }

            // State: No Results
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10
                visible: fluxSearch && !fluxSearch.isSearching && !fluxSearch.hasResults && !fluxSearch.hasError && fluxSearch.query.length > 0

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "No results found"
                    color: "#f8fafc"
                    font.pixelSize: 16
                    font.bold: true
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "Try searching for a different title or switch to another category."
                    color: "#64748b"
                    font.pixelSize: 13
                }
            }

            // State: Error
            Rectangle {
                anchors.centerIn: parent
                width: 480
                implicitHeight: 100
                radius: 8
                color: "#1a0f14"
                border.color: "#7f1d1d"
                border.width: 1
                visible: fluxSearch && fluxSearch.hasError

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 6

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Unable to reach this library"
                        color: "#fca5a5"
                        font.pixelSize: 14
                        font.bold: true
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: fluxSearch ? fluxSearch.errorMessage : ""
                        color: "#f87171"
                        font.pixelSize: 12
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "Check connection to BDIX network (172.16.50.x)"
                        color: "#94a3b8"
                        font.pixelSize: 11
                        font.family: "Consolas, monospace"
                    }
                }
            }

            // State: Results ScrollView
            ScrollView {
                id: resultsScrollView
                anchors.fill: parent
                clip: true
                visible: fluxSearch && fluxSearch.hasResults

                ListView {
                    id: resultsList
                    width: resultsScrollView.width
                    model: fluxSearch
                    spacing: 8

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
