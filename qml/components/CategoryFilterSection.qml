import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var searchManager: null
    property var libraryModel: null
    property bool collapsible: false
    property bool isExpanded: true
    signal selectionChanged()

    // Definition of the 6 logical category groups
    readonly property var categoryGroups: [
        {
            title: "English Movies",
            items: [
                { id: "english_movies", name: "English Movies" },
                { id: "english_movies_1080p", name: "English Movies 1080p" },
                { id: "awards_tv_shows", name: "Awards & TV Shows" }
            ]
        },
        {
            title: "Hindi Movies",
            items: [
                { id: "hindi_movies", name: "Hindi Movies" },
                { id: "south_movies_hindi_dubbed", name: "South-Movie Hindi Dubbed" }
            ]
        },
        {
            title: "Animation",
            items: [
                { id: "animation_movies", name: "Animation Movies" },
                { id: "animation_movies_1080p", name: "Animation Movies 1080p" }
            ]
        },
        {
            title: "Other Movies",
            items: [
                { id: "foreign_movies", name: "Foreign Movies" },
                { id: "documentary", name: "Documentary" },
                { id: "movies_3d", name: "3D Movies" }
            ]
        },
        {
            title: "Regional Movies",
            items: [
                { id: "south_movies", name: "South Indian Movies" },
                { id: "kolkata_bangla_movies", name: "Kolkata Bangla Movies" },
                { id: "satyajit_ray_films", name: "Satyajit Ray Films" }
            ]
        },
        {
            title: "TV & Web Series",
            items: [
                { id: "tv_web_series", name: "TV & Web Series" },
                { id: "korean_series", name: "Korean TV & Web Series" },
                { id: "cartoon_tv_series", name: "Cartoon TV Series" },
                { id: "wwe_aew", name: "WWE & AEW Wrestling" }
            ]
        }
    ]

    // Responsive column count based on available width
    // Mobile (< 520px): 1 column
    // Tablet (< 800px): 2 columns
    // Desktop (>= 800px): 3 columns
    readonly property int numColumns: {
        if (width < 520) return 1
        if (width < 800) return 2
        return 3
    }

    readonly property real colSpacing: 14
    readonly property real rowSpacing: 14
    readonly property real colWidth: numColumns === 1
        ? width
        : Math.floor((width - (numColumns - 1) * colSpacing) / numColumns)

    implicitHeight: mainColumn.implicitHeight

    ColumnLayout {
        id: mainColumn
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        // =====================================================================
        // SECTION 1: "All" Category Option (At the Top)
        // =====================================================================
        Rectangle {
            id: allBar
            Layout.fillWidth: true
            implicitHeight: 40
            radius: 6

            readonly property bool isAllChecked: root.searchManager ? root.searchManager.isAllSelected : true

            color: allBarMouse.containsMouse ? "#131722" : "#0D1017"
            border.color: allBarMouse.containsMouse ? "#2E384D" : "#1B212D"
            border.width: 1

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                // Left: Clickable All checkbox and title
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.fill: parent
                        spacing: 10

                        // Checkbox Square for "All"
                        Rectangle {
                            implicitWidth: 15
                            implicitHeight: 15
                            radius: 3
                            color: allBar.isAllChecked ? "#0284C7" : "#10131B"
                            border.color: allBar.isAllChecked ? "#0284C7" : (allBarMouse.containsMouse ? "#3A475F" : "#283142")
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                visible: allBar.isAllChecked
                                text: "✓"
                                color: "#FFFFFF"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                            }
                        }

                        // Checkbox Label & Subtitle
                        Text {
                            text: "All Categories"
                            color: allBar.isAllChecked ? "#F0F6FC" : "#D1D5DB"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        Text {
                            text: "• Select all 17 DhakaFlix categories"
                            color: "#5C6677"
                            font.pixelSize: 11
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }
                    }

                    MouseArea {
                        id: allBarMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (root.searchManager) {
                                root.searchManager.toggleAll()
                                root.selectionChanged()
                            }
                        }
                    }
                }

                // Selected Counter Badge
                Rectangle {
                    implicitHeight: 22
                    implicitWidth: countText.implicitWidth + 14
                    radius: 11
                    color: "#131722"
                    border.color: "#1F2633"
                    border.width: 1

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: {
                            if (!root.searchManager) return "17 / 17"
                            var cnt = root.searchManager.selectedCount
                            return cnt + " / 17 selected"
                        }
                        color: allBar.isAllChecked ? "#38BDF8" : "#8F96A3"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }

                // Collapsible Toggle Button (if collapsible is enabled)
                Rectangle {
                    visible: root.collapsible
                    implicitHeight: 24
                    implicitWidth: toggleRow.implicitWidth + 14
                    radius: 4
                    color: toggleMouse.containsMouse ? "#1E2638" : "#131824"
                    border.color: toggleMouse.containsMouse ? "#3B4860" : "#222B3D"
                    border.width: 1

                    RowLayout {
                        id: toggleRow
                        anchors.centerIn: parent
                        spacing: 5

                        Text {
                            text: "Categories"
                            color: "#94A3B8"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                        }

                        Text {
                            text: root.isExpanded ? "▲" : "▼"
                            color: "#64748B"
                            font.pixelSize: 8
                        }
                    }

                    MouseArea {
                        id: toggleMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            root.isExpanded = !root.isExpanded
                        }
                    }
                }
            }
        }

        // =====================================================================
        // SECTION 2: Responsive Columns of Logical Category Groups
        // =====================================================================
        Grid {
            id: groupsGrid
            Layout.fillWidth: true
            columns: root.numColumns
            columnSpacing: root.colSpacing
            rowSpacing: root.rowSpacing
            visible: !root.collapsible || root.isExpanded

            Repeater {
                model: root.categoryGroups

                delegate: Rectangle {
                    id: groupCard
                    width: root.colWidth
                    implicitHeight: groupCol.implicitHeight + 20
                    radius: 6
                    color: "#0A0D14"
                    border.color: "#181E29"
                    border.width: 1

                    ColumnLayout {
                        id: groupCol
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 10
                        spacing: 6

                        // Group Section Heading
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6

                            Rectangle {
                                implicitWidth: 3
                                implicitHeight: 11
                                radius: 1.5
                                color: "#38BDF8"
                                opacity: 0.85
                            }

                            Text {
                                text: modelData.title.toUpperCase()
                                color: "#8E9AA8"
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                font.letterSpacing: 0.6
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                        }

                        // Category Items inside this Group
                        Repeater {
                            model: modelData.items

                            delegate: Rectangle {
                                id: itemRow
                                Layout.fillWidth: true
                                implicitHeight: 28
                                radius: 4

                                readonly property string catId: modelData.id
                                readonly property bool isSelected: {
                                    if (!root.searchManager) return true
                                    return root.searchManager.selectedCategoryIds.indexOf(catId) !== -1
                                }

                                // Clean background: subtle hover only, no full outline border when selected
                                color: itemMouse.containsMouse ? "#141822" : "transparent"
                                border.color: "transparent"
                                border.width: 0

                                Behavior on color { ColorAnimation { duration: 100 } }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    // Checkbox Box - only the check mark indicates selection
                                    Rectangle {
                                        implicitWidth: 14
                                        implicitHeight: 14
                                        radius: 3
                                        color: itemRow.isSelected ? "#0284C7" : "#0F121A"
                                        border.color: itemRow.isSelected ? "#0284C7" : (itemMouse.containsMouse ? "#3A465B" : "#242B38")
                                        border.width: 1

                                        Text {
                                            anchors.centerIn: parent
                                            visible: itemRow.isSelected
                                            text: "✓"
                                            color: "#FFFFFF"
                                            font.pixelSize: 9
                                            font.weight: Font.Bold
                                        }
                                    }

                                    // Category Name (Entire Row Clickable)
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: itemRow.isSelected
                                            ? "#F0F6FC"
                                            : (itemMouse.containsMouse ? "#CBD5E1" : "#8B949E")
                                        font.pixelSize: 11
                                        font.weight: itemRow.isSelected ? Font.Medium : Font.Normal
                                        elide: Text.ElideRight
                                    }
                                }

                                MouseArea {
                                    id: itemMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (root.searchManager) {
                                            root.searchManager.toggleCategory(itemRow.catId)
                                            root.selectionChanged()
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
