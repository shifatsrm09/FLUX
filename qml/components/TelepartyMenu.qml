import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// The "Teleparty" menu in the title bar. Declared as a child of the button that opens it
// (see ClassicTitleBar.qml).
//
// Backed by `fluxTeleparty`; its `state` is one of  idle | connecting | joined | reconnecting
//   idle                  -> Host / Join choice (Join asks for the 5-digit code)
//   connecting            -> "Connecting..." with a cancel button
//   joined / reconnecting -> the session code, member count and "Leave session"
Popup {
    id: menu

    readonly property string st: fluxTeleparty ? fluxTeleparty.state : "idle"
    readonly property bool inSession: st === "joined" || st === "reconnecting"

    // Idle state: false = Host / Join choice, true = code entry
    property bool joining: false
    // Briefly true after the code was copied
    property bool copied: false

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

    onOpened: {
        if (fluxTeleparty) fluxTeleparty.clearError()
    }

    onClosed: {
        menu.joining = false
        menu.copied = false
        codeField.text = ""
        if (fluxTeleparty) fluxTeleparty.clearError()
    }

    // Back to the plain choice whenever a session starts or ends
    Connections {
        target: fluxTeleparty

        function onStateChanged() {
            menu.joining = false
            menu.copied = false
            codeField.text = ""
        }
    }

    Timer {
        id: copiedTimer
        interval: 1600
        onTriggered: menu.copied = false
    }

    function submitJoin() {
        if (!fluxTeleparty) return
        var code = codeField.text.trim()
        if (code.length !== 5) return
        fluxTeleparty.join(code)
    }

    background: Rectangle {
        radius: 14
        color: Theme.bgRaised
        border.width: 1
        border.color: Theme.borderHi
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Heading ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 14
            spacing: 4

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "TELEPARTY"
                    color: Theme.accent
                    font.pixelSize: 13
                    font.weight: Font.Black
                    font.letterSpacing: 3
                }

                Item { Layout.fillWidth: true }

                // Status dot + text
                Rectangle {
                    visible: menu.st !== "idle"
                    implicitWidth: 8
                    implicitHeight: 8
                    radius: 4
                    color: menu.st === "joined" ? Theme.success : Theme.warning
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    visible: menu.st !== "idle"
                    text: fluxTeleparty ? fluxTeleparty.statusText.replace("Teleparty: ", "") : ""
                    color: Theme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
            }

            Text {
                Layout.fillWidth: true
                text: menu.inSession
                      ? "Everything you play, pause or skip is shared with the party."
                      : "Watch the same stream together, in sync."
                color: Theme.textMute
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        // ---- Body ----
        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 16
            spacing: 12

            // ===== Idle: Host / Join =====
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.st === "idle" && !menu.joining
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    FluxButton {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        text: "Host"
                        onClicked: if (fluxTeleparty) fluxTeleparty.host()
                    }

                    FluxButton {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        variant: "secondary"
                        text: "Join"
                        onClicked: {
                            if (fluxTeleparty) fluxTeleparty.clearError()
                            menu.joining = true
                            codeField.forceActiveFocus()
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Host starts a party and gives you a 5-digit code. Friends use Join and type that code."
                    color: Theme.textMute
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    text: "Streaming only: downloaded files can't be shared."
                    color: Theme.textMute
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }

            // ===== Idle: enter the code =====
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.st === "idle" && menu.joining
                spacing: 10

                Text {
                    text: "Enter the 5-digit code"
                    color: Theme.text
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 48
                    radius: 12
                    color: codeField.activeFocus ? "#26FFFFFF" : "#1AFFFFFF"
                    border.width: 1
                    border.color: codeField.activeFocus ? Theme.accentRing : Theme.border

                    Behavior on color { ColorAnimation { duration: 120 } }

                    TextField {
                        id: codeField

                        anchors.fill: parent
                        anchors.leftMargin: 14
                        anchors.rightMargin: 14
                        placeholderText: "00000"
                        placeholderTextColor: Theme.textMute
                        color: Theme.text
                        font.pixelSize: 22
                        font.weight: Font.Bold
                        font.letterSpacing: 8
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        maximumLength: 5
                        inputMethodHints: Qt.ImhDigitsOnly
                        selectByMouse: true
                        background: null
                        validator: RegularExpressionValidator { regularExpression: /^\d{0,5}$/ }

                        onTextChanged: {
                            if (fluxTeleparty && fluxTeleparty.errorText.length > 0) fluxTeleparty.clearError()
                        }
                        onAccepted: menu.submitJoin()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    FluxButton {
                        implicitHeight: 40
                        variant: "ghost"
                        text: "Back"
                        onClicked: {
                            menu.joining = false
                            codeField.text = ""
                            if (fluxTeleparty) fluxTeleparty.clearError()
                        }
                    }

                    FluxButton {
                        Layout.fillWidth: true
                        implicitHeight: 40
                        text: "Join party"
                        enabled: codeField.text.length === 5
                        onClicked: menu.submitJoin()
                    }
                }
            }

            // Error from the last attempt (bad code, no session, no connection...)
            Text {
                Layout.fillWidth: true
                visible: menu.st === "idle" && !!fluxTeleparty && fluxTeleparty.errorText.length > 0
                text: fluxTeleparty ? fluxTeleparty.errorText : ""
                color: Theme.danger
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }

            // ===== Connecting =====
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.st === "connecting"
                spacing: 12

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 12

                    FluxSpinner {
                        size: 22
                        thickness: 3
                        running: menu.st === "connecting" && menu.visible
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: (fluxTeleparty && fluxTeleparty.isHost) ? "Creating your party\u2026" : "Looking for the party\u2026"
                        color: Theme.text
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                }

                FluxButton {
                    Layout.fillWidth: true
                    implicitHeight: 38
                    variant: "secondary"
                    text: "Cancel"
                    onClicked: if (fluxTeleparty) fluxTeleparty.leave()
                }
            }

            // ===== In a session =====
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.inSession
                spacing: 12

                // The code, big, with a copy button
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 64
                    radius: 12
                    color: "#14FFFFFF"
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Text {
                            text: "SESSION CODE"
                            color: Theme.textMute
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            font.letterSpacing: 1.4
                        }

                        Text {
                            text: fluxTeleparty ? fluxTeleparty.code : ""
                            color: Theme.text
                            font.pixelSize: 26
                            font.weight: Font.Black
                            font.letterSpacing: 6
                        }
                    }

                    FluxButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        implicitHeight: 34
                        variant: "secondary"
                        text: menu.copied ? "Copied" : "Copy"
                        onClicked: {
                            if (!fluxTeleparty) return
                            fluxTeleparty.copyCode()
                            menu.copied = true
                            copiedTimer.restart()
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    readonly property int n: fluxTeleparty ? fluxTeleparty.memberCount : 0
                    text: n <= 1 ? "Only you so far. Share the code with friends."
                                 : (n + " people in this party")
                    color: Theme.textDim
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: menu.st === "reconnecting"
                    text: "Connection lost. Trying to reconnect\u2026"
                    color: Theme.warning
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                FluxButton {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    variant: "secondary"
                    text: "Leave session"
                    onClicked: {
                        if (fluxTeleparty) fluxTeleparty.leave()
                        menu.close()
                    }
                }
            }
        }
    }
}
