import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

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
        spacing: 8

        // ---- "All" chip ---------------------------------------------------------
        Rectangle {
            id: allChip
            implicitHeight: 34
            implicitWidth: allRow.implicitWidth + 28
            radius: 17

            readonly property bool isChecked: root.searchManager ? root.searchManager.isAllSelected : true

            color: isChecked ? Theme.accentSoft : (allMouse.containsMouse ? Theme.surfaceHi : Theme.surface)
            border.width: 1
            border.color: isChecked ? Theme.accentRing : (allMouse.containsMouse ? Theme.borderHi : Theme.border)

            Behavior on color { ColorAnimation { duration: 120 } }
            Behavior on border.color { ColorAnimation { duration: 120 } }

            RowLayout {
                id: allRow
                anchors.centerIn: parent
                spacing: 7

                Text {
                    visible: allChip.isChecked
                    text: "\u2713"
                    color: Theme.accentHover
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }

                Text {
                    text: "All"
                    color: allChip.isChecked ? Theme.text : Theme.textDim
                    font.pixelSize: 12
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

            FluxToolTip {
                visible: allMouse.containsMouse
                delay: 500
                text: allChip.isChecked
                      ? "All categories selected (click to unlock individual categories)"
                      : "Select all categories at once"
            }
        }

        // ---- Individual categories ---------------------------------------------------
        Repeater {
            model: root.libraryModel

            delegate: Rectangle {
                id: catChip
                implicitHeight: 34
                implicitWidth: catRow.implicitWidth + 28
                radius: 17

                readonly property bool isLocked: root.searchManager ? root.searchManager.isAllSelected : true
                readonly property bool isChecked: !isLocked && root.searchManager && root.searchManager.isLibrarySelected(index)

                opacity: isLocked ? 0.4 : 1.0
                Behavior on opacity { NumberAnimation { duration: 150 } }

                color: isChecked ? Theme.accentSoft : (!isLocked && catMouse.containsMouse ? Theme.surfaceHi : Theme.surface)
                border.width: 1
                border.color: isChecked ? Theme.accentRing : (!isLocked && catMouse.containsMouse ? Theme.borderHi : Theme.border)

                Behavior on color { ColorAnimation { duration: 120 } }
                Behavior on border.color { ColorAnimation { duration: 120 } }

                RowLayout {
                    id: catRow
                    anchors.centerIn: parent
                    spacing: 7

                    Text {
                        visible: catChip.isChecked
                        text: "\u2713"
                        color: Theme.accentHover
                        font.pixelSize: 12
                        font.weight: Font.Bold
                    }

                    Text {
                        text: model.name
                        color: catChip.isChecked ? Theme.text : Theme.textDim
                        font.pixelSize: 12
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
                            return // Locked: user must uncheck All first
                        }
                        if (root.searchManager) {
                            root.searchManager.toggleLibrary(index)
                            root.selectionChanged()
                        }
                    }
                }

                FluxToolTip {
                    visible: catMouse.containsMouse
                    delay: 400
                    text: catChip.isLocked
                          ? "Locked because 'All' is selected. Uncheck 'All' to select individual categories."
                          : (catChip.isChecked ? "Click to deselect" : "Click to select")
                }
            }
        }
    }
}
