import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: root

    property var window: null
    property bool isMaximized: window ? window.visibility === Window.Maximized : false
    property bool showNowPlaying: false

    signal homeClicked()
    signal nowPlayingClicked()
    signal devToolsClicked()

    height: 36
    color: "#08090C"

    function toggleMaximize() {
        if (!root.window) return
        if (root.window.visibility === Window.Maximized) {
            root.window.showNormal()
        } else {
            root.window.showMaximized()
        }
    }

    // Draggable / Double-click maximize area covering entire titlebar
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

    // Left Title: Clickable FLUX Home Navigation
    Item {
        id: fluxHomeBtn
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: fluxText.implicitWidth + 8
        implicitHeight: 24
        z: 3

        Text {
            id: fluxText
            anchors.centerIn: parent
            text: "FLUX"
            font.pixelSize: 12
            font.weight: Font.Bold
            font.letterSpacing: 2
            color: fluxMouse.containsMouse ? "#38BDF8" : "#E2E8F0"

            Behavior on color { ColorAnimation { duration: 120 } }
        }

        MouseArea {
            id: fluxMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.homeClicked()
        }

        ToolTip.visible: fluxMouse.containsMouse
        ToolTip.delay: 400
        ToolTip.text: "Return to Home Page"
    }

    // Right Controls
    RowLayout {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0
        z: 2

        // Return to Player if already streaming
        Text {
            visible: root.showNowPlaying
            text: "Now Playing ▶"
            color: nowPlayingMouse.containsMouse ? "#7DD3FC" : "#38BDF8"
            font.pixelSize: 12
            font.weight: Font.Medium
            Layout.rightMargin: 14

            MouseArea {
                id: nowPlayingMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.nowPlayingClicked()
            }
        }

        // Developer Tools Access
        Item {
            implicitWidth: 32
            implicitHeight: 36
            Layout.rightMargin: 6

            Text {
                anchors.centerIn: parent
                text: "···"
                color: devMouse.containsMouse ? "#F5F5F5" : "#64748B"
                font.pixelSize: 16
                font.weight: Font.Bold
            }

            MouseArea {
                id: devMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.devToolsClicked()
            }

            ToolTip.visible: devMouse.containsMouse
            ToolTip.delay: 500
            ToolTip.text: "Developer Tools (Ctrl+D)"
        }

        // Minimize Button
        Rectangle {
            id: minBtn
            Layout.preferredWidth: 46
            Layout.fillHeight: true
            color: minMouse.containsPress ? "#262D3D" : (minMouse.containsMouse ? "#161B26" : "transparent")

            // Minimize Glyph
            Rectangle {
                anchors.centerIn: parent
                width: 10
                height: 1
                color: minMouse.containsMouse ? "#F1F5F9" : "#94A3B8"
            }

            MouseArea {
                id: minMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.ArrowCursor
                onClicked: {
                    if (root.window) root.window.showMinimized()
                }
            }

            ToolTip.visible: minMouse.containsMouse
            ToolTip.delay: 500
            ToolTip.text: "Minimize"
        }

        // Maximize / Restore Button
        Rectangle {
            id: maxBtn
            Layout.preferredWidth: 46
            Layout.fillHeight: true
            color: maxMouse.containsPress ? "#262D3D" : (maxMouse.containsMouse ? "#161B26" : "transparent")

            // Maximize / Restore Glyph
            Item {
                anchors.centerIn: parent
                width: 10
                height: 10

                // Normal state: Single clean square outline
                Rectangle {
                    visible: !root.isMaximized
                    anchors.fill: parent
                    color: "transparent"
                    border.color: maxMouse.containsMouse ? "#F1F5F9" : "#94A3B8"
                    border.width: 1
                }

                // Maximized state: Restore icon (overlapping dual squares)
                Item {
                    visible: root.isMaximized
                    anchors.fill: parent

                    // Back square
                    Rectangle {
                        x: 2
                        y: 0
                        width: 8
                        height: 8
                        color: "transparent"
                        border.color: maxMouse.containsMouse ? "#F1F5F9" : "#94A3B8"
                        border.width: 1
                    }

                    // Front square
                    Rectangle {
                        x: 0
                        y: 2
                        width: 8
                        height: 8
                        color: maxMouse.containsPress ? "#262D3D" : (maxMouse.containsMouse ? "#161B26" : "#08090C")
                        border.color: maxMouse.containsMouse ? "#F1F5F9" : "#94A3B8"
                        border.width: 1
                    }
                }
            }

            MouseArea {
                id: maxMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.ArrowCursor
                onClicked: root.toggleMaximize()
            }

            ToolTip.visible: maxMouse.containsMouse
            ToolTip.delay: 500
            ToolTip.text: root.isMaximized ? "Restore Down" : "Maximize"
        }

        // Close Button
        Rectangle {
            id: closeBtn
            Layout.preferredWidth: 46
            Layout.fillHeight: true
            color: closeMouse.containsPress ? "#B91C1C" : (closeMouse.containsMouse ? "#E81123" : "transparent")

            // Close Glyph ('✕')
            Item {
                anchors.centerIn: parent
                width: 10
                height: 10

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1.2
                    rotation: 45
                    color: closeMouse.containsMouse ? "#FFFFFF" : "#94A3B8"
                    antialiasing: true
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: 12
                    height: 1.2
                    rotation: -45
                    color: closeMouse.containsMouse ? "#FFFFFF" : "#94A3B8"
                    antialiasing: true
                }
            }

            MouseArea {
                id: closeMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.ArrowCursor
                onClicked: {
                    if (root.window) root.window.close()
                }
            }

            ToolTip.visible: closeMouse.containsMouse
            ToolTip.delay: 500
            ToolTip.text: "Close"
        }
    }

    // Hairline Bottom Separator Border
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: "#141721"
    }
}
