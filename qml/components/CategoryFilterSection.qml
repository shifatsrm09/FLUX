import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Category picker: one column per group, items stacked underneath.
// Used on the home page (always open) and as a collapsible filter on the results page.
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

    readonly property bool groupsOpen: !root.collapsible || root.isExpanded
    readonly property int rowHeight: root.collapsible ? 38 : 44

    // Responsive columns: as many equal-width group columns as fit (max 6)
    readonly property real colSpacing: 20
    readonly property int colMinWidth: 200
    readonly property int columnCount: Math.max(1, Math.min(6, Math.floor((root.width + root.colSpacing) / (root.colMinWidth + root.colSpacing))))
    readonly property real colWidth: Math.max(160, Math.floor((root.width - (root.columnCount - 1) * root.colSpacing) / root.columnCount))

    implicitHeight: mainColumn.implicitHeight

    // =========================================================================
    // One selectable category row
    // =========================================================================
    Component {
        id: itemDelegate

        Rectangle {
            id: item

            required property var modelData

            readonly property string catId: item.modelData.id
            readonly property bool isSelected: root.searchManager
                                               ? root.searchManager.selectedCategoryIds.indexOf(item.catId) !== -1
                                               : true

            width: root.colWidth
            height: root.rowHeight
            radius: 10
            color: itemMouse.containsMouse ? Theme.surfaceHi : Theme.surface
            border.width: 1
            border.color: itemMouse.containsMouse ? Theme.borderHi : Theme.border

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                Text {
                    Layout.fillWidth: true
                    text: item.modelData.name
                    color: item.isSelected ? Theme.text : Theme.textDim
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }

                // Selected state: just a check mark
                Text {
                    visible: item.isSelected
                    text: "\u2713"
                    color: Theme.accentHover
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }
            }

            MouseArea {
                id: itemMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor

                onClicked: {
                    if (root.searchManager) {
                        root.searchManager.toggleCategory(item.catId)
                        root.selectionChanged()
                    }
                }
            }
        }
    }

    // =========================================================================
    // Layout
    // =========================================================================
    ColumnLayout {
        id: mainColumn

        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 20

        // ---- "All categories" bar ------------------------------------------------
        Rectangle {
            id: allBar

            Layout.fillWidth: true
            Layout.preferredHeight: 56
            radius: 12

            readonly property bool isAllChecked: root.searchManager ? root.searchManager.isAllSelected : true

            color: allMouse.containsMouse ? Theme.surfaceHi : Theme.surface
            border.width: 1
            border.color: allMouse.containsMouse ? Theme.borderHi : Theme.border

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 14
                spacing: 14

                // Clickable toggle area (label + check mark)
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    RowLayout {
                        anchors.fill: parent
                        spacing: 12

                        Text {
                            Layout.fillWidth: true
                            text: "All Categories"
                            color: allBar.isAllChecked ? Theme.text : Theme.textDim
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Text {
                            visible: allBar.isAllChecked
                            text: "\u2713"
                            color: Theme.accentHover
                            font.pixelSize: 16
                            font.weight: Font.Bold
                        }
                    }

                    MouseArea {
                        id: allMouse
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

                // Selected counter
                Rectangle {
                    implicitHeight: 26
                    implicitWidth: countText.implicitWidth + 22
                    radius: 13
                    color: Theme.surfaceHi
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        id: countText
                        anchors.centerIn: parent
                        text: root.searchManager ? (root.searchManager.selectedCount + " / 17 selected") : "17 / 17"
                        color: Theme.textDim
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }

                // Expand / collapse (results page)
                Rectangle {
                    visible: root.collapsible
                    implicitHeight: 32
                    implicitWidth: toggleRow.implicitWidth + 28
                    radius: 16
                    color: toggleMouse.containsMouse ? "#33FFFFFF" : "#1FFFFFFF"

                    Behavior on color { ColorAnimation { duration: 120 } }

                    RowLayout {
                        id: toggleRow
                        anchors.centerIn: parent
                        spacing: 6

                        Text {
                            text: "Categories"
                            color: Theme.text
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                        }

                        FluxIcon {
                            name: root.isExpanded ? "chevronUp" : "chevronDown"
                            size: 14
                            strokeWidth: 2.4
                            color: Theme.textDim
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: toggleMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.isExpanded = !root.isExpanded
                    }
                }
            }
        }

        // ---- Category columns ----------------------------------------------------------
        Item {
            id: groupsWrapper

            Layout.fillWidth: true
            Layout.preferredHeight: root.groupsOpen ? groupsFlow.implicitHeight : 0
            clip: true
            opacity: root.groupsOpen ? 1.0 : 0.0
            visible: height > 0.5 || opacity > 0.0

            Behavior on Layout.preferredHeight { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 200 } }

            Flow {
                id: groupsFlow

                width: parent.width
                spacing: root.colSpacing

                Repeater {
                    model: root.categoryGroups

                    delegate: Column {
                        id: groupColumn

                        required property var modelData

                        width: root.colWidth
                        spacing: 8

                        // Column heading
                        Item {
                            width: parent.width
                            height: 30

                            Rectangle {
                                id: headingBar
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: 14
                                radius: 1.5
                                color: Theme.accent
                            }

                            Text {
                                anchors.left: headingBar.right
                                anchors.leftMargin: 9
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                text: groupColumn.modelData.title
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                            }
                        }

                        Repeater {
                            model: groupColumn.modelData.items
                            delegate: itemDelegate
                        }
                    }
                }
            }
        }
    }
}
