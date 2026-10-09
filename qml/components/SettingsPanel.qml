import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// App settings dialog (opened from the gear at the bottom-right).
Popup {
    id: panel

    property string errorText: ""

    readonly property bool atDefaultLocation: !!fluxDownloads
        && fluxDownloads.downloadLocation.toLowerCase() === fluxDownloads.defaultLocation.toLowerCase()

    // Escape: close the folder picker first, then the dialog
    function dismiss() {
        if (picker.visible) picker.close()
        else panel.close()
    }

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min(560, (Overlay.overlay ? Overlay.overlay.width : 560) - 48)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onAboutToShow: errorText = ""

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
    }

    FolderPicker {
        id: picker

        onChosen: function(path) {
            if (fluxDownloads) fluxDownloads.changeLocation(path)
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
