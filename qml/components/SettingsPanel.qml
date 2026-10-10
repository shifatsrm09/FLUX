import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// App settings dialog (opened from the gear at the bottom-right).
Popup {
    id: panel

    property string errorText: ""
    property string targetErrorText: ""

    readonly property bool atDefaultLocation: !!fluxDownloads
        && fluxDownloads.downloadLocation.toLowerCase() === fluxDownloads.defaultLocation.toLowerCase()

    // Escape: close a folder picker first, then the dialog
    function dismiss() {
        if (targetPicker.visible) targetPicker.close()
        else if (picker.visible) picker.close()
        else panel.close()
    }

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, (Overlay.overlay ? Overlay.overlay.width : 560) - 48)
    // Header (about 70px) + all sections; scrolls when the window is shorter than that
    height: Math.min(70 + settingsBody.implicitHeight, (Overlay.overlay ? Overlay.overlay.height : 720) - 48)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onAboutToShow: {
        errorText = ""
        targetErrorText = ""
    }

    Overlay.modal: Rectangle { color: "#B3000000" }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
        NumberAnimation { property: "scale"; from: 0.96; to: 1; duration: 200; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
    }

    background: Rectangle {
        radius: 18
        color: Theme.bgRaised
        border.width: 1
        border.color: Theme.borderHi
    }

    Connections {
        target: fluxDownloads

        function onLocationError(message) { panel.errorText = message }
        function onLocationChanged() { panel.errorText = "" }
        function onTargetError(message) { panel.targetErrorText = message }
        function onTargetsChanged() { panel.targetErrorText = "" }
    }

    // Picker for the download location
    FolderPicker {
        id: picker

        onChosen: function(path) {
            if (fluxDownloads) fluxDownloads.changeLocation(path)
        }
    }

    // Picker for extra library targets
    FolderPicker {
        id: targetPicker

        title: "Add a folder to scan"

        onChosen: function(path) {
            if (fluxDownloads) fluxDownloads.addTarget(path)
        }
    }

    // Small uppercase section label
    component SectionLabel: Text {
        color: Theme.textMute
        font.pixelSize: 11
        font.weight: Font.Bold
        font.letterSpacing: 1.2
    }

    // "Series  ->  C:\path" row
    component PathRow: RowLayout {
        property string label: ""
        property string path: ""

        spacing: 10

        Text {
            text: parent.label
            color: Theme.textDim
            font.pixelSize: 12
            font.weight: Font.DemiBold
            Layout.preferredWidth: 78
        }

        Text {
            text: parent.path
            color: Theme.text
            font.pixelSize: 12
            font.family: Theme.monoFamily
            elide: Text.ElideMiddle
            Layout.fillWidth: true
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Header ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 22
            Layout.bottomMargin: 8
            spacing: 10

            FluxIcon {
                name: "settings"
                size: 22
                color: Theme.text
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: "Settings"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Bold
                font.letterSpacing: -0.3
                Layout.fillWidth: true
            }

            IconButton {
                iconName: "close"
                iconSize: 16
                implicitWidth: 34
                implicitHeight: 34
                tip: "Close"
                tipAbove: false
                onClicked: panel.close()
            }
        }

        // ---- Scrollable body ----
        Flickable {
            id: scroller

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: settingsBody.implicitHeight
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: FluxScrollBar { }

            ColumnLayout {
                id: settingsBody

                width: scroller.width
                spacing: 0

                // ---- Downloads ----
                SectionLabel {
                    text: "DOWNLOADS"
                    Layout.leftMargin: 22
                    Layout.topMargin: 10
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 22
                    Layout.rightMargin: 22
                    Layout.topMargin: 10
                    implicitHeight: downloadsColumn.implicitHeight + 36
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        id: downloadsColumn

                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            text: "Download location"
                            color: Theme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 40
                            radius: 10
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.border

                            Text {
                                anchors.fill: parent
                                anchors.leftMargin: 14
                                anchors.rightMargin: 14
                                verticalAlignment: Text.AlignVCenter
                                text: fluxDownloads ? fluxDownloads.downloadLocation : ""
                                color: Theme.text
                                font.pixelSize: 13
                                font.family: Theme.monoFamily
                                elide: Text.ElideMiddle
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            FluxButton {
                                text: "Change\u2026"
                                iconName: "folder"
                                iconSize: 18
                                implicitHeight: 38
                                onClicked: picker.openAt(fluxDownloads ? fluxDownloads.downloadLocation : "")
                            }

                            FluxButton {
                                text: "Open folder"
                                variant: "secondary"
                                implicitHeight: 38
                                onClicked: if (fluxDownloads) fluxDownloads.openRoot()
                            }

                            FluxButton {
                                text: "Reset to default"
                                variant: "ghost"
                                implicitHeight: 38
                                visible: !panel.atDefaultLocation
                                onClicked: if (fluxDownloads) fluxDownloads.resetLocation()
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: panel.errorText.length > 0
                            text: panel.errorText
                            color: Theme.danger
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 1
                            color: Theme.border
                        }

                        PathRow {
                            Layout.fillWidth: true
                            label: "Series"
                            path: fluxDownloads ? fluxDownloads.seriesDir : ""
                        }

                        PathRow {
                            Layout.fillWidth: true
                            label: "Individuals"
                            path: fluxDownloads ? fluxDownloads.individualsDir : ""
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Folders you download as a pack are saved under Series, one folder per show. Single movies and episodes are saved under Individuals. Changing the location doesn't move files you've already downloaded."
                            color: Theme.textMute
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

                // ---- Library targets ----
                SectionLabel {
                    text: "LIBRARY TARGETS"
                    Layout.leftMargin: 22
                    Layout.topMargin: 20
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 22
                    Layout.rightMargin: 22
                    Layout.topMargin: 10
                    implicitHeight: targetsColumn.implicitHeight + 36
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        id: targetsColumn

                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            text: "More places to find media"
                            color: Theme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Add folders from anywhere on your PC. FLUX looks inside them for playable videos and lists them in your Library, and its search covers them too."
                            color: Theme.textMute
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        // Added targets
                        Repeater {
                            model: fluxDownloads ? fluxDownloads.targets : []

                            Rectangle {
                                id: targetRow

                                required property string modelData

                                Layout.fillWidth: true
                                implicitHeight: 40
                                radius: 10
                                color: Theme.bg
                                border.width: 1
                                border.color: Theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 4
                                    spacing: 10

                                    FluxIcon {
                                        name: "folder"
                                        size: 16
                                        color: Theme.textDim
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    Text {
                                        text: targetRow.modelData
                                        color: Theme.text
                                        font.pixelSize: 12
                                        font.family: Theme.monoFamily
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                        Layout.alignment: Qt.AlignVCenter
                                    }

                                    IconButton {
                                        iconName: "close"
                                        iconSize: 14
                                        implicitWidth: 32
                                        implicitHeight: 32
                                        tip: "Remove"
                                        tipAbove: false
                                        Layout.alignment: Qt.AlignVCenter
                                        onClicked: if (fluxDownloads) fluxDownloads.removeTarget(targetRow.modelData)
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: !fluxDownloads || fluxDownloads.targets.length === 0
                            text: "No extra folders added yet."
                            color: Theme.textDim
                            font.pixelSize: 12
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            FluxButton {
                                text: "Add more targets\u2026"
                                iconName: "folder"
                                iconSize: 18
                                implicitHeight: 38
                                onClicked: targetPicker.openAt("")
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: panel.targetErrorText.length > 0
                            text: panel.targetErrorText
                            color: Theme.danger
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: "Targets are only read, never changed. Downloads still go to your download location, and removing a target doesn't delete any files."
                            color: Theme.textMute
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }

                // ---- Playback ----
                SectionLabel {
                    text: "PLAYBACK"
                    Layout.leftMargin: 22
                    Layout.topMargin: 20
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 22
                    Layout.rightMargin: 22
                    Layout.topMargin: 10
                    Layout.bottomMargin: 22
                    implicitHeight: 62
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 18
                        anchors.rightMargin: 18
                        spacing: 12

                        ColumnLayout {
                            spacing: 2
                            Layout.fillWidth: true

                            Text {
                                text: "Autoplay next episode"
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }

                            Text {
                                text: "Offer the next episode when one finishes"
                                color: Theme.textMute
                                font.pixelSize: 12
                            }
                        }

                        ToggleSwitch {
                            checked: fluxUser ? fluxUser.autoplayNext : true
                            onToggled: function(value) { if (fluxUser) fluxUser.autoplayNext = value }
                        }
                    }
                }
            }
        }
    }
}
