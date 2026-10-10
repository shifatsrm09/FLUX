import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// The "..." menu in the title bar: shows the running version and handles updates.
// Declared as a child of the button that opens it (see ClassicTitleBar.qml).
//
// Backed by `fluxUpdater`; its `state` is one of
//   idle | checking | upToDate | available | downloading | installing | error
Popup {
    id: menu

    readonly property string st: fluxUpdater ? fluxUpdater.state : "idle"

    // The "Check for updates" row is shown whenever there's no update waiting
    readonly property bool showCheck: st === "idle" || st === "checking" || st === "upToDate" || st === "error"

    width: 300
    padding: 0
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
    }

    background: Rectangle {
        radius: 14
        color: Theme.bgRaised
        border.width: 1
        border.color: Theme.borderHi
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Version ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 14
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "FLUX"
                    color: Theme.accent
                    font.pixelSize: 15
                    font.weight: Font.Black
                    font.letterSpacing: 3
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Version " + (fluxUpdater ? fluxUpdater.currentVersion : "")
                    color: Theme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            Text {
                Layout.fillWidth: true
                text: fluxUpdater && fluxUpdater.portable ? "Portable build" : "Installed build"
                color: Theme.textMute
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        // ---- Updates ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            spacing: 10

            // Check for updates (+ result of the last check)
            FluxButton {
                visible: menu.showCheck
                Layout.fillWidth: true
                implicitHeight: 38
                variant: "secondary"
                enabled: menu.st !== "checking"
                text: menu.st === "checking" ? "Checking\u2026" : "Check for updates"
                onClicked: if (fluxUpdater) fluxUpdater.checkForUpdates(false)
            }

            Text {
                Layout.fillWidth: true
                visible: menu.st === "upToDate"
                text: "You're on the latest version."
                color: Theme.success
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: menu.st === "error"
                text: fluxUpdater ? fluxUpdater.errorText : ""
                color: Theme.danger
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            // Update available
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.st === "available"
                spacing: 8

                Text {
                    text: "UPDATE AVAILABLE"
                    color: Theme.success
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 1.2
                }

                Text {
                    Layout.fillWidth: true
                    text: "FLUX v" + (fluxUpdater ? fluxUpdater.latestVersion : "")
                    color: Theme.text
                    font.pixelSize: 16
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    // Release notes are Markdown; show them as plain bullets
                    text: fluxUpdater ? fluxUpdater.releaseNotes.replace(/^#{1,6}\s*/gm, "\u2022 ") : ""
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    maximumLineCount: 6
                    elide: Text.ElideRight
                }

                FluxButton {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    implicitHeight: 40
                    text: "Update now"
                    onClicked: if (fluxUpdater) fluxUpdater.startUpdate()
                }

                Text {
                    Layout.fillWidth: true
                    text: "FLUX will download the update, install it and restart."
                    color: Theme.textMute
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Text {
                    text: "View release on GitHub"
                    color: linkMouse.containsMouse ? Theme.accentHover : Theme.textDim
                    font.pixelSize: 11
                    font.underline: linkMouse.containsMouse

                    MouseArea {
                        id: linkMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: if (fluxUpdater) fluxUpdater.openReleasePage()
                    }
                }
            }

            // Downloading
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.st === "downloading"
                spacing: 8

                Text {
                    Layout.fillWidth: true
                    text: "Downloading v" + (fluxUpdater ? fluxUpdater.latestVersion : "") + "\u2026"
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 6
                    radius: 3
                    color: Theme.surfaceTop

                    Rectangle {
                        width: parent.width * (fluxUpdater ? fluxUpdater.progress : 0)
                        height: parent.height
                        radius: 3
                        color: Theme.accent

                        Behavior on width { NumberAnimation { duration: 120 } }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: fluxUpdater ? fluxUpdater.progressText : ""
                        color: Theme.textDim
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }

                    FluxButton {
                        text: "Cancel"
                        variant: "ghost"
                        implicitHeight: 30
                        onClicked: if (fluxUpdater) fluxUpdater.cancel()
                    }
                }
            }

            // Installing
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.st === "installing"
                spacing: 6

                Text {
                    Layout.fillWidth: true
                    text: "Installing update\u2026"
                    color: Theme.text
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: fluxUpdater && !fluxUpdater.portable
                          ? "FLUX will close and restart. Windows may ask for permission to install."
                          : "FLUX will close and restart in a moment."
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }
    }
}
