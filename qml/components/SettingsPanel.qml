import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// App settings dialog (opened from the gear at the bottom-right).
Popup {
    id: panel

    property string errorText: ""
    property string targetErrorText: ""
    property string serverTestStatus: ""
    property bool serverTesting: false
    property bool serverTestSuccess: false
    property bool showPinPrompt: false
    property string devPinErrorText: ""
    property string customCodeSavedText: ""

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

    Timer {
        id: customCodeSavedTimer
        interval: 2500
        repeat: false
        onTriggered: panel.customCodeSavedText = ""
    }

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
        serverTestStatus = ""
        serverTesting = false
        serverTestSuccess = false
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

                // ---- Media Server ----
                SectionLabel {
                    text: "MEDIA SERVER"
                    Layout.leftMargin: 22
                    Layout.topMargin: 10
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 22
                    Layout.rightMargin: 22
                    Layout.topMargin: 10
                    implicitHeight: serverColumn.implicitHeight + 36
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: Theme.border

                    ColumnLayout {
                        id: serverColumn

                        anchors.fill: parent
                        anchors.margins: 18
                        spacing: 12

                        Text {
                            text: "Active Media Server"
                            color: Theme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        // Server choice buttons (Server 1 vs Server 2)
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Rectangle {
                                id: server1Btn
                                Layout.fillWidth: true
                                implicitHeight: 46
                                radius: 10
                                color: (fluxUser && fluxUser.activeServer === 1) ? Theme.accent : Theme.surface
                                border.width: 1
                                border.color: (fluxUser && fluxUser.activeServer === 1) ? Theme.accent : Theme.border

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (fluxUser) fluxUser.activeServer = 1
                                        panel.serverTestStatus = ""
                                    }
                                }

                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    FluxIcon {
                                        name: (fluxUser && fluxUser.activeServer === 1) ? "check" : "folder"
                                        size: 16
                                        color: (fluxUser && fluxUser.activeServer === 1) ? "#FFFFFF" : Theme.textDim
                                    }
                                    Text {
                                        text: "Server 1 (Dhaka Flix)"
                                        color: (fluxUser && fluxUser.activeServer === 1) ? "#FFFFFF" : Theme.text
                                        font.pixelSize: 13
                                        font.weight: (fluxUser && fluxUser.activeServer === 1) ? Font.DemiBold : Font.Normal
                                    }
                                }
                            }

                            Rectangle {
                                id: server2Btn
                                Layout.fillWidth: true
                                implicitHeight: 46
                                radius: 10
                                color: (fluxUser && fluxUser.activeServer === 2) ? Theme.accent : Theme.surface
                                border.width: 1
                                border.color: (fluxUser && fluxUser.activeServer === 2) ? Theme.accent : Theme.border

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (fluxUser) fluxUser.activeServer = 2
                                        panel.serverTestStatus = ""
                                    }
                                }

                                RowLayout {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    FluxIcon {
                                        name: (fluxUser && fluxUser.activeServer === 2) ? "check" : "folder"
                                        size: 16
                                        color: (fluxUser && fluxUser.activeServer === 2) ? "#FFFFFF" : Theme.textDim
                                    }
                                    Text {
                                        text: "Server 2 (ICC FTP)"
                                        color: (fluxUser && fluxUser.activeServer === 2) ? "#FFFFFF" : Theme.text
                                        font.pixelSize: 13
                                        font.weight: (fluxUser && fluxUser.activeServer === 2) ? Font.DemiBold : Font.Normal
                                    }
                                }
                            }
                        }

                        // Test server connection
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8

                            FluxButton {
                                text: panel.serverTesting ? "Testing\u2026" : "Test Connection"
                                iconName: "refresh"
                                iconSize: 16
                                implicitHeight: 36
                                enabled: !panel.serverTesting
                                onClicked: {
                                    if (!fluxUser) return
                                    panel.serverTesting = true
                                    panel.serverTestStatus = "Checking server connectivity..."
                                    var srv = fluxUser.activeServer
                                    fluxUser.testServerConnection(srv, "", function(ok, msg) {
                                        panel.serverTesting = false
                                        panel.serverTestSuccess = ok
                                        panel.serverTestStatus = ok ? ("Connected successfully! (" + (srv === 2 ? "ICC FTP" : "Dhaka Flix") + ")")
                                                                    : ("Connection failed: " + msg)
                                    })
                                }
                            }

                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            Layout.fillWidth: true
                            visible: panel.serverTestStatus.length > 0
                            text: panel.serverTestStatus
                            color: panel.serverTestSuccess ? Theme.success : Theme.danger
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }

                        Text {
                            Layout.fillWidth: true
                            text: fluxUser && fluxUser.activeServer === 2
                                  ? "Server 2 streams directly from ICC FTP nodes with instant video lookup and poster artwork."
                                  : "Server 1 connects to Dhaka Flix BDIX indexer using h5ai directory search."
                            color: Theme.textMute
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                        }
                    }
                }


                // ---- Downloads ----
                SectionLabel {
                    text: "DOWNLOADS"
                    Layout.leftMargin: 22
                    Layout.topMargin: 20
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

                SectionLabel {
                    text: "VIEWING CACHE"
                    Layout.leftMargin: 22
                    Layout.topMargin: 20
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 22
                    Layout.rightMargin: 22
                    Layout.topMargin: 10
                    implicitHeight: 76
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
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                text: "Recently watched media"
                                color: Theme.text
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            Text {
                                text: fluxCache ? fluxCache.sizeText : "0.00 GB of 10 GB"
                                color: Theme.textMute
                                font.pixelSize: 12
                            }
                        }

                        FluxButton {
                            text: "Clear cache"
                            variant: "secondary"
                            implicitHeight: 36
                            enabled: !!fluxCache && fluxCache.sizeBytes > 0
                            onClicked: fluxCache.clearCache()
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

                // ---- Developer Options ----
                SectionLabel {
                    text: "DEVELOPER OPTIONS"
                    Layout.leftMargin: 22
                    Layout.topMargin: 20
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 22
                    Layout.rightMargin: 22
                    Layout.topMargin: 10
                    Layout.bottomMargin: 24
                    implicitHeight: devColumn.implicitHeight + 32
                    radius: 14
                    color: Theme.surface
                    border.width: 1
                    border.color: (fluxUser && fluxUser.devModeUnlocked) ? "#4046D369" : Theme.border

                    ColumnLayout {
                        id: devColumn

                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 14

                        // --- 1. Developer Mode On / Off Row ---
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12

                            Rectangle {
                                width: 34
                                height: 34
                                radius: 17
                                color: (fluxUser && fluxUser.devModeUnlocked) ? "#2646D369" : "#1AFFFFFF"

                                FluxIcon {
                                    anchors.centerIn: parent
                                    name: (fluxUser && fluxUser.devModeUnlocked) ? "check" : "settings"
                                    size: 16
                                    color: (fluxUser && fluxUser.devModeUnlocked) ? "#46D369" : Theme.textDim
                                }
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.preferredWidth: 0
                                spacing: 2

                                Text {
                                    text: "Developer Mode"
                                    color: (fluxUser && fluxUser.devModeUnlocked) ? "#46D369" : Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    Layout.fillWidth: true
                                    text: (fluxUser && fluxUser.devModeUnlocked)
                                          ? "Advanced developer settings & room overrides active"
                                          : "Enable to access developer tools and custom room overrides"
                                    color: Theme.textMute
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                }
                            }

                            ToggleSwitch {
                                checked: !!fluxUser && fluxUser.devModeUnlocked
                                onToggled: function(val) {
                                    if (!val) {
                                        if (fluxUser) fluxUser.devModeUnlocked = false
                                        panel.showPinPrompt = false
                                        devPinInput.text = ""
                                        panel.devPinErrorText = ""
                                        panel.customCodeSavedText = ""
                                    } else {
                                        panel.showPinPrompt = true
                                        panel.devPinErrorText = ""
                                        devPinInput.forceActiveFocus()
                                    }
                                }
                            }
                        }

                        // --- 2. PIN Entry Prompt (shown when user flips toggle ON but mode is not yet unlocked) ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: (!fluxUser || !fluxUser.devModeUnlocked) && panel.showPinPrompt
                            spacing: 10

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: Theme.border
                            }

                            Text {
                                text: "Enter 4-digit PIN to enable Developer Mode:"
                                color: Theme.text
                                font.pixelSize: 12
                                font.weight: Font.Medium
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 10

                                Rectangle {
                                    implicitHeight: 40
                                    implicitWidth: 140
                                    radius: 10
                                    color: devPinInput.activeFocus ? "#26FFFFFF" : "#1AFFFFFF"
                                    border.width: 1
                                    border.color: panel.devPinErrorText.length > 0 ? Theme.danger : (devPinInput.activeFocus ? Theme.accentRing : Theme.border)

                                    TextField {
                                        id: devPinInput
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 12
                                        echoMode: TextInput.Password
                                        placeholderText: "\u2022 \u2022 \u2022 \u2022"
                                        placeholderTextColor: Theme.textMute
                                        color: Theme.text
                                        font.pixelSize: 18
                                        font.weight: Font.Bold
                                        font.letterSpacing: 6
                                        horizontalAlignment: TextInput.AlignHCenter
                                        verticalAlignment: TextInput.AlignVCenter
                                        maximumLength: 8
                                        inputMethodHints: Qt.ImhDigitsOnly
                                        selectByMouse: true
                                        background: null
                                        validator: RegularExpressionValidator { regularExpression: /^\d{0,8}$/ }
                                        onAccepted: panel.unlockDevMode()
                                        onTextChanged: panel.devPinErrorText = ""
                                    }
                                }

                                FluxButton {
                                    text: "Confirm"
                                    implicitHeight: 40
                                    enabled: devPinInput.text.trim().length >= 4
                                    onClicked: panel.unlockDevMode()
                                }

                                FluxButton {
                                    text: "Cancel"
                                    variant: "ghost"
                                    implicitHeight: 40
                                    onClicked: {
                                        panel.showPinPrompt = false
                                        devPinInput.text = ""
                                        panel.devPinErrorText = ""
                                    }
                                }

                                Item { Layout.fillWidth: true }
                            }

                            Text {
                                visible: panel.devPinErrorText.length > 0
                                text: panel.devPinErrorText
                                color: Theme.danger
                                font.pixelSize: 12
                            }
                        }

                        // --- 3. Developer Options Content (shown only when unlocked / ON) ---
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: !!fluxUser && fluxUser.devModeUnlocked
                            spacing: 14

                            Rectangle {
                                Layout.fillWidth: true
                                height: 1
                                color: Theme.border
                            }

                            // Personal Teleparty Code setting
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 0
                                    spacing: 2

                                    Text {
                                        text: "Personal Teleparty Code"
                                        color: Theme.text
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: "When hosting, always generate this custom code instead of a random 5-digit number."
                                        color: Theme.textMute
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                    }
                                }

                                ToggleSwitch {
                                    checked: fluxUser ? fluxUser.useCustomTelepartyCode : false
                                    onToggled: function(val) {
                                        if (fluxUser) fluxUser.useCustomTelepartyCode = val
                                    }
                                }
                            }

                            // Custom code input field (shown when toggle is on)
                            ColumnLayout {
                                Layout.fillWidth: true
                                visible: !!fluxUser && fluxUser.useCustomTelepartyCode
                                spacing: 8

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 10

                                    Rectangle {
                                        implicitHeight: 40
                                        Layout.fillWidth: true
                                        Layout.preferredWidth: 0
                                        radius: 10
                                        color: customCodeInput.activeFocus ? "#26FFFFFF" : "#1AFFFFFF"
                                        border.width: 1
                                        border.color: customCodeInput.activeFocus ? Theme.accentRing : Theme.border

                                        TextField {
                                            id: customCodeInput
                                            anchors.fill: parent
                                            anchors.leftMargin: 12
                                            anchors.rightMargin: 12
                                            text: fluxUser ? fluxUser.customTelepartyCode : ""
                                            placeholderText: "e.g. CFAT, SHIFA, 77777"
                                            placeholderTextColor: Theme.textMute
                                            color: Theme.text
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                            font.letterSpacing: 2
                                            verticalAlignment: TextInput.AlignVCenter
                                            maximumLength: 16
                                            selectByMouse: true
                                            background: null
                                            validator: RegularExpressionValidator { regularExpression: /^[a-zA-Z0-9_-]{0,16}$/ }

                                            onEditingFinished: {
                                                if (fluxUser) fluxUser.customTelepartyCode = text.trim().toUpperCase()
                                            }
                                        }
                                    }

                                    FluxButton {
                                        text: "Save"
                                        implicitHeight: 40
                                        enabled: customCodeInput.text.trim().length >= 3
                                        onClicked: {
                                            if (fluxUser) {
                                                fluxUser.customTelepartyCode = customCodeInput.text.trim().toUpperCase()
                                                panel.customCodeSavedText = "Personal code set to \"" + fluxUser.customTelepartyCode + "\""
                                                customCodeSavedTimer.restart()
                                            }
                                        }
                                    }
                                }

                                Text {
                                    visible: panel.customCodeSavedText.length > 0
                                    text: panel.customCodeSavedText
                                    color: "#46D369"
                                    font.pixelSize: 11
                                }

                                Text {
                                    text: "3-16 alphanumeric characters. Friends enter this exact code when joining."
                                    color: Theme.textMute
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}