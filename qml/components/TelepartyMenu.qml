import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// The "Teleparty" menu in the title bar. Declared as a child of the button that opens it
// (see ClassicTitleBar.qml).
//
// Matches the FLUX Settings (Steam dark theme) design with square borders.
Popup {
    id: menu

    readonly property string st: fluxTeleparty ? fluxTeleparty.state : "idle"
    readonly property bool inSession: st === "joined" || st === "reconnecting"

    // Idle state: false = Host / Join choice, true = code entry
    property bool joining: false
    // Briefly true after the code was copied
    property bool copied: false

    // Steam Dark Theme Palette matching FLUX Settings
    readonly property color cSidebarBg: "#171a21"
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
        if (code.length < 3) return
        Qt.callLater(function() {
            fluxTeleparty.join(code)
        })
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
        implicitHeight: 34
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
                    color: menu.cSteamBlue
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    font.letterSpacing: 2
                }

                Item { Layout.fillWidth: true }

                // Square Status indicator
                Rectangle {
                    visible: menu.st !== "idle"
                    implicitWidth: 7
                    implicitHeight: 7
                    radius: 0
                    color: menu.st === "joined" ? Theme.success : Theme.warning
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    visible: menu.st !== "idle"
                    text: fluxTeleparty ? fluxTeleparty.statusText.replace("Teleparty: ", "") : ""
                    color: menu.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }

            Text {
                Layout.fillWidth: true
                text: menu.inSession
                      ? (fluxSync && fluxSync.localMode
                         ? "Local video: each participant needs a matching file in their Library."
                         : "Everything you play, pause or skip is shared with the party.")
                      : "Watch the same stream or a matching Library video together."
                color: menu.cTextMute
                font.family: "Segoe UI"
                font.pixelSize: 11
                wrapMode: Text.Wrap
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: menu.cBorder
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

                    MenuButton {
                        Layout.fillWidth: true
                        implicitHeight: 36
                        isAccent: true
                        text: "Host"
                        onClicked: {
                            if (fluxTeleparty) {
                                Qt.callLater(function() {
                                    fluxTeleparty.host()
                                })
                            }
                        }
                    }

                    MenuButton {
                        Layout.fillWidth: true
                        implicitHeight: 36
                        text: "Join"
                        onClicked: {
                            if (fluxTeleparty) fluxTeleparty.clearError()
                            menu.joining = true
                            codeField.forceActiveFocus()
                        }
                    }
                }

                // Saved latched rooms (up to last 3 entries with rejoin option)
                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !!fluxTeleparty && fluxTeleparty.latchedRooms.length > 0
                    spacing: 6

                    Text {
                        text: "Recent Rooms"
                        color: menu.cTextMute
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        Layout.bottomMargin: 2
                    }

                    Repeater {
                        model: fluxTeleparty ? fluxTeleparty.latchedRooms : []

                        Rectangle {
                            id: roomCard
                            required property string modelData

                            Layout.fillWidth: true
                            implicitHeight: 38
                            radius: 0
                            color: menu.cCardBg
                            border.width: 1
                            border.color: menu.cCardBorder

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 8
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    spacing: 6

                                    Rectangle {
                                        width: 6
                                        height: 6
                                        radius: 0
                                        color: Theme.success
                                    }

                                    Text {
                                        text: roomCard.modelData
                                        color: menu.cTextWhite
                                        font.family: "Segoe UI"
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                        font.letterSpacing: 1.5
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                MenuButton {
                                    implicitHeight: 26
                                    implicitWidth: 60
                                    text: "Rejoin"
                                    onClicked: {
                                        if (fluxTeleparty) {
                                            fluxTeleparty.clearError()
                                            Qt.callLater(function() {
                                                fluxTeleparty.join(roomCard.modelData)
                                            })
                                        }
                                    }
                                }

                                Rectangle {
                                    width: 24
                                    height: 24
                                    radius: 0
                                    color: forgetMouse.containsMouse ? "#2e3542" : "transparent"
                                    border.width: forgetMouse.containsMouse ? 1 : 0
                                    border.color: "#3a4150"

                                    FluxIcon {
                                        anchors.centerIn: parent
                                        name: "close"
                                        size: 12
                                        color: forgetMouse.containsMouse ? menu.cTextWhite : menu.cTextDim
                                    }

                                    MouseArea {
                                        id: forgetMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            if (fluxTeleparty) fluxTeleparty.removeLatchedRoom(roomCard.modelData)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Text {
                    Layout.fillWidth: true
                    text: "Host starts a party and gives you a code. Friends use Join and type that code."
                    color: menu.cTextMute
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    text: "In a party, click or open any video to sync playback with everyone. For local videos, room members need a matching local copy."
                    color: menu.cTextMute
                    font.family: "Segoe UI"
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
                    text: "Enter the party code"
                    color: menu.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }

                // Square code input box
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 44
                    radius: 0
                    color: menu.cSidebarBg
                    border.width: 1
                    border.color: codeField.activeFocus ? menu.cSteamBlue : menu.cCardBorder

                    Behavior on border.color { ColorAnimation { duration: 120 } }

                    TextField {
                        id: codeField

                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        placeholderText: "CODE"
                        placeholderTextColor: menu.cTextMute
                        color: menu.cTextWhite
                        font.family: "Segoe UI"
                        font.pixelSize: 18
                        font.weight: Font.Bold
                        font.letterSpacing: 4
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        maximumLength: 16
                        selectByMouse: true
                        background: null
                        validator: RegularExpressionValidator { regularExpression: /^[a-zA-Z0-9_-]{0,16}$/ }

                        onTextChanged: {
                            if (fluxTeleparty && fluxTeleparty.errorText.length > 0) fluxTeleparty.clearError()
                        }
                        onAccepted: menu.submitJoin()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    MenuButton {
                        implicitHeight: 36
                        implicitWidth: 64
                        isGhost: true
                        text: "Back"
                        onClicked: {
                            menu.joining = false
                            codeField.text = ""
                            if (fluxTeleparty) fluxTeleparty.clearError()
                        }
                    }

                    MenuButton {
                        Layout.fillWidth: true
                        implicitHeight: 36
                        isAccent: true
                        text: "Join party"
                        enabled: codeField.text.trim().length >= 3
                        onClicked: menu.submitJoin()
                    }
                }

                RowLayout {
                    visible: !!fluxTeleparty && fluxTeleparty.latchedRooms.length > 0
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Recent:"
                        color: menu.cTextMute
                        font.family: "Segoe UI"
                        font.pixelSize: 11
                    }

                    Flow {
                        Layout.fillWidth: true
                        spacing: 6

                        Repeater {
                            model: fluxTeleparty ? fluxTeleparty.latchedRooms : []

                            Rectangle {
                                id: chipRec
                                required property string modelData

                                radius: 0
                                color: chipMouse.containsMouse ? "#2e3542" : menu.cCardBg
                                border.width: 1
                                border.color: menu.cCardBorder
                                implicitHeight: 22
                                implicitWidth: chipTxt.implicitWidth + 12

                                Text {
                                    id: chipTxt
                                    anchors.centerIn: parent
                                    text: chipRec.modelData
                                    color: menu.cTextWhite
                                    font.family: "Segoe UI"
                                    font.pixelSize: 11
                                    font.weight: Font.Bold
                                    font.letterSpacing: 1
                                }

                                MouseArea {
                                    id: chipMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        codeField.text = chipRec.modelData
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Error from the last attempt (bad code, no session, no connection...)
            Text {
                Layout.fillWidth: true
                visible: menu.st === "idle" && !!fluxTeleparty && fluxTeleparty.errorText.length > 0
                text: fluxTeleparty ? fluxTeleparty.errorText : ""
                color: Theme.danger
                font.family: "Segoe UI"
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
                        size: 20
                        thickness: 3
                        running: menu.st === "connecting" && menu.visible
                        Layout.alignment: Qt.AlignVCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: (fluxTeleparty && fluxTeleparty.isHost) ? "Creating your party\u2026" : "Looking for the party\u2026"
                        color: menu.cTextWhite
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                }

                MenuButton {
                    Layout.fillWidth: true
                    implicitHeight: 34
                    text: "Cancel"
                    onClicked: if (fluxTeleparty) fluxTeleparty.leave()
                }
            }

            // ===== In a session =====
            ColumnLayout {
                Layout.fillWidth: true
                visible: menu.inSession
                spacing: 12

                // The code card, square
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 60
                    radius: 0
                    color: menu.cCardBg
                    border.width: 1
                    border.color: menu.cCardBorder

                    ColumnLayout {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 0

                        Text {
                            text: "SESSION CODE"
                            color: menu.cTextMute
                            font.family: "Segoe UI"
                            font.pixelSize: 10
                            font.weight: Font.Bold
                            font.letterSpacing: 1.4
                        }

                        Text {
                            text: fluxTeleparty ? fluxTeleparty.code : ""
                            color: menu.cTextWhite
                            font.family: "Segoe UI"
                            font.pixelSize: 22
                            font.weight: Font.Black
                            font.letterSpacing: 5
                        }
                    }

                    MenuButton {
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        implicitHeight: 30
                        implicitWidth: 64
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
                    color: menu.cTextDim
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: !!fluxSync && fluxSync.localMode && fluxSync.localStatusText.length > 0
                    text: fluxSync ? fluxSync.localStatusText : ""
                    color: fluxSync && fluxSync.localVerificationState === "WaitingForLocalSelection"
                           ? "#FFFF9AA2" : menu.cTextDim
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Text {
                    Layout.fillWidth: true
                    visible: menu.st === "reconnecting"
                    text: "Connection lost. Trying to reconnect\u2026"
                    color: Theme.warning
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }

                MenuButton {
                    Layout.fillWidth: true
                    implicitHeight: 36
                    isDanger: true
                    text: "Leave session"
                    onClicked: {
                        if (fluxTeleparty) {
                            Qt.callLater(function() {
                                fluxTeleparty.leave()
                            })
                        }
                    }
                }
            }
        }
    }
}
