import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// In-app folder chooser (no native dialog / extra Qt modules needed).
//   FolderPicker { id: picker; onChosen: function(path) { ... } }
//   picker.openAt("C:\\Users\\me\\Downloads")
Popup {
    id: picker

    property string currentPath: ""
    property var entries: []
    property bool creating: false

    signal chosen(string path)

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, (Overlay.overlay ? Overlay.overlay.width : 560) - 48)
    height: Math.min(520, (Overlay.overlay ? Overlay.overlay.height : 520) - 48)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape

    function refresh() {
        entries = fluxDownloads ? fluxDownloads.listDirs(currentPath) : []
        list.positionViewAtBeginning()
    }

    function go(path) {
        creating = false
        currentPath = path
        refresh()
    }

    function openAt(path) {
        currentPath = path
        creating = false
        open()
    }

    onOpened: {
        refresh()
        pathInput.text = currentPath
    }
    onCurrentPathChanged: pathInput.text = currentPath

    Overlay.modal: Rectangle { color: "#B3000000" }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 200; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
    }

    background: Rectangle {
        radius: 16
        color: Theme.bgRaised
        border.width: 1
        border.color: Theme.borderHi
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Header ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            Layout.bottomMargin: 12
            spacing: 8

            Text {
                text: "Choose download folder"
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            IconButton {
                iconName: "close"
                iconSize: 16
                implicitWidth: 34
                implicitHeight: 34
                tip: "Close"
                tipAbove: false
                onClicked: picker.close()
            }
        }

        // ---- Path bar ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            spacing: 8

            IconButton {
                iconName: "back"
                iconSize: 18
                implicitWidth: 38
                implicitHeight: 38
                filled: true
                enabled: picker.currentPath.length > 0
                tip: "Up one level"
                tipAbove: false
                onClicked: picker.go(fluxDownloads.parentDir(picker.currentPath))
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 38
                radius: 19
                color: Theme.surface
                border.width: pathInput.activeFocus ? 2 : 1
                border.color: pathInput.activeFocus ? Theme.accent : Theme.border

                TextField {
                    id: pathInput
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    text: ""
                    placeholderText: "This PC (pick a drive)"
                    placeholderTextColor: Theme.textMute
                    color: Theme.text
                    font.pixelSize: 13
                    selectedTextColor: "#FFFFFF"
                    selectionColor: Theme.accent
                    background: null
                    verticalAlignment: TextInput.AlignVCenter

                    // Type or paste a path and press Enter to jump there
                    onAccepted: picker.go(text.trim())
                }
            }
        }

        // ---- Folder list ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 20
            Layout.topMargin: 12
            Layout.bottomMargin: 12
            radius: 12
            color: Theme.surface
            border.width: 1
            border.color: Theme.border
            clip: true

            ListView {
                id: list

                anchors.fill: parent
                anchors.margins: 4
                clip: true
                model: picker.entries
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: FluxScrollBar { }

                delegate: Rectangle {
                    required property var modelData

                    width: ListView.view.width
                    height: 38
                    radius: 8
                    color: rowMouse.containsMouse ? "#1FFFFFFF" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 10

                        FluxIcon {
                            name: "folder"
                            size: 18
                            color: Theme.textDim
                            Layout.alignment: Qt.AlignVCenter
                        }

                        Text {
                            text: modelData.name
                            color: Theme.text
                            font.pixelSize: 14
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                        }

                        FluxIcon {
                            name: "chevronRight"
                            size: 16
                            color: Theme.textMute
                            Layout.alignment: Qt.AlignVCenter
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: picker.go(modelData.path)
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: list.count === 0
                    text: picker.currentPath.length === 0 ? "No drives found" : "No subfolders here"
                    color: Theme.textMute
                    font.pixelSize: 13
                }
            }
        }

        // ---- New folder ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 12
            spacing: 8
            visible: picker.creating

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 38
                radius: 19
                color: Theme.surface
                border.width: newName.activeFocus ? 2 : 1
                border.color: newName.activeFocus ? Theme.accent : Theme.border

                TextField {
                    id: newName
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 14
                    placeholderText: "New folder name"
                    placeholderTextColor: Theme.textMute
                    color: Theme.text
                    font.pixelSize: 13
                    selectedTextColor: "#FFFFFF"
                    selectionColor: Theme.accent
                    background: null
                    verticalAlignment: TextInput.AlignVCenter

                    onAccepted: createBtn.clicked()
                }
            }

            FluxButton {
                id: createBtn
                text: "Create"
                variant: "secondary"
                implicitHeight: 38
                enabled: newName.text.trim().length > 0

                onClicked: {
                    var made = fluxDownloads.makeDir(picker.currentPath, newName.text)
                    if (made.length > 0) {
                        newName.text = ""
                        picker.go(made)
                    }
                }
            }
        }

        // ---- Footer ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 20
            spacing: 10

            FluxButton {
                text: "New folder"
                variant: "ghost"
                implicitHeight: 40
                enabled: picker.currentPath.length > 0
                onClicked: {
                    picker.creating = !picker.creating
                    if (picker.creating) newName.forceActiveFocus()
                }
            }

            Item { Layout.fillWidth: true }

            FluxButton {
                text: "Cancel"
                variant: "secondary"
                implicitHeight: 40
                onClicked: picker.close()
            }

            FluxButton {
                text: "Select this folder"
                implicitHeight: 40
                enabled: picker.currentPath.length > 0
                onClicked: {
                    picker.chosen(picker.currentPath)
                    picker.close()
                }
            }
        }
    }
}
