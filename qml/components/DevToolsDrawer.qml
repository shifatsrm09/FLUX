import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Drawer {
    id: root

    property var player: null
    property var logger: null
    property var catalogModel: null

    signal playTestRequested(string url, string title)

    edge: Qt.RightEdge
    width: Math.min(460, parent.width * 0.85)
    height: parent.height

    background: Rectangle {
        color: "#0B0D12"
        border.color: "#181C26"
        border.width: 1
    }

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
                    text: "FLUX v0.0.2 • Developer & Diagnostics"
                    color: "#F5F5F5"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "Test catalog and libVLC diagnostic output"
                    color: "#5E6676"
                    font.pixelSize: 11
                }
            }

            Button {
                id: closeBtn
                implicitWidth: 28
                implicitHeight: 28
                background: Rectangle {
                    radius: 14
                    color: closeBtn.hovered ? "#1C202B" : "transparent"
                }
                contentItem: Text {
                    text: "✕"
                    color: "#8F96A3"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#181C26"
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 20

                // -------------------------------------------------------------
                // Section 1: Pre-verified Test Media
                // -------------------------------------------------------------
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Text {
                        text: "PRE-VERIFIED TEST MEDIA"
                        color: "#5E6676"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 1.2
                    }

                    Repeater {
                        model: root.catalogModel

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 64
                            radius: 6
                            color: itemMouse.containsMouse ? "#141720" : "#0F1117"
                            border.color: (root.player && root.player.url === model.url) ? "#38BDF8" : "#1A1D26"
                            border.width: 1

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text {
                                        Layout.fillWidth: true
                                        text: model.title
                                        color: "#F5F5F5"
                                        font.pixelSize: 13
                                        font.weight: Font.Medium
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: model.year + " · " + model.badge
                                        color: "#8F96A3"
                                        font.pixelSize: 11
                                    }
                                }

                                Text {
                                    text: "▶"
                                    color: itemMouse.containsMouse ? "#38BDF8" : "#5E6676"
                                    font.pixelSize: 12
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
                    spacing: 8

                    Text {
                        text: "DIRECT URL STREAM TEST"
                        color: "#5E6676"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 1.2
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8

                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 36
                            radius: 4
                            color: "#0E1016"
                            border.color: "#1E222D"
                            border.width: 1

                            TextField {
                                id: directUrlInput
                                anchors.fill: parent
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                placeholderText: "http://172.16.50.x/... (MKV/MP4)"
                                placeholderTextColor: "#444A57"
                                font.pixelSize: 11
                                font.family: "Consolas, monospace"
                                color: "#F5F5F5"
                                background: null
                            }
                        }

                        Button {
                            id: directPlayBtn
                            implicitWidth: 60
                            implicitHeight: 36
                            background: Rectangle {
                                color: directPlayBtn.hovered ? "#0EA5E9" : "#171A21"
                                border.color: "#232734"
                                border.width: 1
                                radius: 4
                            }
                            contentItem: Text {
                                text: "Play"
                                color: "#F5F5F5"
                                font.pixelSize: 11
                                font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
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
                    spacing: 8

                    Text {
                        text: "LIBVLC DIAGNOSTIC LOGS"
                        color: "#5E6676"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 1.2
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 200
                        color: "#08090C"
                        border.color: "#181C26"
                        border.width: 1
                        radius: 4

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 10
                            clip: true

                            ListView {
                                width: parent.width
                                model: root.logger ? root.logger.recentLogs : []
                                spacing: 2

                                delegate: Text {
                                    width: parent.width
                                    text: modelData
                                    color: {
                                        if (modelData.indexOf("[ERROR]") !== -1) return "#F87171"
                                        if (modelData.indexOf("[WARN ]") !== -1) return "#FBBF24"
                                        if (modelData.indexOf("[VLC  ]") !== -1) return "#38BDF8"
                                        return "#7E8696"
                                    }
                                    font.pixelSize: 10
                                    font.family: "Consolas, monospace"
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
