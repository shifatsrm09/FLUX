import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property var libraryModel: null
    property int selectedIndex: 0
    property string selectedName: ""
    property string selectedGroup: ""

    signal categorySelected(int index, string id, string name)

    implicitWidth: selectorRow.implicitWidth + 24
    implicitHeight: 38

    Rectangle {
        id: selectorBtn
        anchors.fill: parent
        radius: 6
        color: dropdownPopup.visible ? "#171A21" : (mouseArea.containsMouse ? "#14171E" : "#0E1015")
        border.color: dropdownPopup.visible ? "#38BDF8" : (mouseArea.containsMouse ? "#2A2E3B" : "#1A1D26")
        border.width: 1

        Behavior on color { ColorAnimation { duration: 100 } }
        Behavior on border.color { ColorAnimation { duration: 100 } }

        RowLayout {
            id: selectorRow
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: root.selectedName.length > 0 ? root.selectedName : "Select Library"
                color: "#E2E8F0"
                font.pixelSize: 13
                font.weight: Font.Medium
            }

            Text {
                text: "▾"
                color: "#8F96A3"
                font.pixelSize: 11
            }
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (dropdownPopup.visible) {
                    dropdownPopup.close()
                } else {
                    dropdownPopup.open()
                }
            }
        }
    }

    // Dropdown Popover
    Popup {
        id: dropdownPopup
        y: root.height + 6
        width: 300
        height: Math.min(440, contentCol.implicitHeight + 16)
        padding: 6
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#0D0F14"
            border.color: "#202430"
            border.width: 1
            radius: 8
        }

        contentItem: ScrollView {
            clip: true

            Column {
                id: contentCol
                width: dropdownPopup.availableWidth
                spacing: 1

                Repeater {
                    model: root.libraryModel

                    delegate: Column {
                        id: itemCol
                        width: parent.width

                        // Section header when group changes
                        property bool isFirstInGroup: (index === 0 || (root.libraryModel.getRoot(index - 1).group !== model.group))

                        Item {
                            width: parent.width
                            height: 26
                            visible: itemCol.isFirstInGroup

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                text: model.group ? model.group.toUpperCase() : ""
                                color: "#5E6676"
                                font.pixelSize: 10
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }
                        }

                        // Category Item Button
                        Rectangle {
                            width: parent.width
                            height: 32
                            radius: 4
                            color: {
                                if (index === root.selectedIndex) return "#171A24"
                                if (itemMouse.containsMouse) return "#12141A"
                                return "transparent"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: (index === root.selectedIndex) ? "#38BDF8" : (itemMouse.containsMouse ? "#FFFFFF" : "#C7CBD4")
                                    font.pixelSize: 12
                                    font.weight: (index === root.selectedIndex) ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Text {
                                    visible: index === root.selectedIndex
                                    text: "✓"
                                    color: "#38BDF8"
                                    font.pixelSize: 11
                                }
                            }

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.categorySelected(index, model.id, model.name)
                                    dropdownPopup.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
