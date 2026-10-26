import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "Theme.js" as Theme

// Frameless-window title bar styled as a streaming-service nav bar.
//  - translucent gradient over hero content, solid once the page scrolls
//  - overlayMode (player): brand/nav fade out, only window controls remain
Item {
    id: root

    property var window: null
    property bool isMaximized: window ? window.visibility === Window.Maximized : false
    property bool showNowPlaying: false
    property bool nowPlayingNeedsMedia: false
    property string nowPlayingWarning: ""

    property bool solid: false          // opaque background (scrolled / results page)
    property bool overlayMode: false    // player mode: transparent, window controls only
    property bool shown: true           // fade whole bar in / out
    property int downloadBadge: 0
    property bool downloadAttention: false
    property bool downloadsOpen: false
    property bool settingsOpen: false

    readonly property bool menuOpen: (updateMenu && updateMenu.opened)
                                  || (telepartyMenu && telepartyMenu.opened)
                                  || (fluxMenu && fluxMenu.opened)
                                  || downloadsOpen

    property string section: "home"    // which nav link is highlighted: "home" | "bookmarks" | "library" | ""
    // In a Teleparty session offline playback is unavailable: the Library link is grayed out
    property bool offlineLocked: false

    signal homeClicked()
    signal searchClicked()
    signal bookmarksClicked()
    signal libraryClicked()
    signal nowPlayingClicked()
    signal settingsClicked()
    signal downloadsClicked()
    signal closePopupsRequested()

    // Steam-style text link in the titlebar
    component NavLink: Item {
        id: navLink

        property string label: ""
        property bool current: false
        property bool locked: false          // grayed out and not usable right now
        property string lockedTip: ""
        signal clicked()

        implicitWidth: navText.implicitWidth + 8
        implicitHeight: 32
        opacity: navLink.locked ? 0.4 : 1.0

        Behavior on opacity { NumberAnimation { duration: 160 } }

        Text {
            id: navText
            anchors.centerIn: parent
            text: navLink.label
            color: (!navLink.locked && (navLink.current || navMouse.containsMouse)) ? "#FFFFFF" : "#C5C3C0"
            font.pixelSize: 12
            font.weight: navLink.current ? Font.DemiBold : Font.Normal

            Behavior on color { ColorAnimation { duration: 120 } }
        }

        MouseArea {
            id: navMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: navLink.locked ? Qt.ForbiddenCursor : Qt.PointingHandCursor
            onClicked: {
                if (!navLink.locked) {
                    navLink.clicked()
                }
            }
        }

        FluxToolTip {
            visible: navMouse.containsMouse && navLink.locked && navLink.lockedTip.length > 0
            delay: 300
            text: navLink.lockedTip
        }
    }

    height: 40
    opacity: root.shown ? 1.0 : 0.0
    visible: opacity > 0.0

    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    function closeMenu() {
        if (fluxMenu && fluxMenu.opened) fluxMenu.close()
        if (updateMenu && updateMenu.opened) updateMenu.close()
        if (telepartyMenu && telepartyMenu.opened) telepartyMenu.close()
        root.closePopupsRequested()
    }

    function notifyDownloadsClosed() {
        if (downloadsButton) downloadsButton.lastClosed = Date.now()
    }

    function toggleMaximize() {
        if (!root.window) return
        if (root.window.visibility === Window.Maximized) {
            root.window.showNormal()
        } else {
            root.window.showMaximized()
        }
    }

    Binding {
        target: (typeof fluxSnap !== "undefined" && fluxSnap) ? fluxSnap : null
        property: "leftInteractiveWidth"
        value: root.overlayMode ? 120 : (brandRow.x + (brandRow.width > 0 ? brandRow.width : brandRow.implicitWidth) + 12)
    }

    Binding {
        target: (typeof fluxSnap !== "undefined" && fluxSnap) ? fluxSnap : null
        property: "rightInteractiveWidth"
        value: (rightRow.width > 0 ? rightRow.width : rightRow.implicitWidth) + 8
    }

    Binding {
        target: (typeof fluxSnap !== "undefined" && fluxSnap) ? fluxSnap : null
        property: "menuOpen"
        value: root.menuOpen
    }

    Connections {
        target: (typeof fluxSnap !== "undefined" && fluxSnap) ? fluxSnap : null
        function onRequestCloseMenus() {
            root.closeMenu()
        }
    }
    // ---- Backgrounds ---------------------------------------------------------

    // Soft gradient scrim (transparent nav over hero)
    Rectangle {
        anchors.fill: parent
        opacity: (root.solid || root.overlayMode) ? 0.0 : 1.0

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#E6171d25" }
            GradientStop { position: 1.0; color: "#00171d25" }
        }

        Behavior on opacity { NumberAnimation { duration: 240 } }
    }

    // Solid bar with hairline
    Rectangle {
        anchors.fill: parent
        color: "#F2171d25"
        opacity: (root.solid && !root.overlayMode) ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 240 } }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 1
            color: Theme.border
        }
    }

    // ---- Drag / double-click maximize area -------------------------------------
    MouseArea {
        anchors.fill: parent
        anchors.leftMargin: root.overlayMode ? 120 : 0
        acceptedButtons: Qt.LeftButton
        z: 0

        onPressed: function(mouse) {
            if (root.menuOpen) {
                root.closeMenu()
                return
            }
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemMove()
            }
        }

        onDoubleClicked: function(mouse) {
            if (root.menuOpen) {
                root.closeMenu()
                return
            }
            if (mouse.button === Qt.LeftButton && root.window) {
                root.toggleMaximize()
            }
        }
    }

    // ---- Left: brand + nav -------------------------------------------------------
    RowLayout {
        id: brandRow
        width: implicitWidth

        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16
        z: 3
        opacity: root.overlayMode ? 0.0 : 1.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        // Steam-style Brand Menu button: Logo + "FLUX"
        Item {
            id: fluxBtn
            property double lastClosed: 0
            implicitWidth: fluxBtnRow.implicitWidth + 6
            implicitHeight: 32

            RowLayout {
                id: fluxBtnRow
                anchors.centerIn: parent
                spacing: 6

                Image {
                    source: "qrc:/FLUX/resources/flux.png"
                    width: logoText.font.pixelSize
                    height: logoText.font.pixelSize
                    sourceSize: Qt.size(24, 24)
                    fillMode: Image.PreserveAspectFit
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    id: logoText
                    text: "FLUX"
                    color: (fluxMenu.opened || fluxMouse.containsMouse) ? "#FFFFFF" : "#C5C3C0"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter

                    Behavior on color { ColorAnimation { duration: 120 } }
                }
            }

            MouseArea {
                id: fluxMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (Date.now() - fluxBtn.lastClosed < 250) return
                    if (fluxMenu.opened) fluxMenu.close()
                    else fluxMenu.open()
                }
            }

            // Steam-style FLUX menu popup
            Popup {
                id: fluxMenu
                parent: fluxBtn
                x: 0
                y: fluxBtn.height + 6
                width: 220
                padding: 4
                modal: false
                focus: true
                closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
                onClosed: fluxBtn.lastClosed = Date.now()

                enter: Transition {
                    NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100 }
                    NumberAnimation { property: "scale"; from: 0.98; to: 1.0; duration: 100; easing.type: Easing.OutCubic }
                }
                exit: Transition {
                    NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 80 }
                }

                background: Rectangle {
                    color: "#171a21"
                    radius: 4
                    border.width: 1
                    border.color: "#384152"
                }

                contentItem: ColumnLayout {
                    spacing: 2

                    // Option 1: Check for Client Updates...
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        radius: 3
                        color: optUpdateMouse.containsMouse ? "#282e3a" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12

                            Text {
                                text: "Check for Client Updates..."
                                color: optUpdateMouse.containsMouse ? "#FFFFFF" : "#C5C3C0"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: optUpdateMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                fluxMenu.close()
                                if (fluxUpdater) fluxUpdater.checkForUpdates(false)
                                updateMenu.parent = fluxBtn
                                updateMenu.x = 0
                                updateMenu.y = fluxBtn.height + 4
                                updateMenu.open()
                            }
                        }
                    }

                    // Option 2: Settings
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        radius: 3
                        color: optSettingsMouse.containsMouse ? "#282e3a" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12

                            Text {
                                text: "Settings"
                                color: optSettingsMouse.containsMouse ? "#FFFFFF" : "#C5C3C0"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: optSettingsMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                fluxMenu.close()
                                root.settingsClicked()
                            }
                        }
                    }

                    // Separator
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        Layout.topMargin: 2
                        Layout.bottomMargin: 2
                        color: "#2e3440"
                    }

                    // Option 3: Exit
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 28
                        radius: 3
                        color: optExitMouse.containsMouse ? "#282e3a" : "transparent"

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12

                            Text {
                                text: "Exit"
                                color: optExitMouse.containsMouse ? "#FFFFFF" : "#C5C3C0"
                                font.pixelSize: 12
                                Layout.fillWidth: true
                            }
                        }

                        MouseArea {
                            id: optExitMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                fluxMenu.close()
                                if (typeof fluxSystem !== "undefined" && fluxSystem) {
                                    fluxSystem.exitApp()
                                }
                                Qt.quit()
                            }
                        }
                    }
                }
            }
        }

        // Primary navigation items (existing buttons from FLUX)
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 16

            NavLink {
                label: "Home"
                current: root.section === "home"
                onClicked: root.homeClicked()
            }

            // Search with icon + text
            Item {
                id: searchNavItem
                implicitWidth: searchNavRow.implicitWidth + 8
                implicitHeight: 32

                RowLayout {
                    id: searchNavRow
                    anchors.centerIn: parent
                    spacing: 5

                    FluxIcon {
                        name: "search"
                        size: 13
                        color: (root.section === "search" || searchNavMouse.containsMouse) ? "#FFFFFF" : "#C5C3C0"
                        Layout.alignment: Qt.AlignVCenter
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    Text {
                        Layout.alignment: Qt.AlignVCenter
                        text: "Search"
                        color: (root.section === "search" || searchNavMouse.containsMouse) ? "#FFFFFF" : "#C5C3C0"
                        font.pixelSize: 12
                        font.weight: root.section === "search" ? Font.DemiBold : Font.Normal
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }

                MouseArea {
                    id: searchNavMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.searchClicked()
                }

                FluxToolTip {
                    visible: searchNavMouse.containsMouse
                    delay: 500
                    text: "Search & Categories"
                }
            }

            NavLink {
                label: "Bookmarks"
                current: root.section === "bookmarks"
                onClicked: root.bookmarksClicked()
            }

            NavLink {
                label: "Library"
                current: root.section === "library"
                locked: root.offlineLocked
                lockedTip: "Leave Teleparty to access library"
                onClicked: root.libraryClicked()
            }
        }
    }

    // ---- Right: now playing, dev tools, window controls ----
    RowLayout {
        id: rightRow
        width: implicitWidth
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0
        z: 2

        // Return to the player while something is streaming
        Rectangle {
            id: nowPlayingPill

            visible: root.showNowPlaying
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 8
            implicitHeight: 24
            implicitWidth: nowPlayingRow.implicitWidth + 18
            radius: 12
            color: (window && window.partyServerMismatch)
                   ? (nowPlayingMouse.containsMouse ? "#70D97706" : "#55B45309")
                   : (root.nowPlayingNeedsMedia
                      ? (nowPlayingMouse.containsMouse ? "#70A9212B" : "#55A9212B")
                      : (nowPlayingMouse.containsMouse ? "#405865F2" : Theme.accentSoft))
            border.width: 1
            border.color: (window && window.partyServerMismatch)
                          ? "#FFF59E0B"
                          : (root.nowPlayingNeedsMedia ? "#FFFF6573" : Theme.accentRing)

            Behavior on color { ColorAnimation { duration: 120 } }

            RowLayout {
                id: nowPlayingRow
                anchors.centerIn: parent
                spacing: 6

                FluxIcon {
                    visible: !!(window && window.partyServerMismatch)
                    name: "alert"
                    size: 13
                    color: "#FBBF24"
                    Layout.alignment: Qt.AlignVCenter
                }

                Equalizer {
                    visible: !(window && window.partyServerMismatch)
                    running: root.showNowPlaying && !root.nowPlayingNeedsMedia && !(window && window.partyServerMismatch) && root.visible
                             && (window && window.inTeleparty && fluxSync ? fluxSync.partyPlaying : (!!fluxPlayer && fluxPlayer.isPlaying))
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: {
                        if (window && window.partyServerMismatch) {
                            var sName = (window.partyMismatchServerName || "").toLowerCase()
                            var sTag = "ICC/DHAKA FLIX"
                            if (sName.indexOf("icc") !== -1) sTag = "ICC"
                            else if (sName.indexOf("dhaka") !== -1) sTag = "DHAKA FLIX"
                            return "Now Playing: " + sTag + " Content Unsupported on Your Network"
                        }
                        return root.nowPlayingNeedsMedia ? "Add local file" : "Now Playing"
                    }
                    color: (window && window.partyServerMismatch)
                           ? "#FFFDE68A"
                           : (root.nowPlayingNeedsMedia ? "#FFFF9AA2" : Theme.text)
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            MouseArea {
                id: nowPlayingMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: (window && window.partyServerMismatch) ? Qt.ArrowCursor : Qt.PointingHandCursor
                onClicked: {
                    if (!(window && window.partyServerMismatch)) {
                        root.nowPlayingClicked()
                    }
                }
            }

            FluxToolTip {
                visible: (root.nowPlayingNeedsMedia || (window && window.partyServerMismatch)) && nowPlayingMouse.containsMouse
                delay: 200
                text: root.nowPlayingWarning
            }
        }


        // Teleparty: host / join a watch party. Reads "Teleparty: Joined" while in one.
        Rectangle {
            id: telepartyButton

            property double lastClosed: 0   // guards against the press that closes the menu re-opening it
            readonly property string st: fluxTeleparty ? fluxTeleparty.state : "idle"

            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 8
            implicitHeight: 24
            implicitWidth: telepartyRow.implicitWidth + 18
            radius: 12
            color: {
                if (telepartyButton.st === "joined") return telepartyMouse.containsMouse ? "#4046D369" : "#2646D369"
                if (telepartyButton.st !== "idle") return telepartyMouse.containsMouse ? "#40F5B83D" : "#26F5B83D"
                return (telepartyMouse.containsMouse || telepartyMenu.opened) ? "#33FFFFFF" : "#1FFFFFFF"
            }
            border.width: 1
            border.color: {
                if (telepartyButton.st === "joined") return "#6646D369"
                if (telepartyButton.st !== "idle") return "#66F5B83D"
                return Theme.border
            }

            Behavior on color { ColorAnimation { duration: 120 } }

            RowLayout {
                id: telepartyRow
                anchors.centerIn: parent
                spacing: 6

                Rectangle {
                    visible: telepartyButton.st !== "idle"
                    implicitWidth: 6
                    implicitHeight: 6
                    radius: 3
                    color: telepartyButton.st === "joined" ? Theme.success : Theme.warning
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: fluxTeleparty ? fluxTeleparty.statusText : "Teleparty"
                    color: Theme.text
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignVCenter
                }
            }

            MouseArea {
                id: telepartyMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (Date.now() - telepartyButton.lastClosed < 250) return
                    if (telepartyMenu.opened) telepartyMenu.close()
                    else telepartyMenu.open()
                }
            }

            FluxToolTip {
                visible: telepartyMouse.containsMouse && !telepartyMenu.opened
                delay: 500
                text: telepartyButton.st === "joined" ? "Teleparty session options" : "Watch together"
            }

            TelepartyMenu {
                id: telepartyMenu

                property real targetX: telepartyButton.width - width
                x: opened ? x : targetX
                y: telepartyButton.height + 10

                onAboutToShow: {
                    x = telepartyButton.width - width
                }

                onClosed: telepartyButton.lastClosed = Date.now()
            }
        }

        // Download icon button
        Rectangle {
            id: downloadsButton

            property double lastClosed: 0
            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 8
            implicitWidth: 28
            implicitHeight: 28
            radius: 4
            color: (dlMouse.containsPress || root.downloadsOpen) ? "#33FFFFFF"
                   : (dlMouse.containsMouse ? "#1FFFFFFF" : "transparent")

            Behavior on color { ColorAnimation { duration: 120 } }

            FluxIcon {
                anchors.centerIn: parent
                name: "download"
                size: 18
                strokeWidth: 2.0
                color: (dlMouse.containsMouse || root.downloadsOpen) ? "#FFFFFF" : "#C5C3C0"
                Behavior on color { ColorAnimation { duration: 120 } }
            }

            Rectangle {
                visible: root.downloadBadge > 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: -3
                anchors.topMargin: -3
                implicitWidth: Math.max(14, dlBadgeText.implicitWidth + 6)
                implicitHeight: 14
                radius: 7
                color: Theme.accent
                border.width: 1
                border.color: "#171d25"

                Text {
                    id: dlBadgeText
                    anchors.centerIn: parent
                    text: root.downloadBadge
                    color: "#FFFFFF"
                    font.pixelSize: 9
                    font.weight: Font.Bold
                }
            }

            Rectangle {
                visible: root.downloadAttention && root.downloadBadge === 0
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 1
                anchors.topMargin: 1
                width: 7
                height: 7
                radius: 3.5
                color: "#FF4D5E"
                border.width: 1
                border.color: "#171d25"
            }

            MouseArea {
                id: dlMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (Date.now() - downloadsButton.lastClosed < 250) return
                    root.downloadsClicked()
                }
            }

            FluxToolTip {
                visible: dlMouse.containsMouse && !root.downloadsOpen
                delay: 500
                text: "Downloads"
            }
        }

        // More (⋯): version + check for updates
        Rectangle {
            id: moreButton

            property double lastClosed: 0   // guards against the press that closes the menu re-opening it

            Layout.preferredWidth: 46
            Layout.fillHeight: true
            color: (moreMouse.containsPress || updateMenu.opened) ? "#33FFFFFF"
                                                                   : (moreMouse.containsMouse ? "#1FFFFFFF" : "transparent")

            Row {
                anchors.centerIn: parent
                spacing: 3

                Repeater {
                    model: 3

                    Rectangle {
                        width: 3
                        height: 3
                        radius: 1.5
                        color: (moreMouse.containsMouse || updateMenu.opened) ? "#FFFFFF" : "#B4B4BF"
                    }
                }
            }

            // "Update available" dot
            Rectangle {
                visible: !!fluxUpdater && fluxUpdater.updateAvailable
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.rightMargin: 10
                anchors.topMargin: 12
                width: 9
                height: 9
                radius: 4.5
                color: Theme.accent
                border.width: 1
                border.color: "#0A0A0D"
            }

            MouseArea {
                id: moreMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (Date.now() - moreButton.lastClosed < 250) return
                    if (updateMenu.opened) {
                        updateMenu.close()
                    } else {
                        updateMenu.parent = moreButton
                        updateMenu.x = moreButton.width - updateMenu.width
                        updateMenu.y = moreButton.height + 4
                        updateMenu.open()
                    }
                }
            }

            FluxToolTip {
                visible: moreMouse.containsMouse && !updateMenu.opened
                delay: 500
                text: (!!fluxUpdater && fluxUpdater.updateAvailable) ? "Update available" : "More"
            }

            UpdateMenu {
                id: updateMenu

                x: moreButton.width - width
                y: moreButton.height + 4

                onClosed: moreButton.lastClosed = Date.now()
            }
        }

        // Minimize
        Rectangle {
            Layout.preferredWidth: 46
            Layout.fillHeight: true
            color: minMouse.containsPress ? "#33FFFFFF" : (minMouse.containsMouse ? "#1FFFFFFF" : "transparent")

            Rectangle {
                anchors.centerIn: parent
                width: 10
                height: 1
                color: minMouse.containsMouse ? "#FFFFFF" : "#B4B4BF"
            }

            MouseArea {
                id: minMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (root.window) root.window.showMinimized()
                }
            }
        }

        // Maximize / Restore
        Rectangle {
            Layout.preferredWidth: 46
            Layout.fillHeight: true
            color: maxMouse.containsPress ? "#33FFFFFF" : (maxMouse.containsMouse ? "#1FFFFFFF" : "transparent")

            Item {
                anchors.centerIn: parent
                width: 10
                height: 10

                // Normal state: single square outline
                Rectangle {
                    visible: !root.isMaximized
                    anchors.fill: parent
                    color: "transparent"
                    border.width: 1
                    border.color: maxMouse.containsMouse ? "#FFFFFF" : "#B4B4BF"
                }

                // Maximized state: two overlapping squares (back square drawn as an L)
                Item {
                    visible: root.isMaximized
                    anchors.fill: parent

                    Rectangle {
                        x: 2; y: 0; width: 8; height: 1
                        color: maxMouse.containsMouse ? "#FFFFFF" : "#B4B4BF"
                    }

                    Rectangle {
                        x: 9; y: 0; width: 1; height: 8
                        color: maxMouse.containsMouse ? "#FFFFFF" : "#B4B4BF"
                    }

                    Rectangle {
                        x: 0; y: 2; width: 8; height: 8
                        color: "transparent"
                        border.width: 1
                        border.color: maxMouse.containsMouse ? "#FFFFFF" : "#B4B4BF"
                    }
                }
            }

            MouseArea {
                id: maxMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.toggleMaximize()
            }
        }

        // Close
        Rectangle {
            Layout.preferredWidth: 46
            Layout.fillHeight: true
            color: closeMouse.containsPress ? "#B91C1C" : (closeMouse.containsMouse ? "#E81123" : "transparent")

            Item {
                anchors.centerIn: parent
                width: 10
                height: 10

                Rectangle {
                    anchors.centerIn: parent
                    width: 13
                    height: 1.2
                    rotation: 45
                    antialiasing: true
                    color: closeMouse.containsMouse ? "#FFFFFF" : "#B4B4BF"
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 13
                    height: 1.2
                    rotation: -45
                    antialiasing: true
                    color: closeMouse.containsMouse ? "#FFFFFF" : "#B4B4BF"
                }
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    if (root.window) root.window.close()
                }
            }
        }
    }
}
