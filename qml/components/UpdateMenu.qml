import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// The "..." menu in the title bar: shows the running version and handles updates.
// Matches the FLUX Settings (Steam dark theme) design with square borders.
Popup {
    id: menu

    readonly property string st: fluxUpdater ? fluxUpdater.state : "idle"

    // The "Check for updates" row is shown whenever there's no update waiting
    readonly property bool showCheck: st === "idle" || st === "checking" || st === "upToDate" || st === "error"

    // Steam Dark Theme Palette matching FLUX Settings
    readonly property color cContentBg: "#1b1e24"
    readonly property color cBorder: "#2a2f3a"
    readonly property color cCardBg: "#21252d"
    readonly property color cCardBorder: "#2e3440"
    readonly property color cSteamBlue: "#1a9fff"
    readonly property color cTextWhite: "#F2F3F5"
    readonly property color cTextDim: "#949BA4"
    readonly property color cTextMute: "#80848E"

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

    // Settings-style Square Border Background
    background: Rectangle {
        radius: 0
        color: menu.cContentBg
        border.width: 1
        border.color: menu.cBorder
    }

    // Settings-style Square Button Component
    component MenuButton: Rectangle {
        id: btnRoot
        property string text: ""
        property string iconName: ""
        property bool isAccent: false
        property bool isGhost: false
        property bool isDanger: false
        property bool enabled: true
        signal clicked()

        opacity: btnRoot.enabled ? 1.0 : 0.45
        implicitHeight: 32
        radius: 0
        border.width: btnRoot.isGhost ? (btnMouse.containsMouse ? 1 : 0) : 1
        border.color: btnRoot.isDanger ? (btnMouse.containsMouse ? "#FF6B7A" : "#F23F43")
                    : btnRoot.isAccent ? (btnMouse.containsMouse ? "#29a8ff" : menu.cSteamBlue)
                    : (btnMouse.containsMouse ? "#4e5666" : "#3a4150")

        color: btnRoot.isDanger ? (btnMouse.pressed ? "#A81E22" : btnMouse.containsMouse ? "#C72E32" : "#991D21")
             : btnRoot.isAccent ? (btnMouse.pressed ? "#1177cc" : btnMouse.containsMouse ? "#29a8ff" : menu.cSteamBlue)
             : btnRoot.isGhost ? (btnMouse.pressed ? "#171a21" : btnMouse.containsMouse ? "#242933" : "transparent")
             : (btnMouse.pressed ? "#1a2029" : btnMouse.containsMouse ? "#2e3542" : "#242933")

        Behavior on color { ColorAnimation { duration: 100 } }
        Behavior on border.color { ColorAnimation { duration: 100 } }

        RowLayout {
            anchors.centerIn: parent
            spacing: 6

            FluxIcon {
                visible: btnRoot.iconName !== ""
                name: btnRoot.iconName
                size: 14
                color: "#FFFFFF"
            }

            Text {
                text: btnRoot.text
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: btnRoot.enabled
            cursorShape: btnRoot.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (btnRoot.enabled) btnRoot.clicked()
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Version Header ----
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
                    color: menu.cSteamBlue
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    font.letterSpacing: 2
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: "Version " + (fluxUpdater ? fluxUpdater.currentVersion : "")
                    color: menu.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            Text {
                Layout.fillWidth: true
                text: fluxUpdater && fluxUpdater.portable ? "Portable build" : "Installed build"
                color: menu.cTextMute
                font.family: "Segoe UI"
                font.pixelSize: 11
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: menu.cBorder
        }

        // ---- Updates Body ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            spacing: 10

            // Check for updates
            MenuButton {
                visible: menu.showCheck
                Layout.fillWidth: true
                implicitHeight: 34
                enabled: menu.st !== "checking"
                text: menu.st === "checking" ? "Checking\u2026" : "Check for updates"
                onClicked: if (fluxUpdater) fluxUpdater.checkForUpdates(false)
            }

            Text {
                Layout.fillWidth: true
                visible: menu.st === "upToDate"
                text: "You're on the latest version."
                color: Theme.success
                font.family: "Segoe UI"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            Text {
                Layout.fillWidth: true
                visible: menu.st === "error"
                text: fluxUpdater ? fluxUpdater.errorText : ""
                color: Theme.danger
                font.family: "Segoe UI"
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
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                    font.letterSpacing: 1.2
                }

                Text {
                    Layout.fillWidth: true
                    text: "FLUX v" + (fluxUpdater ? fluxUpdater.latestVersion : "")
                    color: menu.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }

                Text {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: fluxUpdater ? fluxUpdater.releaseNotes.replace(/^#{1,6}\s*/gm, "\u2022 ") : ""
                    color: menu.cTextDim
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                    maximumLineCount: 6
                    elide: Text.ElideRight
                }

                MenuButton {
                    Layout.fillWidth: true
                    Layout.topMargin: 4
                    implicitHeight: 34
                    isAccent: true
                    text: "Update now"
                    onClicked: if (fluxUpdater) fluxUpdater.startUpdate()
                }

                Text {
                    Layout.fillWidth: true
                    text: "FLUX will download the update, install it and restart."
                    color: menu.cTextMute
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Text {
                    text: "View release on GitHub"
                    color: linkMouse.containsMouse ? "#29a8ff" : menu.cSteamBlue
                    font.family: "Segoe UI"
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
                    color: menu.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                // Square progress track
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 6
                    radius: 0
                    color: menu.cCardBg
                    border.width: 1
                    border.color: menu.cCardBorder

                    Rectangle {
                        width: parent.width * (fluxUpdater ? fluxUpdater.progress : 0)
                        height: parent.height
                        radius: 0
                        color: menu.cSteamBlue

                        Behavior on width { NumberAnimation { duration: 120 } }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: fluxUpdater ? fluxUpdater.progressText : ""
                        color: menu.cTextDim
                        font.family: "Segoe UI"
                        font.pixelSize: 12
                        Layout.fillWidth: true
                    }

                    MenuButton {
                        text: "Cancel"
                        isGhost: true
                        implicitHeight: 26
                        implicitWidth: 60
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
                    color: menu.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Text {
                    Layout.fillWidth: true
                    text: "FLUX will close and restart in a moment."
                    color: menu.cTextDim
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: menu.cBorder
        }

        // ---- Cache footer ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 16
            Layout.topMargin: 10
            Layout.bottomMargin: 14
            spacing: 10

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: "Viewing cache"
                    color: menu.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                Text {
                    text: fluxCache ? fluxCache.sizeText : "0.00 GB of 10 GB"
                    color: menu.cTextMute
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                }
            }

            MenuButton {
                text: "Clear"
                implicitHeight: 28
                implicitWidth: 60
                enabled: !!fluxCache && fluxCache.sizeBytes > 0
                onClicked: fluxCache.clearCache()
            }
        }
    }
}
