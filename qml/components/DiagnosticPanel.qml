import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var player: null
    property var logger: null
    property bool isExpanded: false

    implicitHeight: isExpanded ? 240 : 42
    color: "#0b1329"
    border.color: "#1e293b"
    border.width: 1
    radius: 6

    Behavior on implicitHeight { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Top Status Bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // State Pill
            Rectangle {
                id: statePill
                implicitWidth: stateText.implicitWidth + 14
                implicitHeight: 24
                radius: 12
                color: {
                    if (!root.player) return "#334155"
                    var st = root.player.state
                    if (st === "Playing") return "#065f46"
                    if (st.indexOf("Buffering") !== -1) return "#854d0e"
                    if (st === "Paused") return "#1e3a8a"
                    if (st === "Error") return "#991b1b"
                    return "#334155"
                }

                Text {
                    id: stateText
                    anchors.centerIn: parent
                    text: root.player ? root.player.state : "Idle"
                    color: "#ffffff"
                    font.pixelSize: 11
                    font.bold: true
                }
            }

            // Resolution info
            Text {
                text: (root.player && root.player.videoWidth > 0)
                      ? "Resolution: " + root.player.videoWidth + " × " + root.player.videoHeight
                      : "Resolution: Waiting for stream..."
                color: "#94a3b8"
                font.pixelSize: 11
                font.family: "Consolas, monospace"
            }

            // URL snippet
            Text {
                Layout.fillWidth: true
                text: (root.player && root.player.url.length > 0) ? "URL: " + root.player.url : "No active stream"
                color: "#64748b"
                font.pixelSize: 11
                font.family: "Consolas, monospace"
                elide: Text.ElideMiddle
            }

            // Expand Logs Button
            Button {
                id: expandBtn
                implicitWidth: 100
                implicitHeight: 26
                background: Rectangle {
                    color: expandBtn.hovered ? "#1e293b" : "transparent"
                    border.color: "#334155"
                    border.width: 1
                    radius: 4
                }
                contentItem: Text {
                    text: root.isExpanded ? "▲ Hide Logs" : "▼ Show Logs"
                    color: "#38bdf8"
                    font.pixelSize: 11
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: root.isExpanded = !root.isExpanded
            }
        }

        // Expanded Diagnostics: Error Banner + Live Log View
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.isExpanded
            spacing: 6

            // Error Message (if any)
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: errorText.implicitHeight + 12
                color: "#450a0a"
                border.color: "#ef4444"
                border.width: 1
                radius: 4
                visible: root.player && root.player.errorMessage.length > 0

                Text {
                    id: errorText
                    anchors.fill: parent
                    anchors.margins: 6
                    text: "⚠️ Playback Error: " + (root.player ? root.player.errorMessage : "")
                    color: "#fecaca"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }

            // Log Console
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                ListView {
                    id: logList
                    model: root.logger ? root.logger.recentLogs : []
                    spacing: 2

                    delegate: Text {
                        width: ListView.view.width
                        text: modelData
                        color: {
                            if (modelData.indexOf("[ERROR]") !== -1) return "#f87171"
                            if (modelData.indexOf("[WARN ]") !== -1) return "#fbbf24"
                            if (modelData.indexOf("[VLC  ]") !== -1) return "#38bdf8"
                            return "#94a3b8"
                        }
                        font.pixelSize: 11
                        font.family: "Consolas, monospace"
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }
}
