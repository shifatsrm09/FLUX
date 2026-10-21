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
    readonly property bool menuOpen: updateMenu.opened || telepartyMenu.opened
    property string section: "home"    // which nav link is highlighted: "home" | "bookmarks" | "library" | ""
    // In a Teleparty session offline playback is unavailable: the Library link is grayed out
    property bool offlineLocked: false

    signal homeClicked()
    signal searchClicked()
    signal bookmarksClicked()
    signal libraryClicked()
    signal nowPlayingClicked()

    // Text link in the nav (Home / Library) with an underline on the current one
    component NavLink: Item {
        id: navLink

        property string label: ""
        property bool current: false
        property bool locked: false          // grayed out and not usable right now
        property string lockedTip: ""
        signal clicked()

        implicitWidth: navText.implicitWidth
        implicitHeight: 32
        opacity: navLink.locked ? 0.4 : 1.0

        Behavior on opacity { NumberAnimation { duration: 160 } }

        Text {
            id: navText
            anchors.centerIn: parent
            text: navLink.label
            color: (!navLink.locked && (navLink.current || navMouse.containsMouse)) ? Theme.text : Theme.textDim
            font.pixelSize: 14
            font.weight: navLink.current ? Font.Bold : Font.DemiBold

            Behavior on color { ColorAnimation { duration: 120 } }
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            width: navText.implicitWidth
            height: 2
            radius: 1
            color: Theme.accent
            opacity: navLink.current ? 1.0 : 0.0

            Behavior on opacity { NumberAnimation { duration: 160 } }
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

    height: 48
    opacity: root.shown ? 1.0 : 0.0
    visible: opacity > 0.0

    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

    function closeMenu() {
        updateMenu.close()
        telepartyMenu.close()
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
        value: root.overlayMode ? 0 : (brandRow.x + brandRow.width + 12)
    }

    Binding {
        target: (typeof fluxSnap !== "undefined" && fluxSnap) ? fluxSnap : null
        property: "rightInteractiveWidth"
        value: rightRow.width + 8
    }
    // ---- Backgrounds ---------------------------------------------------------

    // Soft gradient scrim (transparent nav over hero)
    Rectangle {
        anchors.fill: parent
        opacity: (root.solid || root.overlayMode) ? 0.0 : 1.0

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#E61E1F22" }
            GradientStop { position: 1.0; color: "#001E1F22" }
        }

        Behavior on opacity { NumberAnimation { duration: 240 } }
    }

    // Solid bar with hairline
    Rectangle {
        anchors.fill: parent
        color: "#F21E1F22"
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
        acceptedButtons: Qt.LeftButton
        z: 0

        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                if (Qt.platform.os !== "windows") root.window.startSystemMove()
            }
        }

        onDoubleClicked: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.toggleMaximize()
            }
        }
    }

    // ---- Left: brand + nav -------------------------------------------------------
    RowLayout {
        id: brandRow

        anchors.left: parent.left
        anchors.leftMargin: 32
        anchors.verticalCenter: parent.verticalCenter
        spacing: 30
        z: 3
        opacity: root.overlayMode ? 0.0 : 1.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        // Wordmark
        Item {
            implicitWidth: logoText.implicitWidth
            implicitHeight: 32

            Text {
                id: logoText
                anchors.centerIn: parent
                text: "FLUX"
                color: logoMouse.containsMouse ? Theme.accent : "#FFFFFF"
                font.pixelSize: 26
                font.weight: Font.Black
                font.letterSpacing: 4

                Behavior on color { ColorAnimation { duration: 120 } }
            }

            MouseArea {
                id: logoMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.homeClicked()
            }

            FluxToolTip {
                visible: logoMouse.containsMouse
                delay: 500
                text: "Return to Home Page"
            }
        }

        // Primary navigation
        RowLayout {
            Layout.alignment: Qt.AlignVCenter
            spacing: 24

            NavLink {
                label: "Home"
                current: root.section === "home"
                onClicked: root.homeClicked()
            }

            // Search with icon
            Item {
                id: searchNavItem
                implicitWidth: searchNavRow.implicitWidth + 8
                implicitHeight: 32

                Row {
                    id: searchNavRow
                    anchors.centerIn: parent
                    spacing: 6

                    FluxIcon {
                        name: "search"
                        size: 15
                        color: (root.section === "search" || searchNavMouse.containsMouse) ? Theme.text : Theme.textDim
                        anchors.verticalCenter: parent.verticalCenter
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Search"
                        color: (root.section === "search" || searchNavMouse.containsMouse) ? Theme.text : Theme.textDim
                        font.pixelSize: 14
                        font.weight: root.section === "search" ? Font.Bold : Font.DemiBold
                        Behavior on color { ColorAnimation { duration: 120 } }
                    }
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    width: searchNavRow.implicitWidth
                    height: 2
                    radius: 1
                    color: Theme.accent
                    opacity: root.section === "search" ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 160 } }
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
                    if (updateMenu.opened) updateMenu.close()
                    else updateMenu.open()
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
