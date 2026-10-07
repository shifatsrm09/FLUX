import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var libraryModel: null
    property var searchManager: null
    signal selectionChanged()

    implicitHeight: flowLayout.implicitHeight

    Flow {
        id: flowLayout
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 6

        // =====================================================================
        // CHECKPOINT 1: "All" Option
        // =====================================================================
        Rectangle {
            id: allChip
            implicitHeight: 28
            implicitWidth: allRow.implicitWidth + 18
            radius: 5

            readonly property bool isChecked: root.searchManager ? root.searchManager.isAllSelected : true

            color: isChecked ? "#132338" : (allMouse.containsMouse ? "#151922" : "#0D1017")
            border.color: isChecked ? "#0284C7" : (allMouse.containsMouse ? "#334155" : "#1E2433")
            border.width: 1

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            RowLayout {
                id: allRow
                anchors.centerIn: parent
                spacing: 7

                // Checkbox Box
                Rectangle {
                    width: 13
                    height: 13
                    radius: 3
                    color: allChip.isChecked ? "#0284C7" : "#10131B"
                    border.color: allChip.isChecked ? "#0284C7" : "#2A3347"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        visible: allChip.isChecked
                        text: "✓"
                        color: "#FFFFFF"
                        font.pixelSize: 9
                        font.weight: Font.Bold
                    }
                }

                // Checkpoint Label
                Text {
                    text: "All"
                    color: allChip.isChecked ? "#F0F6FC" : "#8B949E"
                    font.pixelSize: 11
                    font.weight: allChip.isChecked ? Font.DemiBold : Font.Normal
                }
            }

            MouseArea {
                id: allMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (root.searchManager) {
                        root.searchManager.setAllSelected(!allChip.isChecked)
                        root.selectionChanged()
                    }
                }
            }

            ToolTip.visible: allMouse.containsMouse
            ToolTip.delay: 500
            ToolTip.text: allChip.isChecked
                          ? "All categories selected (click to unlock individual categories)"
                          : "Select all categories at once"
        }

        // =====================================================================
        // CHECKPOINTS 2..N: Individual Categories
        // =====================================================================
        Repeater {
            model: root.libraryModel

            delegate: Rectangle {
                id: catChip
                implicitHeight: 28
                implicitWidth: catRow.implicitWidth + 18
                radius: 5

                readonly property bool isLocked: root.searchManager ? root.searchManager.isAllSelected : true
                readonly property bool isChecked: !isLocked && root.searchManager && root.searchManager.isLibrarySelected(index)

                opacity: isLocked ? 0.38 : 1.0
                Behavior on opacity { NumberAnimation { duration: 150 } }

                color: isChecked ? "#132338" : (!isLocked && catMouse.containsMouse ? "#151922" : "#0D1017")
                border.color: isChecked ? "#0284C7" : (!isLocked && catMouse.containsMouse ? "#334155" : "#1E2433")
                border.width: 1

                Behavior on color { ColorAnimation { duration: 120 } }
                Behavior on border.color { ColorAnimation { duration: 120 } }

                RowLayout {
                    id: catRow
                    anchors.centerIn: parent
                    spacing: 7

                    // Checkbox Box (shows lock icon if All is selected)
                    Rectangle {
                        width: 13
                        height: 13
                        radius: 3
                        color: catChip.isChecked ? "#0284C7" : "#10131B"
                        border.color: catChip.isChecked ? "#0284C7" : (catChip.isLocked ? "#1A202C" : "#2A3347")
                        border.width: 1

                        // Checkmark icon
                        Text {
                            anchors.centerIn: parent
                            visible: catChip.isChecked
                            text: "✓"
                            color: "#FFFFFF"
                            font.pixelSize: 9
                            font.weight: Font.Bold
                        }

                        // Locked indicator (small lock symbol when All is active)
                        Text {
                            anchors.centerIn: parent
                            visible: catChip.isLocked
                            text: "🔒"
                            font.pixelSize: 7
                            opacity: 0.6
                        }
                    }

                    // Checkpoint Label
                    Text {
                        text: model.name
                        color: catChip.isChecked ? "#F0F6FC" : (catChip.isLocked ? "#556070" : "#8B949E")
                        font.pixelSize: 11
                        font.weight: catChip.isChecked ? Font.DemiBold : Font.Normal
                    }
                }

                MouseArea {
                    id: catMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: catChip.isLocked ? Qt.ForbiddenCursor : Qt.PointingHandCursor
                    onClicked: {
                        if (catChip.isLocked) {
                            return // Locked! User must uncheck All first
                        }
                        if (root.searchManager) {
                            root.searchManager.toggleLibrary(index)
                            root.selectionChanged()
                        }
                    }
                }

                ToolTip.visible: catMouse.containsMouse
                ToolTip.delay: 400
                ToolTip.text: catChip.isLocked
                              ? "Locked because 'All' is selected. Uncheck 'All' to select individual categories."
                              : (catChip.isChecked ? "Click to deselect" : "Click to select")
            }
        }
    }
}
