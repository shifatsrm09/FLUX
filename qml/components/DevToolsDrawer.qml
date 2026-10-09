import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Drawer {
    id: root

    property var player: null
    property var logger: null
    property var catalogModel: null

    signal playTestRequested(string url, string title)

    edge: Qt.RightEdge
    width: Math.min(480, parent.width * 0.85)
    height: parent.height

    background: Rectangle {
        color: Theme.bgRaised

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Theme.border
        }
    }

    Overlay.modal: Rectangle { color: "#99000000" }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // Header
        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Text {
                    text: "FLUX v0.0.3 \u2022 Developer & Diagnostics"
                    color: Theme.text
                    font.pixelSize: 17
                    font.weight: Font.Bold
                }

                Text {
                    text: "Test catalog and libVLC diagnostic output"
                    color: Theme.textMute
                    font.pixelSize: 12
                }
            }

            IconButton {
                iconName: "close"
                iconSize: 16
                iconColor: Theme.textDim
                implicitWidth: 34
                implicitHeight: 34
                tip: "Close"
                tipAbove: false
                onClicked: root.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Theme.border
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 24

                // -------------------------------------------------------------
                // Section 1: Pre-verified Test Media
                // -------------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "PRE-VERIFIED TEST MEDIA"
                        color: Theme.textMute
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 1.4
                    }

                    Repeater {
                        model: root.catalogModel

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 66
                            radius: 12
                            color: itemMouse.containsMouse ? Theme.surfaceHi : Theme.surface
                            border.width: 1
                            border.color: (root.player && root.player.url === model.url) ? Theme.accent : Theme.border

                            Behavior on color { ColorAnimation { duration: 120 } }

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 3

                                    Text {
                                        Layout.fillWidth: true
                                        text: model.title
                                        color: Theme.text
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: model.year + " \u00B7 " + model.badge
                                        color: Theme.textDim
                                        font.pixelSize: 12
                                    }
                                }

                                Rectangle {
                                    implicitWidth: 34
                                    implicitHeight: 34
                                    radius: 17
                                    color: itemMouse.containsMouse ? Theme.accent : "#1FFFFFFF"

                                    Behavior on color { ColorAnimation { duration: 120 } }

                                    FluxIcon {
                                        anchors.centerIn: parent
                                        anchors.horizontalCenterOffset: 1
                                        name: "play"
                                        size: 16
                                        color: "#FFFFFF"
                                    }
                                }
                            }

                            MouseArea {
                                id: itemMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    root.playTestRequested(model.url, model.title)
                                    root.close()
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // Section 2: Direct URL Test
                // -------------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "DIRECT URL STREAM TEST"
                        color: Theme.textMute
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 1.4
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 42
                            radius: 21
                            color: Theme.surface
                            border.width: directUrlInput.activeFocus ? 2 : 1
                            border.color: directUrlInput.activeFocus ? Theme.accent : Theme.border

                            TextField {
                                id: directUrlInput
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 16
                                placeholderText: "http://172.16.50.x/... (MKV/MP4)"
                                placeholderTextColor: Theme.textMute
                                font.pixelSize: 12
                                font.family: Theme.monoFamily
                                color: Theme.text
                                selectionColor: Theme.accent
                                background: null
                            }
                        }

                        FluxButton {
                            id: directPlayBtn
                            text: "Play"
                            implicitHeight: 42

                            onClicked: {
                                if (directUrlInput.text.trim().length > 0) {
                                    root.playTestRequested(directUrlInput.text.trim(), "Direct Stream")
                                    root.close()
                                }
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // Section 3: libVLC Diagnostic Console
                // -------------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 10

                    Text {
                        text: "LIBVLC DIAGNOSTIC LOGS"
                        color: Theme.textMute
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 1.4
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 220
                        color: Theme.bg
                        border.width: 1
                        border.color: Theme.border
                        radius: 12

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 12
                            clip: true

                            ListView {
                                width: parent.width
                                model: root.logger ? root.logger.recentLogs : []
                                spacing: 3

                                delegate: Text {
                                    width: parent.width
                                    text: modelData
                                    color: {
                                        if (modelData.indexOf("[ERROR]") !== -1) return Theme.danger
                                        if (modelData.indexOf("[WARN ]") !== -1) return Theme.warning
                                        if (modelData.indexOf("[VLC  ]") !== -1) return "#5CB8FF"
                                        return Theme.textDim
                                    }
                                    font.pixelSize: 10
                                    font.family: Theme.monoFamily
                                    wrapMode: Text.WrapAnywhere
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
