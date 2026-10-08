import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Item {
    id: root

    property var libraryModel: null
    property int selectedIndex: 0
    property string selectedName: ""
    property string selectedGroup: ""

    signal categorySelected(int index, string id, string name)

    implicitWidth: selectorRow.implicitWidth + 36
    implicitHeight: 40

    Rectangle {
        id: selectorBtn
        anchors.fill: parent
        radius: height / 2
        color: dropdownPopup.visible ? Theme.surfaceTop : (mouseArea.containsMouse ? Theme.surfaceHi : Theme.surface)
        border.width: 1
        border.color: dropdownPopup.visible ? Theme.accent : (mouseArea.containsMouse ? Theme.borderHi : Theme.border)

        Behavior on color { ColorAnimation { duration: 100 } }
        Behavior on border.color { ColorAnimation { duration: 100 } }

        RowLayout {
            id: selectorRow
            anchors.centerIn: parent
            spacing: 8

            Text {
                text: root.selectedName.length > 0 ? root.selectedName : "Select Library"
                color: Theme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            FluxIcon {
                name: "chevronDown"
                size: 14
                strokeWidth: 2.4
                color: Theme.textDim
                Layout.alignment: Qt.AlignVCenter
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

    // Dropdown popover
    Popup {
        id: dropdownPopup
        y: root.height + 8
        width: 320
        height: Math.min(440, contentCol.implicitHeight + 16)
        padding: 8
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#F2101015"
            border.width: 1
            border.color: Theme.borderHi
            radius: 14
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
                            height: 28
                            visible: itemCol.isFirstInGroup

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                text: model.group ? model.group.toUpperCase() : ""
                                color: Theme.textMute
                                font.pixelSize: 10
                                font.weight: Font.Bold
                                font.letterSpacing: 1.2
                            }
                        }

                        // Category item
                        Rectangle {
                            width: parent.width
                            height: 34
                            radius: 8
                            color: {
                                if (index === root.selectedIndex) return Theme.accentSoft
                                if (itemMouse.containsMouse) return "#1FFFFFFF"
                                return "transparent"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: (index === root.selectedIndex) ? Theme.text : (itemMouse.containsMouse ? "#FFFFFF" : Theme.textDim)
                                    font.pixelSize: 12
                                    font.weight: (index === root.selectedIndex) ? Font.DemiBold : Font.Normal
                                    elide: Text.ElideRight
                                }

                                Text {
                                    visible: index === root.selectedIndex
                                    text: "\u2713"
                                    color: Theme.accentHover
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
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
