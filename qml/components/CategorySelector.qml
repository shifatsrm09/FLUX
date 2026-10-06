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

    implicitWidth: 260
    implicitHeight: 44

    // Main Selection Button
    Rectangle {
        id: selectorBtn
        anchors.fill: parent
        radius: 8
        color: dropdownPopup.visible ? "#1a253c" : (mouseArea.containsMouse ? "#141e32" : "#0d1527")
        border.color: dropdownPopup.visible ? "#3b82f6" : (mouseArea.containsMouse ? "#334155" : "#1e293b")
        border.width: 1

        Behavior on color { ColorAnimation { duration: 120 } }
        Behavior on border.color { ColorAnimation { duration: 120 } }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1

                Text {
                    text: root.selectedGroup.length > 0 ? root.selectedGroup.toUpperCase() : "CATEGORY"
                    color: "#64748b"
                    font.pixelSize: 9
                    font.bold: true
                    font.letterSpacing: 1
                }

                Text {
                    Layout.fillWidth: true
                    text: root.selectedName.length > 0 ? root.selectedName : "Select Library"
                    color: "#f8fafc"
                    font.pixelSize: 13
                    font.bold: true
                    elide: Text.ElideRight
                }
            }

            Text {
                text: dropdownPopup.visible ? "▲" : "▼"
                color: "#94a3b8"
                font.pixelSize: 10
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

    // Dropdown Popup
    Popup {
        id: dropdownPopup
        y: root.height + 6
        width: 320
        height: Math.min(460, contentColumn.implicitHeight + 16)
        padding: 6
        modal: true
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#0a1120"
            border.color: "#1e293b"
            border.width: 1
            radius: 8
        }

        contentItem: ScrollView {
            id: scrollView
            clip: true

            Column {
                id: contentColumn
                width: dropdownPopup.availableWidth
                spacing: 2

                Repeater {
                    model: root.libraryModel

                    delegate: Column {
                        id: itemColumn
                        width: parent.width

                        // Section header when group changes
                        property bool isFirstInGroup: (index === 0 || (root.libraryModel.getRoot(index - 1).group !== model.group))

                        Rectangle {
                            width: parent.width
                            height: 28
                            color: "transparent"
                            visible: itemColumn.isFirstInGroup

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                text: model.group ? model.group.toUpperCase() : ""
                                color: "#38bdf8"
                                font.pixelSize: 10
                                font.bold: true
                                font.letterSpacing: 1.5
                            }
                        }

                        // Category Item Button
                        Rectangle {
                            width: parent.width
                            height: 36
                            radius: 6
                            color: {
                                if (index === root.selectedIndex) return "#1e3a8a"
                                if (itemMouseArea.containsMouse) return "#131d31"
                                return "transparent"
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12

                                Text {
                                    Layout.fillWidth: true
                                    text: model.name
                                    color: (index === root.selectedIndex) ? "#ffffff" : "#e2e8f0"
                                    font.pixelSize: 12
                                    font.bold: (index === root.selectedIndex)
                                    elide: Text.ElideRight
                                }

                                Text {
                                    visible: index === root.selectedIndex
                                    text: "✓"
                                    color: "#60a5fa"
                                    font.pixelSize: 12
                                    font.bold: true
                                }
                            }

                            MouseArea {
                                id: itemMouseArea
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
