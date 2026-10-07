import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Rectangle {
    id: root

    property var window: null
    property string subtitle: ""
    property bool isMaximized: window ? window.visibility === Window.Maximized : false

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

    // Left Branding & Context Title
    RowLayout {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 9
        z: 1

        // Sleek FLUX Logo Mark
        Rectangle {
            width: 18
            height: 18
            radius: 4
            color: "#0F172A"
            border.color: "#38BDF8"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "F"
                color: "#38BDF8"
                font.pixelSize: 11
                font.weight: Font.Black
            }
        }

        // App Title
        Text {
            text: "FLUX"
            font.pixelSize: 12
            font.weight: Font.Bold
            font.letterSpacing: 2
            color: "#E2E8F0"
        }

        // Separator
        Text {
            text: "•"
            font.pixelSize: 10
            color: "#334155"
        }

        // Dynamic Subtitle / Playing Item
        Text {
            text: root.subtitle !== "" ? root.subtitle : "Media Player"
            font.pixelSize: 11
            color: root.subtitle !== "" ? "#94A3B8" : "#64748B"
            font.weight: root.subtitle !== "" ? Font.Medium : Font.Normal
            elide: Text.ElideRight
            Layout.maximumWidth: Math.max(120, root.width - 320)
        }
    }

    // Right Window Control Buttons (Minimize, Maximize / Restore, Close)
    RowLayout {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        spacing: 0
        z: 2

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
