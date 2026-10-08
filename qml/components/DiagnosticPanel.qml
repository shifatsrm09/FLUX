import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Rectangle {
    id: root

    property var player: null
    property var logger: null
    property bool isExpanded: false

    implicitHeight: isExpanded ? 240 : 46
    color: Theme.surface
    border.width: 1
    border.color: Theme.border
    radius: 12

    Behavior on implicitHeight { NumberAnimation { duration: 200; easing.type: Easing.OutQuad } }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        spacing: 6

        // Top status bar
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            // State pill
            Rectangle {
                id: statePill
                implicitWidth: stateText.implicitWidth + 20
                implicitHeight: 26
                radius: 13
                color: {
                    if (!root.player) return Theme.surfaceTop
                    var st = root.player.state
                    if (st === "Playing") return "#1F5C33"
                    if (st.indexOf("Buffering") !== -1) return "#6B4A12"
                    if (st === "Paused") return "#1F3A6E"
                    if (st === "Error") return "#7A1F1F"
                    return Theme.surfaceTop
                }

                Text {
                    id: stateText
                    anchors.centerIn: parent
                    text: root.player ? root.player.state : "Idle"
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }

            Text {
                text: (root.player && root.player.videoWidth > 0)
                      ? "Resolution: " + root.player.videoWidth + " \u00D7 " + root.player.videoHeight
                      : "Resolution: Waiting for stream..."
                color: Theme.textDim
                font.pixelSize: 11
                font.family: Theme.monoFamily
            }

            Text {
                Layout.fillWidth: true
                text: (root.player && root.player.url.length > 0) ? "URL: " + root.player.url : "No active stream"
                color: Theme.textMute
                font.pixelSize: 11
                font.family: Theme.monoFamily
                elide: Text.ElideMiddle
            }

            FluxButton {
                text: root.isExpanded ? "Hide Logs" : "Show Logs"
                variant: "secondary"
                implicitHeight: 30
                onClicked: root.isExpanded = !root.isExpanded
            }
        }

        // Expanded diagnostics: error banner + live log view
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: root.isExpanded
            spacing: 6

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: errorText.implicitHeight + 16
                color: "#26FF5D5D"
                border.width: 1
                border.color: "#66FF5D5D"
                radius: 8
                visible: root.player && root.player.errorMessage.length > 0

                Text {
                    id: errorText
                    anchors.fill: parent
                    anchors.margins: 8
                    text: "Playback Error: " + (root.player ? root.player.errorMessage : "")
                    color: "#FFC9C9"
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }
            }

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
                            if (modelData.indexOf("[ERROR]") !== -1) return Theme.danger
                            if (modelData.indexOf("[WARN ]") !== -1) return Theme.warning
                            if (modelData.indexOf("[VLC  ]") !== -1) return "#5CB8FF"
                            return Theme.textDim
                        }
                        font.pixelSize: 11
                        font.family: Theme.monoFamily
                        wrapMode: Text.WrapAnywhere
                    }
                }
            }
        }
    }
}
