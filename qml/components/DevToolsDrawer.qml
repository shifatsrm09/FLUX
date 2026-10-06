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
        color: "#080e1b"
        border.color: "#1e293b"
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 16

        // Header
        RowLayout {
            Layout.fillWidth: true

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    text: "Developer Tools"
                    color: "#ffffff"
                    font.pixelSize: 16
                    font.bold: true
                }
                Text {
                    text: "Internal diagnostics & pre-verified test streams"
                    color: "#64748b"
                    font.pixelSize: 11
                }
            }

            Button {
                id: closeBtn
                implicitWidth: 32
                implicitHeight: 32
                background: Rectangle {
                    radius: 16
                    color: closeBtn.hovered ? "#334155" : "#1e293b"
                }
                contentItem: Text {
                    text: "✕"
                    color: "#94a3b8"
                    font.pixelSize: 12
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: "#1e293b"
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                width: parent.width
                spacing: 18

                // -------------------------------------------------------------
                // Section 1: Pre-verified Test Catalog
                // -------------------------------------------------------------
                Text {
                    text: "PRE-VERIFIED TEST MEDIA"
                    color: "#94a3b8"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }

                Repeater {
                    model: root.catalogModel

                    delegate: MediaCard {
                        Layout.fillWidth: true
                        title: model.title
                        year: model.year
                        badge: model.badge
                        description: model.description
                        url: model.url
                        isSelected: (root.player && root.player.url === model.url)
                        onClicked: {
                            root.playTestRequested(model.url, model.title)
                            root.close()
                        }
                    }
                }

                // -------------------------------------------------------------
                // Section 2: Direct URL Test
                // -------------------------------------------------------------
                Text {
                    text: "DIRECT STREAM TEST"
                    color: "#94a3b8"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    TextField {
                        id: directUrlInput
                        Layout.fillWidth: true
                        implicitHeight: 36
                        placeholderText: "http://172.16.50.x/... (MKV/MP4)"
                        font.pixelSize: 11
                        font.family: "Consolas, monospace"
                        color: "#f8fafc"
                        background: Rectangle {
                            color: "#0f172a"
                            border.color: "#1e293b"
                            radius: 4
                        }
                    }

                    Button {
                        id: directPlayBtn
                        implicitWidth: 64
                        implicitHeight: 36
                        background: Rectangle {
                            color: "#2563eb"
                            radius: 4
                        }
                        contentItem: Text {
                            text: "Play"
                            color: "#ffffff"
                            font.pixelSize: 11
                            font.bold: true
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            if (directUrlInput.text.trim().length > 0) {
                                root.playTestRequested(directUrlInput.text.trim(), "Direct URL Stream")
                                root.close()
                            }
                        }
                    }
                }

                // -------------------------------------------------------------
                // Section 3: libVLC Diagnostic Console
                // -------------------------------------------------------------
                Text {
                    text: "LIBVLC DIAGNOSTIC LOGS"
                    color: "#94a3b8"
                    font.pixelSize: 11
                    font.bold: true
                    font.letterSpacing: 1
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 180
                    color: "#050912"
                    border.color: "#1e293b"
                    border.width: 1
                    radius: 6

                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 8
                        clip: true

                        ListView {
                            width: parent.width
                            model: root.logger ? root.logger.recentLogs : []
                            spacing: 3

                            delegate: Text {
                                width: parent.width
                                text: modelData
                                color: {
                                    if (modelData.indexOf("[ERROR]") !== -1) return "#f87171"
                                    if (modelData.indexOf("[WARN ]") !== -1) return "#fbbf24"
                                    if (modelData.indexOf("[VLC  ]") !== -1) return "#38bdf8"
                                    return "#94a3b8"
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
