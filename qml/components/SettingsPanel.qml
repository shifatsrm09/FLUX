import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Steam-Style Settings Dialog for FLUX
Popup {
    id: panel

    property string errorText: ""
    property string targetErrorText: ""
    property string serverTestStatus: ""
    property bool serverTesting: false
    property bool serverTestSuccess: false

    property bool server1Testing: false
    property string server1Status: ""
    property bool server1Success: false

    property bool server2Testing: false
    property string server2Status: ""
    property bool server2Success: false

    property bool server3Testing: false
    property string server3Status: ""
    property bool server3Success: false
    property bool showPinPrompt: false
    property string devPinErrorText: ""
    property string customCodeSavedText: ""

    // Active category in left sidebar
    property string currentTab: "server"

    // Steam Dark Theme Palette
    readonly property color cSidebarBg: "#171a21"       // Steam deep dark navy sidebar
    readonly property color cContentBg: "#1b1e24"       // Steam main content background
    readonly property color cBorder: "#2a2f3a"          // Subtle slate border
    readonly property color cCardBg: "#21252d"          // Steam settings card background
    readonly property color cCardBorder: "#2e3440"      // Card outline
    readonly property color cItemHover: "#1e232b"       // Sidebar hover
    readonly property color cItemActive: "#282c35"      // Sidebar active pill
    readonly property color cSteamBlue: "#1a9fff"       // Steam vibrant blue accent
    readonly property color cTextWhite: "#F2F3F5"       // Primary text
    readonly property color cTextDim: "#949BA4"         // Secondary text
    readonly property color cTextMute: "#80848E"        // Helper / label text

    function unlockDevMode() {
        if (!fluxUser) return
        if (fluxUser.verifyDevPin(devPinInput.text.trim())) {
            devPinErrorText = ""
            devPinInput.text = ""
            panel.showPinPrompt = false
        } else {
            devPinErrorText = "Incorrect 4-digit PIN."
        }
    }

    function testServer(num) {
        var targetUrl = ""
        if (num === 1) {
            targetUrl = "http://172.16.50.14/"
            server1Testing = true
            server1Status = "Checking server response..."
        } else if (num === 2) {
            targetUrl = "http://10.16.100.244/login.php"
            server2Testing = true
            server2Status = "Checking server response..."
        } else if (num === 3) {
            targetUrl = "http://new.circleftp.net:5000/api/categories"
            server3Testing = true
            server3Status = "Checking server response..."
        }

        var xhr = new XMLHttpRequest()
        var startTime = Date.now()
        xhr.open("GET", targetUrl, true)
        xhr.timeout = 3500

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                var elapsed = Date.now() - startTime
                var success = (xhr.status >= 200 && xhr.status < 400)
                var statusMsg = success
                    ? ("Online · Latency " + elapsed + " ms")
                    : ("Server unreachable (" + (xhr.status > 0 ? ("HTTP " + xhr.status) : "timeout / network error") + ")")

                if (num === 1) {
                    server1Testing = false
                    server1Success = success
                    server1Status = statusMsg
                } else if (num === 2) {
                    server2Testing = false
                    server2Success = success
                    server2Status = statusMsg
                } else if (num === 3) {
                    server3Testing = false
                    server3Success = success
                    server3Status = statusMsg
                }
            }
        }

        xhr.ontimeout = function() {
            var timeoutMsg = "Request timed out (> 3.5s)"
            if (num === 1) {
                server1Testing = false
                server1Success = false
                server1Status = timeoutMsg
            } else if (num === 2) {
                server2Testing = false
                server2Success = false
                server2Status = timeoutMsg
            } else if (num === 3) {
                server3Testing = false
                server3Success = false
                server3Status = timeoutMsg
            }
        }

        xhr.onerror = function() {
            var errMsg = "Connection failed"
            if (num === 1) {
                server1Testing = false
                server1Success = false
                server1Status = errMsg
            } else if (num === 2) {
                server2Testing = false
                server2Success = false
                server2Status = errMsg
            } else if (num === 3) {
                server3Testing = false
                server3Success = false
                server3Status = errMsg
            }
        }

        xhr.send()
    }

    function toggleServer(num, targetState) {
        if (!fluxUser) return
        if (!targetState) {
            fluxUser.setServerEnabled(num, false)
            if (num === 1) server1Status = "Server turned off."
            else if (num === 2) server2Status = "Server turned off."
            else if (num === 3) server3Status = "Server turned off."
            return
        }

        if (num === 1) {
            server1Testing = true
            server1Status = "Testing connection to Dhaka Flix..."
        } else if (num === 2) {
            server2Testing = true
            server2Status = "Testing connection to ICC Flix..."
        } else if (num === 3) {
            server3Testing = true
            server3Status = "Testing connection to Circle FTP..."
        }

        var targetUrl = ""
        if (num === 1) {
            targetUrl = "http://172.16.50.14/"
        } else if (num === 2) {
            targetUrl = "http://10.16.100.244/login.php"
        } else if (num === 3) {
            targetUrl = "http://new.circleftp.net:5000/api/categories"
        }

        var xhr = new XMLHttpRequest()
        var startTime = Date.now()
        xhr.open("GET", targetUrl, true)
        xhr.timeout = 3500

        xhr.onreadystatechange = function() {
            if (xhr.readyState === XMLHttpRequest.DONE) {
                var elapsed = Date.now() - startTime
                var success = (xhr.status >= 200 && xhr.status < 400)
                var statusMsg = success
                    ? ("Online · Latency " + elapsed + " ms")
                    : ("Server unreachable (" + (xhr.status > 0 ? ("HTTP " + xhr.status) : "timeout / network error") + "). Cannot turn on.")

                if (num === 1) {
                    server1Testing = false
                    server1Success = success
                    server1Status = statusMsg
                } else if (num === 2) {
                    server2Testing = false
                    server2Success = success
                    server2Status = statusMsg
                } else if (num === 3) {
                    server3Testing = false
                    server3Success = success
                    server3Status = statusMsg
                }

                if (success) {
                    fluxUser.setServerEnabled(num, true)
                } else {
                    fluxUser.setServerEnabled(num, false)
                }
            }
        }

        xhr.ontimeout = function() {
            var timeoutMsg = "Request timed out (> 3.5s). Cannot turn on."
            if (num === 1) {
                server1Testing = false
                server1Success = false
                server1Status = timeoutMsg
            } else if (num === 2) {
                server2Testing = false
                server2Success = false
                server2Status = timeoutMsg
            } else if (num === 3) {
                server3Testing = false
                server3Success = false
                server3Status = timeoutMsg
            }
            fluxUser.setServerEnabled(num, false)
        }

        xhr.onerror = function() {
            var errMsg = "Connection failed / network unsupported. Cannot turn on."
            if (num === 1) {
                server1Testing = false
                server1Success = false
                server1Status = errMsg
            } else if (num === 2) {
                server2Testing = false
                server2Success = false
                server2Status = errMsg
            } else if (num === 3) {
                server3Testing = false
                server3Success = false
                server3Status = errMsg
            }
            fluxUser.setServerEnabled(num, false)
        }

        xhr.send()
    }

    Timer {
        id: customCodeSavedTimer
        interval: 2500
        repeat: false
        onTriggered: panel.customCodeSavedText = ""
    }

    readonly property bool atDefaultLocation: !!fluxDownloads
        && fluxDownloads.downloadLocation.toLowerCase() === fluxDownloads.defaultLocation.toLowerCase()

    // Escape: close a folder picker or dropdown first, then the dialog
    function dismiss() {
        if (srvDropdownPopup && srvDropdownPopup.visible) srvDropdownPopup.close()
        else if (targetPicker.visible) targetPicker.close()
        else if (picker.visible) picker.close()
        else panel.close()
    }

    onCurrentTabChanged: {
        if (srvDropdownPopup && srvDropdownPopup.visible) srvDropdownPopup.close()
    }
    onClosed: {
        if (srvDropdownPopup && srvDropdownPopup.visible) srvDropdownPopup.close()
    }

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min(920, (Overlay.overlay ? Overlay.overlay.width : 920) - 32)
    height: Math.min(620, (Overlay.overlay ? Overlay.overlay.height : 620) - 32)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onAboutToShow: {
        errorText = ""
        targetErrorText = ""
        serverTestStatus = ""
        serverTesting = false
        serverTestSuccess = false
        server1Testing = false
        server1Status = ""
        server1Success = false
        server2Testing = false
        server2Status = ""
        server2Success = false
        server3Testing = false
        server3Status = ""
        server3Success = false
        if (currentTab === "") currentTab = "server"
    }

    Overlay.modal: Rectangle { color: "#B3000000" }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160 }
        NumberAnimation { property: "scale"; from: 0.97; to: 1; duration: 180; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 120 }
    }

    background: Rectangle {
        radius: 8
        color: panel.cContentBg
        border.width: 1
        border.color: panel.cBorder
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

    // Steam-style Pill Toggle Switch component
    component SteamToggle: Item {
        id: stRoot

        property bool checked: false
        signal toggled(bool value)

        implicitWidth: 42
        implicitHeight: 22

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: stRoot.checked ? panel.cSteamBlue : "#3e4450"

            Behavior on color { ColorAnimation { duration: 160 } }

            Rectangle {
                width: 16
                height: 16
                radius: 8
                y: 3
                x: stRoot.checked ? parent.width - width - 3 : 3
                color: "#FFFFFF"

                Behavior on x { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            }
        }

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: stRoot.toggled(!stRoot.checked)
        }
    }

    // Steam-style Button component
    component SteamButton: Rectangle {
        id: btnRoot

        property string text: ""
        property string iconName: ""
        property bool isAccent: false
        signal clicked()

        implicitWidth: Math.max(88, btnRow.implicitWidth + 24)
        implicitHeight: 32
        radius: 4
        color: btnMouse.pressed ? (isAccent ? "#1177cc" : "#1a2029")
             : btnMouse.containsMouse ? (isAccent ? "#29a8ff" : "#2e3542")
             : (isAccent ? panel.cSteamBlue : "#242933")
        border.width: 1
        border.color: isAccent ? panel.cSteamBlue : "#3a4150"

        Behavior on color { ColorAnimation { duration: 120 } }

        RowLayout {
            id: btnRow
            anchors.centerIn: parent
            spacing: 6

            FluxIcon {
                visible: btnRoot.iconName !== ""
                name: btnRoot.iconName
                size: 14
                color: btnRoot.isAccent ? "#FFFFFF" : panel.cTextWhite
            }

            Text {
                text: btnRoot.text
                color: btnRoot.isAccent ? "#FFFFFF" : panel.cTextWhite
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }

        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btnRoot.clicked()
        }
    }

    // Steam-style Setting Card Row
    component SteamCard: Rectangle {
        default property alias content: cardLayout.children
        property alias rowSpacing: cardLayout.spacing

        Layout.fillWidth: true
        implicitHeight: cardLayout.implicitHeight + 24
        radius: 4
        color: panel.cCardBg
        border.width: 1
        border.color: panel.cCardBorder

        ColumnLayout {
            id: cardLayout
            anchors.fill: parent
            anchors.margins: 12
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 8
        }
    }

    // Steam Section Header Label
    component SteamSectionTitle: Text {
        color: panel.cTextWhite
        font.pixelSize: 15
        font.weight: Font.DemiBold
        Layout.topMargin: 16
        Layout.bottomMargin: 4
    }

    contentItem: RowLayout {
        spacing: 0

        // ====================================================================
        // LEFT SIDEBAR: Steam Navigation
        // ====================================================================
        Rectangle {
            Layout.preferredWidth: 220
            Layout.fillHeight: true
            color: panel.cSidebarBg
            border.width: 0

            // Right border line separating sidebar from content
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: panel.cBorder
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header: "FLUX SETTINGS"
                Item {
                    Layout.fillWidth: true
                    implicitHeight: 56

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        text: "FLUX SETTINGS"
                        color: panel.cSteamBlue
                        font.pixelSize: 13
                        font.weight: Font.Bold
                        font.letterSpacing: 1.1
                    }
                }

                // Nav Items List
                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 2
                    interactive: false

                    model: [
                        { id: "server",    label: "Media Server",       icon: "server" },
                        { id: "downloads", label: "Downloads",          icon: "download" },
                        { id: "playback",  label: "Playback",           icon: "film" },
                        { id: "storage",   label: "Library & Storage",  icon: "storage" },
                        { id: "system",    label: "System",             icon: "settings" },
                        { id: "developer", label: "Developer",          icon: "shield" },
                        { id: "about",     label: "About",              icon: "info" }
                    ]

                    delegate: Item {
                        width: navList.width
                        height: 36

                        readonly property bool isSelected: panel.currentTab === modelData.id

                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            radius: 4
                            color: isSelected ? panel.cItemActive
                                 : navMouse.containsMouse ? panel.cItemHover
                                 : "transparent"

                            Behavior on color { ColorAnimation { duration: 100 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12

                                FluxIcon {
                                    name: modelData.icon
                                    size: 16
                                    color: isSelected ? "#FFFFFF"
                                         : navMouse.containsMouse ? "#c7d5e0"
                                         : panel.cTextMute
                                    Layout.alignment: Qt.AlignVCenter
                                }

                                Text {
                                    text: modelData.label
                                    color: isSelected ? "#FFFFFF"
                                         : navMouse.containsMouse ? "#c7d5e0"
                                         : panel.cTextDim
                                    font.pixelSize: 13
                                    font.weight: isSelected ? Font.DemiBold : Font.Normal
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                }
                            }

                            MouseArea {
                                id: navMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: panel.currentTab = modelData.id
                            }
                        }
                    }
                }

                // Footer build version string
                Item {
                    Layout.fillWidth: true
                    implicitHeight: 38

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        text: "FLUX v2.2.5"
                        color: "#545d6b"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                    }
                }
            }
        }

        // ====================================================================
        // RIGHT MAIN CONTENT PANE
        // ====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: panel.cContentBg

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Top Bar with Category Header & Steam Close Button
                Item {
                    Layout.fillWidth: true
                    implicitHeight: 56

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 28
                        anchors.verticalCenter: parent.verticalCenter
                        text: {
                            switch (panel.currentTab) {
                                case "server":    return "Media Server"
                                case "downloads": return "Downloads"
                                case "playback":  return "Playback"
                                case "storage":   return "Library & Storage"
                                case "system":    return "System & Startup"
                                case "developer": return "Developer Options"
                                case "about":     return "About FLUX"
                                default:          return "Settings"
                            }
                        }
                        color: panel.cTextWhite
                        font.pixelSize: 22
                        font.weight: Font.Bold
                    }

                    // Window Close Button
                    Rectangle {
                        anchors.right: parent.right
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32
                        height: 32
                        radius: 4
                        color: closeMouse.pressed ? "#171a21"
                             : closeMouse.containsMouse ? "#2c323d"
                             : "transparent"

                        FluxIcon {
                            anchors.centerIn: parent
                            name: "close"
                            size: 14
                            color: closeMouse.containsMouse ? "#FFFFFF" : panel.cTextDim
                        }

                        MouseArea {
                            id: closeMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: panel.close()
                        }
                    }
                }

                // Divider line below category title
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: panel.cBorder
                }

                // Scrollable Content Stack
                Flickable {
                    id: bodyScroller
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: width
                    contentHeight: contentCol.implicitHeight + 40
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ScrollBar.vertical: FluxScrollBar { }

                    ColumnLayout {
                        id: contentCol
                        width: bodyScroller.width - 56
                        anchors.horizontalCenter: parent.horizontalCenter
                        spacing: 12

                        Item { implicitHeight: 8 }

                        // ====================================================
                        // 1. MEDIA SERVER TAB
                        // ====================================================
                        ColumnLayout {
                            visible: panel.currentTab === "server"
                            Layout.fillWidth: true
                            spacing: 12

                            SteamSectionTitle { text: "Active Server Mirror" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Primary Media Server"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Select the BDIX streaming mirror for browsing movies, series, and top picks."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }

                                    // Server Selector Dropdown
                                    Item {
                                        id: srvDropdownContainer
                                        implicitWidth: 220
                                        implicitHeight: 34

                                        Rectangle {
                                            id: srvBtn
                                            anchors.fill: parent
                                            radius: 4
                                            color: srvMouse.containsMouse || srvDropdownPopup.visible ? "#282e3a" : "#171a21"
                                            border.width: 1
                                            border.color: srvDropdownPopup.visible ? panel.cSteamBlue : "#384152"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 12
                                                anchors.rightMargin: 10

                                                Text {
                                                    text: {
                                                        if (!fluxUser || fluxUser.enabledServerCount === 0) return "No Servers Enabled"
                                                        if (fluxUser.activeServer === 3 && fluxUser.server3Enabled) return "Server 3 (Circle FTP)"
                                                        if (fluxUser.activeServer === 2 && fluxUser.server2Enabled) return "Server 2 (ICC Flix)"
                                                        if (fluxUser.activeServer === 1 && fluxUser.server1Enabled) return "Server 1 (Dhaka Flix)"
                                                        return "Select Server"
                                                    }
                                                    color: panel.cTextWhite
                                                    font.pixelSize: 12
                                                    font.weight: Font.DemiBold
                                                    Layout.fillWidth: true
                                                }

                                                FluxIcon {
                                                    name: "chevronDown"
                                                    size: 12
                                                    color: panel.cSteamBlue
                                                    rotation: srvDropdownPopup.visible ? 180 : 0
                                                    Behavior on rotation { NumberAnimation { duration: 150 } }
                                                }
                                            }

                                            MouseArea {
                                                id: srvMouse
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (srvDropdownPopup.visible) {
                                                        srvDropdownPopup.close()
                                                    } else {
                                                        srvDropdownPopup.open()
                                                    }
                                                }
                                            }
                                        }

                                        // Dropdown menu popup (QtQuick.Controls Popup overlays above all layout cards)
                                        Popup {
                                            id: srvDropdownPopup
                                            parent: srvBtn
                                            x: 0
                                            y: srvBtn.height + 4
                                            width: srvBtn.width
                                            padding: 4
                                            modal: false
                                            focus: true
                                            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

                                            enter: Transition {
                                                NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 120 }
                                                NumberAnimation { property: "scale"; from: 0.98; to: 1.0; duration: 120; easing.type: Easing.OutCubic }
                                            }
                                            exit: Transition {
                                                NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 90 }
                                            }

                                            background: Rectangle {
                                                color: "#171a21"
                                                radius: 4
                                                border.width: 1
                                                border.color: "#384152"
                                            }

                                            contentItem: ColumnLayout {
                                                spacing: 2

                                                // Option 1: Dhaka Flix
                                                Rectangle {
                                                    visible: fluxUser ? fluxUser.server1Enabled : true
                                                    Layout.fillWidth: true
                                                    implicitHeight: 32
                                                    radius: 3
                                                    color: opt1Mouse.containsMouse ? "#282e3a" : (fluxUser && fluxUser.activeServer === 1 ? "#1f2633" : "transparent")

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 10
                                                        anchors.rightMargin: 10
                                                        spacing: 8

                                                        Text {
                                                            text: (fluxUser && fluxUser.activeServer === 1) ? "\u2713" : ""
                                                            color: panel.cSteamBlue
                                                            font.pixelSize: 12
                                                            font.weight: Font.Bold
                                                            Layout.preferredWidth: 12
                                                        }

                                                        Text {
                                                            text: "Server 1 (Dhaka Flix)"
                                                            color: (fluxUser && fluxUser.activeServer === 1) ? panel.cTextWhite : panel.cTextDim
                                                            font.pixelSize: 12
                                                            font.weight: (fluxUser && fluxUser.activeServer === 1) ? Font.DemiBold : Font.Normal
                                                            Layout.fillWidth: true
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: opt1Mouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (fluxUser) fluxUser.activeServer = 1
                                                            srvDropdownPopup.close()
                                                            panel.serverTestStatus = ""
                                                        }
                                                    }
                                                }

                                                // Option 2: ICC Flix
                                                Rectangle {
                                                    visible: fluxUser ? fluxUser.server2Enabled : true
                                                    Layout.fillWidth: true
                                                    implicitHeight: 32
                                                    radius: 3
                                                    color: opt2Mouse.containsMouse ? "#282e3a" : (fluxUser && fluxUser.activeServer === 2 ? "#1f2633" : "transparent")

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 10
                                                        anchors.rightMargin: 10
                                                        spacing: 8

                                                        Text {
                                                            text: (fluxUser && fluxUser.activeServer === 2) ? "\u2713" : ""
                                                            color: panel.cSteamBlue
                                                            font.pixelSize: 12
                                                            font.weight: Font.Bold
                                                            Layout.preferredWidth: 12
                                                        }

                                                        Text {
                                                            text: "Server 2 (ICC Flix)"
                                                            color: (fluxUser && fluxUser.activeServer === 2) ? panel.cTextWhite : panel.cTextDim
                                                            font.pixelSize: 12
                                                            font.weight: (fluxUser && fluxUser.activeServer === 2) ? Font.DemiBold : Font.Normal
                                                            Layout.fillWidth: true
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: opt2Mouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (fluxUser) fluxUser.activeServer = 2
                                                            srvDropdownPopup.close()
                                                            panel.serverTestStatus = ""
                                                        }
                                                    }
                                                }

                                                // Option 3: Circle FTP
                                                Rectangle {
                                                    visible: fluxUser ? fluxUser.server3Enabled : true
                                                    Layout.fillWidth: true
                                                    implicitHeight: 32
                                                    radius: 3
                                                    color: opt3Mouse.containsMouse ? "#282e3a" : (fluxUser && fluxUser.activeServer === 3 ? "#1f2633" : "transparent")

                                                    RowLayout {
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 10
                                                        anchors.rightMargin: 10
                                                        spacing: 8

                                                        Text {
                                                            text: (fluxUser && fluxUser.activeServer === 3) ? "\u2713" : ""
                                                            color: panel.cSteamBlue
                                                            font.pixelSize: 12
                                                            font.weight: Font.Bold
                                                            Layout.preferredWidth: 12
                                                        }

                                                        Text {
                                                            text: "Server 3 (Circle FTP)"
                                                            color: (fluxUser && fluxUser.activeServer === 3) ? panel.cTextWhite : panel.cTextDim
                                                            font.pixelSize: 12
                                                            font.weight: (fluxUser && fluxUser.activeServer === 3) ? Font.DemiBold : Font.Normal
                                                            Layout.fillWidth: true
                                                        }
                                                    }

                                                    MouseArea {
                                                        id: opt3Mouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (fluxUser) fluxUser.activeServer = 3
                                                            srvDropdownPopup.close()
                                                            panel.serverTestStatus = ""
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Host addresses info card
                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        RowLayout {
                                            spacing: 8
                                            Rectangle { width: 6; height: 6; radius: 3; color: (fluxUser && fluxUser.activeServer === 1) ? panel.cSteamBlue : panel.cTextMute }
                                            Text { text: "Server 1: Dhaka Flix"; color: panel.cTextWhite; font.pixelSize: 12; font.weight: Font.DemiBold }
                                            Text { text: "http://172.16.50.14/"; color: panel.cTextMute; font.family: Theme.monoFamily; font.pixelSize: 11 }
                                        }

                                        RowLayout {
                                            spacing: 8
                                            Rectangle { width: 6; height: 6; radius: 3; color: (fluxUser && fluxUser.activeServer === 2) ? panel.cSteamBlue : panel.cTextMute }
                                            Text { text: "Server 2: ICC Flix"; color: panel.cTextWhite; font.pixelSize: 12; font.weight: Font.DemiBold }
                                            Text { text: "http://10.16.100.244/"; color: panel.cTextMute; font.family: Theme.monoFamily; font.pixelSize: 11 }
                                        }

                                        RowLayout {
                                            spacing: 8
                                            Rectangle { width: 6; height: 6; radius: 3; color: (fluxUser && fluxUser.activeServer === 3) ? panel.cSteamBlue : panel.cTextMute }
                                            Text { text: "Server 3: Circle FTP"; color: panel.cTextWhite; font.pixelSize: 12; font.weight: Font.DemiBold }
                                            Text { text: "http://new.circleftp.net/"; color: panel.cTextMute; font.family: Theme.monoFamily; font.pixelSize: 11 }
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                SteamSectionTitle {
                                    text: "Connection & Latency Diagnostic"
                                    Layout.fillWidth: true
                                }
                                SteamButton {
                                    text: (panel.server1Testing || panel.server2Testing || panel.server3Testing) ? "Testing..." : "Test All"
                                    iconName: "refresh"
                                    isAccent: false
                                    onClicked: {
                                        panel.testServer(1)
                                        panel.testServer(2)
                                        panel.testServer(3)
                                    }
                                }
                            }

                            SteamCard {
                                // Row 1: Server 1 (Dhaka Flix)
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 14

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: !(fluxUser && fluxUser.server1Enabled) ? panel.cTextMute
                                             : panel.server1Status === "" ? Theme.success
                                             : panel.server1Success ? Theme.success
                                             : Theme.danger
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        RowLayout {
                                            spacing: 8
                                            Text {
                                                text: "Server 1: Dhaka Flix"
                                                color: (fluxUser && fluxUser.server1Enabled) ? panel.cTextWhite : panel.cTextDim
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                            }

                                            Text {
                                                text: "http://172.16.50.14/"
                                                color: panel.cTextMute
                                                font.family: Theme.monoFamily
                                                font.pixelSize: 11
                                            }
                                        }

                                        Text {
                                            text: panel.server1Status !== "" ? panel.server1Status
                                                : (!(fluxUser && fluxUser.server1Enabled) ? "Server turned off." : "Online · Ready for streaming and search.")
                                            color: !(fluxUser && fluxUser.server1Enabled) ? panel.cTextMute
                                                 : panel.server1Status === "" ? Theme.success
                                                 : panel.server1Success ? Theme.success
                                                 : Theme.danger
                                            font.pixelSize: 12
                                            font.weight: panel.server1Status !== "" ? Font.DemiBold : Font.Normal
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }

                                    SteamButton {
                                        text: panel.server1Testing ? "Testing..." : "Test Latency"
                                        iconName: panel.server1Testing ? "refresh" : "swap"
                                        isAccent: false
                                        onClicked: panel.testServer(1)
                                    }

                                    ToggleSwitch {
                                        checked: fluxUser ? fluxUser.server1Enabled : false
                                        enabled: !panel.server1Testing
                                        onToggled: function(val) {
                                            panel.toggleServer(1, val)
                                        }
                                    }
                                }

                                // Separator
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: panel.cCardBorder
                                }

                                // Row 2: Server 2 (ICC Flix)
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 14

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: !(fluxUser && fluxUser.server2Enabled) ? panel.cTextMute
                                             : panel.server2Status === "" ? Theme.success
                                             : panel.server2Success ? Theme.success
                                             : Theme.danger
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        RowLayout {
                                            spacing: 8
                                            Text {
                                                text: "Server 2: ICC Flix"
                                                color: (fluxUser && fluxUser.server2Enabled) ? panel.cTextWhite : panel.cTextDim
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                            }

                                            Text {
                                                text: "http://10.16.100.244/"
                                                color: panel.cTextMute
                                                font.family: Theme.monoFamily
                                                font.pixelSize: 11
                                            }
                                        }

                                        Text {
                                            text: panel.server2Status !== "" ? panel.server2Status
                                                : (!(fluxUser && fluxUser.server2Enabled) ? "Server turned off." : "Online · Ready for streaming and search.")
                                            color: !(fluxUser && fluxUser.server2Enabled) ? panel.cTextMute
                                                 : panel.server2Status === "" ? Theme.success
                                                 : panel.server2Success ? Theme.success
                                                 : Theme.danger
                                            font.pixelSize: 12
                                            font.weight: panel.server2Status !== "" ? Font.DemiBold : Font.Normal
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }

                                    SteamButton {
                                        text: panel.server2Testing ? "Testing..." : "Test Latency"
                                        iconName: panel.server2Testing ? "refresh" : "swap"
                                        isAccent: false
                                        onClicked: panel.testServer(2)
                                    }

                                    ToggleSwitch {
                                        checked: fluxUser ? fluxUser.server2Enabled : false
                                        enabled: !panel.server2Testing
                                        onToggled: function(val) {
                                            panel.toggleServer(2, val)
                                        }
                                    }
                                }

                                // Separator
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: panel.cCardBorder
                                }

                                // Row 3: Server 3 (Circle FTP)
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 14

                                    Rectangle {
                                        width: 8
                                        height: 8
                                        radius: 4
                                        color: !(fluxUser && fluxUser.server3Enabled) ? panel.cTextMute
                                             : panel.server3Status === "" ? Theme.success
                                             : panel.server3Success ? Theme.success
                                             : Theme.danger
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        RowLayout {
                                            spacing: 8
                                            Text {
                                                text: "Server 3: Circle FTP"
                                                color: (fluxUser && fluxUser.server3Enabled) ? panel.cTextWhite : panel.cTextDim
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                            }

                                            Text {
                                                text: "http://new.circleftp.net/"
                                                color: panel.cTextMute
                                                font.family: Theme.monoFamily
                                                font.pixelSize: 11
                                            }
                                        }

                                        Text {
                                            text: panel.server3Status !== "" ? panel.server3Status
                                                : (!(fluxUser && fluxUser.server3Enabled) ? "Server turned off." : "Online · Ready for streaming and search.")
                                            color: !(fluxUser && fluxUser.server3Enabled) ? panel.cTextMute
                                                 : panel.server3Status === "" ? Theme.success
                                                 : panel.server3Success ? Theme.success
                                                 : Theme.danger
                                            font.pixelSize: 12
                                            font.weight: panel.server3Status !== "" ? Font.DemiBold : Font.Normal
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }

                                    SteamButton {
                                        text: panel.server3Testing ? "Testing..." : "Test Latency"
                                        iconName: panel.server3Testing ? "refresh" : "swap"
                                        isAccent: false
                                        onClicked: panel.testServer(3)
                                    }

                                    ToggleSwitch {
                                        checked: fluxUser ? fluxUser.server3Enabled : false
                                        enabled: !panel.server3Testing
                                        onToggled: function(val) {
                                            panel.toggleServer(3, val)
                                        }
                                    }
                                }
                            }
                        }

                        // ====================================================
                        // 2. DOWNLOADS TAB (Steam-style 1:1)
                        // ====================================================
                        ColumnLayout {
                            visible: panel.currentTab === "downloads"
                            Layout.fillWidth: true
                            spacing: 12

                            SteamSectionTitle { text: "Download Location" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Text {
                                            text: "Downloads folder"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        // Monospace path preview box
                                        Rectangle {
                                            Layout.fillWidth: true
                                            implicitHeight: 32
                                            radius: 4
                                            color: "#171a21"
                                            border.width: 1
                                            border.color: panel.errorText !== "" ? Theme.danger : "#2f3644"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 10
                                                anchors.rightMargin: 10

                                                Text {
                                                    text: fluxDownloads ? fluxDownloads.downloadLocation : "C:\\Users\\Downloads"
                                                    color: panel.cTextWhite
                                                    font.family: Theme.monoFamily
                                                    font.pixelSize: 11
                                                    elide: Text.ElideMiddle
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }

                                        Text {
                                            visible: panel.errorText !== ""
                                            text: panel.errorText
                                            color: Theme.danger
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }

                                    ColumnLayout {
                                        spacing: 6
                                        Layout.alignment: Qt.AlignBottom

                                        RowLayout {
                                            spacing: 8

                                            SteamButton {
                                                text: "Change..."
                                                iconName: "folder"
                                                onClicked: {
                                                    if (fluxDownloads) {
                                                        var p = fluxDownloads.browseFolder("Select Downloads Folder")
                                                        if (p) fluxDownloads.changeLocation(p)
                                                    }
                                                }
                                            }

                                            SteamButton {
                                                visible: !panel.atDefaultLocation
                                                text: "Reset"
                                                onClicked: {
                                                    if (fluxDownloads) fluxDownloads.resetLocation()
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Subfolder Structure
                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Text {
                                            text: "Automated Subfolder Sorting"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Downloaded media is automatically routed into dedicated subfolders:"
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }

                                        RowLayout {
                                            spacing: 8
                                            Text { text: "Series:"; color: panel.cTextDim; font.pixelSize: 11; font.weight: Font.DemiBold }
                                            Text { text: (fluxDownloads ? fluxDownloads.downloadLocation : "") + "\\Series"; color: panel.cTextWhite; font.family: Theme.monoFamily; font.pixelSize: 11; elide: Text.ElideMiddle }
                                        }

                                        RowLayout {
                                            spacing: 8
                                            Text { text: "Movies:"; color: panel.cTextDim; font.pixelSize: 11; font.weight: Font.DemiBold }
                                            Text { text: (fluxDownloads ? fluxDownloads.downloadLocation : "") + "\\Movies"; color: panel.cTextWhite; font.family: Theme.monoFamily; font.pixelSize: 11; elide: Text.ElideMiddle }
                                        }
                                    }
                                }
                            }

                            SteamSectionTitle { text: "Download Options" }

                            // Steam Toggle Rows
                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Display download rates in bits per second"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Show speeds in Mbps rather than MB/s in the download manager."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }
                                    }

                                    SteamToggle {
                                        checked: false
                                        onToggled: function(val) { checked = val }
                                    }
                                }
                            }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Throttle downloads while streaming"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Limit background file transfers so active video playback maintains high bandwidth."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }
                                    }

                                    SteamToggle {
                                        checked: true
                                        onToggled: function(val) { checked = val }
                                    }
                                }
                            }
                        }

                        // ====================================================
                        // 3. PLAYBACK TAB
                        // ====================================================
                        ColumnLayout {
                            visible: panel.currentTab === "playback"
                            Layout.fillWidth: true
                            spacing: 12

                            SteamSectionTitle { text: "Playback Behavior" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Autoplay next episode"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Continue with the next available episode in the folder when the current one ends."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }
                                    }

                                    SteamToggle {
                                        checked: fluxUser ? fluxUser.autoplayNext : true
                                        onToggled: function(val) {
                                            if (fluxUser) fluxUser.autoplayNext = val
                                        }
                                    }
                                }
                            }

                            SteamSectionTitle { text: "Dialogue & Subtitle Preference" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Prioritize full dialogue tracks over signs"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Automatically prefer main English dialogue subtitles over auxiliary Songs & Signs."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }
                                    }

                                    SteamToggle {
                                        checked: true
                                        onToggled: function(val) { checked = val }
                                    }
                                }
                            }
                        }

                        // ====================================================
                        // 4. LIBRARY & STORAGE TAB
                        // ====================================================
                        ColumnLayout {
                            visible: panel.currentTab === "storage"
                            Layout.fillWidth: true
                            spacing: 12

                            SteamSectionTitle { text: "Local Scanned Folders" }

                            SteamCard {
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    RowLayout {
                                        Layout.fillWidth: true

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                text: "Additional Library Targets"
                                                color: panel.cTextWhite
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                            }

                                            Text {
                                                text: "Folders on this PC scanned for local offline movies and series."
                                                color: panel.cTextMute
                                                font.pixelSize: 12
                                            }
                                        }

                                        SteamButton {
                                            text: "Add Folder..."
                                            iconName: "folder"
                                            onClicked: {
                                                if (fluxDownloads) fluxDownloads.promptAddTarget()
                                            }
                                        }
                                    }

                                    Text {
                                        visible: panel.targetErrorText !== ""
                                        text: panel.targetErrorText
                                        color: Theme.danger
                                        font.pixelSize: 11
                                        font.weight: Font.DemiBold
                                    }

                                    // List of targets
                                    ListView {
                                        id: targetList
                                        Layout.fillWidth: true
                                        implicitHeight: Math.min(180, contentHeight)
                                        clip: true
                                        spacing: 6
                                        model: fluxDownloads ? fluxDownloads.targets : []

                                        delegate: Rectangle {
                                            width: targetList.width
                                            height: 36
                                            radius: 4
                                            color: "#171a21"
                                            border.width: 1
                                            border.color: "#2a303c"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.leftMargin: 12
                                                anchors.rightMargin: 8
                                                spacing: 8

                                                FluxIcon {
                                                    name: "folder"
                                                    size: 14
                                                    color: panel.cSteamBlue
                                                }

                                                Text {
                                                    text: modelData
                                                    color: panel.cTextWhite
                                                    font.family: Theme.monoFamily
                                                    font.pixelSize: 11
                                                    elide: Text.ElideMiddle
                                                    Layout.fillWidth: true
                                                }

                                                Rectangle {
                                                    width: 26
                                                    height: 26
                                                    radius: 3
                                                    color: delMouse.containsMouse ? "#3b2025" : "transparent"

                                                    FluxIcon {
                                                        anchors.centerIn: parent
                                                        name: "close"
                                                        size: 12
                                                        color: delMouse.containsMouse ? Theme.danger : panel.cTextMute
                                                    }

                                                    MouseArea {
                                                        id: delMouse
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            if (fluxDownloads) fluxDownloads.removeTarget(modelData)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Text {
                                        visible: !fluxDownloads || !fluxDownloads.targets || fluxDownloads.targets.length === 0
                                        text: "No extra folders added yet."
                                        color: panel.cTextMute
                                        font.pixelSize: 12
                                        font.italic: true
                                        Layout.topMargin: 4
                                    }
                                }
                            }

                            SteamSectionTitle { text: "Cache & Temporary Files" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Stream & Thumbnail Cache"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Storage used: " + (fluxCache ? fluxCache.sizeText : "0 B")
                                            color: panel.cSteamBlue
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: fluxDownloads ? (fluxDownloads.downloadLocation + "\\.cache") : ""
                                            color: panel.cTextMute
                                            font.family: Theme.monoFamily
                                            font.pixelSize: 11
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }
                                    }

                                    SteamButton {
                                        text: "Clear Cache"
                                        iconName: "refresh"
                                        onClicked: {
                                            if (fluxCache) fluxCache.clearCache()
                                        }
                                    }
                                }
                            }
                        }

                        // ====================================================
                        // 5. SYSTEM TAB
                        // ====================================================
                        ColumnLayout {
                            visible: panel.currentTab === "system"
                            Layout.fillWidth: true
                            spacing: 12

                            SteamSectionTitle { text: "Startup & Window Behavior" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Start FLUX when Windows boots up"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Automatically launch FLUX into the system tray when Windows starts."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }
                                    }

                                    SteamToggle {
                                        checked: !!fluxSystem && fluxSystem.autostart
                                        onToggled: function(val) {
                                            if (fluxSystem) fluxSystem.autostart = val
                                        }
                                    }
                                }
                            }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Keep FLUX running in tray when closed"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "Clicking the close button keeps FLUX running in the Windows notification tray."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }
                                    }

                                    SteamToggle {
                                        checked: !fluxSystem || fluxSystem.closeToTray
                                        onToggled: function(val) {
                                            if (fluxSystem) fluxSystem.closeToTray = val
                                        }
                                    }
                                }
                            }
                        }

                        // ====================================================
                        // 6. DEVELOPER TAB
                        // ====================================================
                        ColumnLayout {
                            visible: panel.currentTab === "developer"
                            Layout.fillWidth: true
                            spacing: 12

                            SteamSectionTitle { text: "Developer Security" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "Developer Mode"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: (fluxUser && fluxUser.devModeUnlocked)
                                                ? "Developer mode unlocked. Diagnostic tools and room code overrides active."
                                                : "Protected mode. Enter 4-digit PIN to access internal diagnostics."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                        }
                                    }

                                    SteamToggle {
                                        checked: !!fluxUser && fluxUser.devModeUnlocked
                                        onToggled: function(val) {
                                            if (!val) {
                                                if (fluxUser) fluxUser.devModeUnlocked = false
                                                panel.showPinPrompt = false
                                            } else {
                                                panel.showPinPrompt = true
                                                panel.devPinErrorText = ""
                                            }
                                        }
                                    }
                                }
                            }

                            // PIN prompt card
                            SteamCard {
                                visible: (!fluxUser || !fluxUser.devModeUnlocked) && panel.showPinPrompt

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Text {
                                            text: "Enter 4-Digit Developer PIN"
                                            color: panel.cTextWhite
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                        }

                                        RowLayout {
                                            spacing: 8

                                            Rectangle {
                                                width: 130
                                                height: 32
                                                radius: 4
                                                color: "#171a21"
                                                border.width: 1
                                                border.color: panel.devPinErrorText !== "" ? Theme.danger : "#384152"

                                                TextInput {
                                                    id: devPinInput
                                                    anchors.fill: parent
                                                    anchors.leftMargin: 8
                                                    anchors.rightMargin: 8
                                                    verticalAlignment: TextInput.AlignVCenter
                                                    color: panel.cTextWhite
                                                    echoMode: TextInput.Password
                                                    maximumLength: 4
                                                    font.pixelSize: 14
                                                    font.family: Theme.monoFamily
                                                    onAccepted: panel.unlockDevMode()
                                                }
                                            }

                                            SteamButton {
                                                text: "Unlock"
                                                isAccent: true
                                                onClicked: panel.unlockDevMode()
                                            }

                                            SteamButton {
                                                text: "Cancel"
                                                onClicked: panel.showPinPrompt = false
                                            }
                                        }

                                        Text {
                                            visible: panel.devPinErrorText !== ""
                                            text: panel.devPinErrorText
                                            color: Theme.danger
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }
                            }

                            // Unlocked developer settings
                            ColumnLayout {
                                visible: !!fluxUser && fluxUser.devModeUnlocked
                                Layout.fillWidth: true
                                spacing: 12

                                SteamSectionTitle { text: "Teleparty Room Code Overrides" }

                                SteamCard {
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 16

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Text {
                                                text: "Use Personal Custom Room Code"
                                                color: panel.cTextWhite
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                            }

                                            Text {
                                                text: "Host Teleparty sessions with your fixed vanity code instead of random generation."
                                                color: panel.cTextMute
                                                font.pixelSize: 12
                                            }
                                        }

                                        SteamToggle {
                                            checked: fluxUser ? fluxUser.useCustomTelepartyCode : false
                                            onToggled: function(val) {
                                                if (fluxUser) fluxUser.useCustomTelepartyCode = val
                                            }
                                        }
                                    }
                                }

                                SteamCard {
                                    visible: !!fluxUser && fluxUser.useCustomTelepartyCode

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12

                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4

                                            Text {
                                                text: "Personal Room Code"
                                                color: panel.cTextWhite
                                                font.pixelSize: 13
                                                font.weight: Font.DemiBold
                                            }

                                            RowLayout {
                                                spacing: 8

                                                Rectangle {
                                                    width: 180
                                                    height: 32
                                                    radius: 4
                                                    color: "#171a21"
                                                    border.width: 1
                                                    border.color: "#384152"

                                                    TextInput {
                                                        id: customCodeInput
                                                        anchors.fill: parent
                                                        anchors.leftMargin: 8
                                                        anchors.rightMargin: 8
                                                        verticalAlignment: TextInput.AlignVCenter
                                                        color: panel.cTextWhite
                                                        text: fluxUser ? fluxUser.customTelepartyCode : ""
                                                        maximumLength: 12
                                                        font.pixelSize: 13
                                                        font.family: Theme.monoFamily
                                                        font.capitalization: Font.AllUppercase
                                                        onAccepted: {
                                                            if (fluxUser) {
                                                                fluxUser.customTelepartyCode = text.trim().toUpperCase()
                                                                panel.customCodeSavedText = "Code saved: " + fluxUser.customTelepartyCode
                                                                customCodeSavedTimer.restart()
                                                            }
                                                        }
                                                    }
                                                }

                                                SteamButton {
                                                    text: "Save Code"
                                                    isAccent: true
                                                    onClicked: {
                                                        if (fluxUser) {
                                                            fluxUser.customTelepartyCode = customCodeInput.text.trim().toUpperCase()
                                                            panel.customCodeSavedText = "Code saved: " + fluxUser.customTelepartyCode
                                                            customCodeSavedTimer.restart()
                                                        }
                                                    }
                                                }
                                            }

                                            Text {
                                                visible: panel.customCodeSavedText !== ""
                                                text: panel.customCodeSavedText
                                                color: panel.cTextWhite
                                                font.pixelSize: 11
                                                font.weight: Font.DemiBold
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // ====================================================
                        // 7. ABOUT TAB
                        // ====================================================
                        ColumnLayout {
                            visible: panel.currentTab === "about"
                            Layout.fillWidth: true
                            spacing: 12

                            SteamSectionTitle { text: "About FLUX" }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    Image {
                                        source: "qrc:/FLUX/resources/flux.png"
                                        width: 52
                                        height: 52
                                        sourceSize: Qt.size(104, 104)
                                        fillMode: Image.PreserveAspectFit
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 2

                                        Text {
                                            text: "FLUX Media Player"
                                            color: panel.cTextWhite
                                            font.pixelSize: 16
                                            font.weight: Font.Bold
                                        }

                                        Text {
                                            text: "Version 2.2.5 (Windows x64)"
                                            color: panel.cSteamBlue
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }

                                        Text {
                                            text: "High-performance BDIX media player with Teleparty, Syncplay, and hardware-accelerated streaming."
                                            color: panel.cTextMute
                                            font.pixelSize: 12
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }
                                }
                            }

                            SteamCard {
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 16

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4

                                        Text { text: "Engine & Core Technologies"; color: panel.cTextWhite; font.pixelSize: 13; font.weight: Font.DemiBold }
                                        Text { text: "• LibVLC 3.0.21 Multimedia Pipeline with FreeType text scaler"; color: panel.cTextMute; font.pixelSize: 11 }
                                        Text { text: "• Qt 6.5.3 Quick Controls with C++ Accelerated QML Engine"; color: panel.cTextMute; font.pixelSize: 11 }
                                        Text { text: "• Discord & Steam Hybrid Design Language"; color: panel.cTextMute; font.pixelSize: 11 }
                                    }
                                }
                            }
                        }

                        Item { implicitHeight: 24 }
                    }
                }
            }
        }
    }
}
