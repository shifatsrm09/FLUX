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

    signal homeClicked()
    signal nowPlayingClicked()

    height: 48
    opacity: root.shown ? 1.0 : 0.0
    visible: opacity > 0.0

    Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutCubic } }

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

        // Home link
        Item {
            implicitWidth: homeText.implicitWidth
            implicitHeight: 32

            Text {
                id: homeText
                anchors.centerIn: parent
                text: "Home"
                color: homeMouse.containsMouse ? Theme.text : Theme.textDim
                font.pixelSize: 14
                font.weight: Font.Medium

                Behavior on color { ColorAnimation { duration: 120 } }
            }

            MouseArea {
                id: homeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.homeClicked()
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
