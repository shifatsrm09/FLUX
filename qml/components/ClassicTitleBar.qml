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

    property bool solid: false          // opaque background (scrolled / results page)
    property bool overlayMode: false    // player mode: transparent, window controls only
    property bool shown: true           // fade whole bar in / out
    readonly property bool menuOpen: updateMenu.opened || telepartyMenu.opened
    property string section: "home"    // which nav link is highlighted: "home" | "library" | ""
    // In a Teleparty session offline playback is unavailable: the Library link is grayed out
    property bool offlineLocked: false

    signal homeClicked()
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
                // A locked link still reports the click so the app can explain why
                navLink.clicked()
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

    // ---- Backgrounds ---------------------------------------------------------

    // Soft gradient scrim (transparent nav over hero)
    Rectangle {
        anchors.fill: parent
        opacity: (root.solid || root.overlayMode) ? 0.0 : 1.0

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#E60A0A0D" }
            GradientStop { position: 1.0; color: "#000A0A0D" }
        }

        Behavior on opacity { NumberAnimation { duration: 240 } }
    }

    // Solid bar with hairline
    Rectangle {
        anchors.fill: parent
        color: "#F20A0A0D"
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
                root.window.startSystemMove()
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
                color: logoMouse.containsMouse ? Theme.accentHover : Theme.accent
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

            NavLink {
                label: "Library"
                current: root.section === "library"
                locked: root.offlineLocked
                lockedTip: "Offline playback is unavailable during a Teleparty"
                onClicked: root.libraryClicked()
            }
        }
    }

    // ---- Right: now playing, dev tools, window controls -----------------------------
    RowLayout {
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
            Layout.rightMargin: 12
            implicitHeight: 30
            implicitWidth: nowPlayingRow.implicitWidth + 28
            radius: 15
            color: nowPlayingMouse.containsMouse ? "#40E50914" : Theme.accentSoft
            border.width: 1
            border.color: Theme.accentRing

            Behavior on color { ColorAnimation { duration: 120 } }

            RowLayout {
                id: nowPlayingRow
                anchors.centerIn: parent
                spacing: 8

                Equalizer {
                    running: root.showNowPlaying && root.visible
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: "Now Playing"
                    color: Theme.text
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                }
            }

            MouseArea {
                id: nowPlayingMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.nowPlayingClicked()
            }
        }


        // Teleparty: host / join a watch party. Reads "Teleparty: Joined" while in one.
        Rectangle {
            id: telepartyButton

            property double lastClosed: 0   // guards against the press that closes the menu re-opening it
            readonly property string st: fluxTeleparty ? fluxTeleparty.state : "idle"

            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 12
            implicitHeight: 30
            implicitWidth: telepartyRow.implicitWidth + 28
            radius: 15
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
                spacing: 8

                Rectangle {
                    visible: telepartyButton.st !== "idle"
                    implicitWidth: 7
                    implicitHeight: 7
                    radius: 3.5
                    color: telepartyButton.st === "joined" ? Theme.success : Theme.warning
                    Layout.alignment: Qt.AlignVCenter
                }

                Text {
                    text: fluxTeleparty ? fluxTeleparty.statusText : "Teleparty"
                    color: Theme.text
                    font.pixelSize: 12
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

                x: telepartyButton.width - width
                y: telepartyButton.height + 10

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
