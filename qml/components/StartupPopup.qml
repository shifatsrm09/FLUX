import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

// Discord-style startup updater popup window.
// Replicates the exact Discord updater layout, colors (#282B30),
// typography and geometry:
//  - Logo: flux.png / flux.ico styled in Discord's exact position & proportions
//  - Checking update screen: "Checking for updates..." (with animated ellipsis)
//  - Actual update screen: "INSTALLING UPDATE 1 OF 1" (italic uppercase) + smooth progress bar (#3E4248)
Window {
    id: popup

    signal finished()

    property string phase: "checking"   // "checking" | "downloading" | "installing" | "starting" | "error"
    property bool checkStarted: false
    property bool doneEmitted: false

    width: 300
    height: 350
    minimumWidth: 300
    maximumWidth: 300
    minimumHeight: 350
    maximumHeight: 350
    visible: true
    title: "FLUX"
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    transientParent: null

    x: Math.round((Screen.width - width) / 2)
    y: Math.round((Screen.height - height) / 2)

    function proceedToApp() {
        if (popup.doneEmitted || popup.phase === "downloading" || popup.phase === "installing") {
            return
        }
        safetyTimer.stop()
        popup.phase = "starting"
        launchTimer.start()
    }

    function evaluateUpdaterState() {
        if (!fluxUpdater || popup.doneEmitted || popup.phase === "starting") {
            return
        }
        var st = fluxUpdater.state
        if (st === "available") {
            safetyTimer.stop()
            popup.phase = "downloading"
            fluxUpdater.startUpdate()
        } else if (st === "downloading") {
            safetyTimer.stop()
            popup.phase = "downloading"
        } else if (st === "installing") {
            safetyTimer.stop()
            popup.phase = "installing"
        } else if (st === "upToDate") {
            popup.proceedToApp()
        } else if (st === "idle" && popup.checkStarted && popup.phase === "checking") {
            popup.proceedToApp()
        } else if (st === "error") {
            safetyTimer.stop()
            if (popup.phase === "downloading" || popup.phase === "installing") {
                popup.phase = "error"
            } else {
                popup.proceedToApp()
            }
        }
    }

    Component.onCompleted: {
        if (!fluxUpdater) {
            popup.proceedToApp()
            return
        }
        startCheckTimer.start()
        safetyTimer.start()
    }

    Timer {
        id: startCheckTimer
        interval: 300
        repeat: false
        onTriggered: {
            if (!fluxUpdater) {
                popup.proceedToApp()
                return
            }
            popup.checkStarted = true
            fluxUpdater.checkForUpdates(true)
            popup.evaluateUpdaterState()
        }
    }

    Timer {
        id: safetyTimer
        interval: 6500
        repeat: false
        onTriggered: {
            if (popup.phase === "checking") {
                popup.proceedToApp()
            }
        }
    }

    Timer {
        id: launchTimer
        interval: 300
        repeat: false
        onTriggered: {
            if (!popup.doneEmitted) {
                popup.doneEmitted = true
                popup.finished()
            }
        }
    }

    // Checking ellipsis animation: "Checking for updates" -> "." -> ".." -> "..."
    property int dotCount: 3
    Timer {
        interval: 500
        running: popup.visible && popup.phase === "checking"
        repeat: true
        onTriggered: popup.dotCount = (popup.dotCount % 3) + 1
    }

    Connections {
        target: fluxUpdater

        function onStateChanged() {
            popup.evaluateUpdaterState()
        }
    }

    // ---- Exact Discord Window Card Surface (#282B30) ----
    Rectangle {
        id: card
        anchors.fill: parent
        radius: 8
        color: "#282B30"
        border.width: 1
        border.color: "#383C42"
        clip: true

        // Drag anywhere on the popup card
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            onPressed: function(mouse) {
                if (mouse.button === Qt.LeftButton) {
                    popup.startSystemMove()
                }
            }
        }

        // ---- Center Logo (FLUX v2.png logo centered like Discord Clyde) ----
        Item {
            id: logoContainer
            width: 72
            height: 72
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 96

            Image {
                id: logoImg
                anchors.fill: parent
                source: "qrc:/FLUX/resources/v2.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }
        }

        // ---- Text Area ----
        Item {
            id: textArea
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: logoContainer.bottom
            anchors.topMargin: 38
            width: parent.width - 40
            height: 24

            // "Checking for updates..." (Plain sentence case, regular weight, 14px)
            Text {
                anchors.centerIn: parent
                visible: popup.phase === "checking"
                text: "Checking for updates" + ".".repeat(popup.dotCount)
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 14
                font.weight: Font.DemiBold
            }

            // "INSTALLING UPDATE 1 OF 1" (All-caps, italicized, bold, 13px)
            Text {
                anchors.centerIn: parent
                visible: popup.phase === "downloading" || popup.phase === "installing"
                text: "INSTALLING UPDATE 1 OF 1"
                color: "#FAFBFC"
                font.family: "Segoe UI"
                font.pixelSize: 13
                font.italic: true
                font.bold: true
                font.letterSpacing: 0.5
            }

            // "STARTING..."
            Text {
                anchors.centerIn: parent
                visible: popup.phase === "starting"
                text: "STARTING\u2026"
                color: "#FAFBFC"
                font.family: "Segoe UI"
                font.pixelSize: 13
                font.italic: true
                font.bold: true
                font.letterSpacing: 0.5
            }

            // Error state
            Text {
                anchors.centerIn: parent
                visible: popup.phase === "error"
                text: "Update Interrupted"
                color: "#FF5D5D"
                font.family: "Segoe UI"
                font.pixelSize: 13
                font.bold: true
            }
        }

        // ---- Exact Discord Progress Bar (#3E4248) ----
        // Shown during actual update (downloading / installing)
        Rectangle {
            id: progressTrack
            width: 180
            height: 8
            radius: 4
            color: "#34383E"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: textArea.bottom
            anchors.topMargin: 16
            visible: popup.phase === "downloading" || popup.phase === "installing"
            clip: true

            Rectangle {
                id: progressFill
                height: parent.height
                radius: parent.radius
                color: "#4E545E"
                width: {
                    if (popup.phase === "downloading") {
                        var p = fluxUpdater ? fluxUpdater.progress : 0
                        return Math.max(8, parent.width * p)
                    }
                    return parent.width
                }

                Behavior on width { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
            }
        }

        // Error Retry / Skip Buttons
        RowLayout {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: textArea.bottom
            anchors.topMargin: 12
            visible: popup.phase === "error"
            spacing: 8

            Rectangle {
                width: 76
                height: 28
                radius: 4
                color: retryMouse.containsMouse ? "#4752C4" : "#5865F2"
                Text {
                    anchors.centerIn: parent
                    text: "Retry"
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: retryMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: if (fluxUpdater) { popup.phase = "downloading"; fluxUpdater.startUpdate() }
                }
            }

            Rectangle {
                width: 86
                height: 28
                radius: 4
                color: skipMouse.containsMouse ? "#40444B" : "#36393F"
                Text {
                    anchors.centerIn: parent
                    text: "Continue"
                    color: "#DCDDDE"
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                }
                MouseArea {
                    id: skipMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: popup.proceedToApp()
                }
            }
        }
    }
}
