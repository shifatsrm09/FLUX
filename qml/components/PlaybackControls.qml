import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var player: null
    property bool isUserInteracting: (seekSlider.pressed || volumeSlider.pressed || audioTrackCombo.popup.visible || subTrackCombo.popup.visible)
    signal toggleFullscreenRequested()

    implicitHeight: 64
    color: "#e60a1224"
    radius: 10
    border.color: "#1e2c48"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 12

        // Play / Pause Button
        Button {
            id: playBtn
            implicitWidth: 38
            implicitHeight: 38
            background: Rectangle {
                radius: 19
                color: playBtn.down ? "#1d4ed8" : (playBtn.hovered ? "#2563eb" : "#3b82f6")
            }
            contentItem: Text {
                text: root.player && root.player.isPlaying ? "❚❚" : "▶"
                color: "#ffffff"
                font.pixelSize: 15
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                if (root.player) root.player.togglePlay()
            }
        }

        // Stop Button
        Button {
            id: stopBtn
            implicitWidth: 34
            implicitHeight: 34
            background: Rectangle {
                radius: 17
                color: stopBtn.down ? "#475569" : (stopBtn.hovered ? "#334155" : "#1e293b")
            }
            contentItem: Text {
                text: "■"
                color: "#94a3b8"
                font.pixelSize: 13
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                if (root.player) root.player.stop()
            }
        }

        // Current Playback Time
        Text {
            id: currentTimeLabel
            text: root.player ? root.player.formattedTime : "00:00"
            color: "#e2e8f0"
            font.pixelSize: 12
            font.family: "Consolas, monospace"
            font.bold: true
        }

        // Seek Bar (Timeline Slider)
        Slider {
            id: seekSlider
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            value: root.player ? root.player.position : 0.0

            property bool userSeeking: false

            background: Rectangle {
                x: seekSlider.leftPadding
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 6
                width: seekSlider.availableWidth
                height: implicitHeight
                radius: 3
                color: "#1e293b"

                Rectangle {
                    width: seekSlider.visualPosition * parent.width
                    height: parent.height
                    color: "#3b82f6"
                    radius: 3
                }
            }

            handle: Rectangle {
                x: seekSlider.leftPadding + seekSlider.visualPosition * (seekSlider.availableWidth - width)
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                implicitWidth: 14
                implicitHeight: 14
                radius: 7
                color: seekSlider.pressed ? "#60a5fa" : "#ffffff"
                border.color: "#3b82f6"
                border.width: 2
            }

            onPressedChanged: {
                userSeeking = pressed
                if (!pressed && root.player) {
                    root.player.seek(value)
                }
            }

            onMoved: {
                if (root.player) {
                    root.player.seek(value)
                }
            }
        }

        // Total Duration
        Text {
            id: durationLabel
            text: root.player ? root.player.formattedDuration : "00:00"
            color: "#94a3b8"
            font.pixelSize: 12
            font.family: "Consolas, monospace"
        }

        // Volume Mute Button
        Button {
            id: muteBtn
            implicitWidth: 32
            implicitHeight: 32
            background: Rectangle { color: "transparent" }
            contentItem: Text {
                text: root.player && root.player.muted ? "🔇" : "🔊"
                font.pixelSize: 15
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                if (root.player) root.player.setMuted(!root.player.muted)
            }
        }

        // Volume Slider
        Slider {
            id: volumeSlider
            implicitWidth: 80
            from: 0
            to: 100
            value: root.player ? root.player.volume : 100

            background: Rectangle {
                x: volumeSlider.leftPadding
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                implicitWidth: 80
                implicitHeight: 4
                width: volumeSlider.availableWidth
                height: implicitHeight
                radius: 2
                color: "#334155"

                Rectangle {
                    width: volumeSlider.visualPosition * parent.width
                    height: parent.height
                    color: "#38bdf8"
                    radius: 2
                }
            }

            handle: Rectangle {
                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                implicitWidth: 10
                implicitHeight: 10
                radius: 5
                color: "#ffffff"
            }

            onMoved: {
                if (root.player) root.player.setVolume(Math.round(value))
            }
        }

        // Audio Track Selector
        ComboBox {
            id: audioTrackCombo
            implicitWidth: 120
            implicitHeight: 32
            model: root.player ? root.player.audioTracks : []
            textRole: "name"
            valueRole: "id"

            background: Rectangle {
                color: "#1e293b"
                radius: 4
                border.color: "#334155"
            }

            contentItem: Text {
                leftPadding: 8
                rightPadding: 8
                text: audioTrackCombo.currentText.length > 0 ? audioTrackCombo.currentText : "Audio Track"
                color: "#93c5fd"
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            onActivated: function(index) {
                if (root.player && model && model.length > index) {
                    var trackId = model[index].id
                    root.player.selectAudioTrack(trackId)
                }
            }
        }

        // Subtitle Track Selector
        ComboBox {
            id: subTrackCombo
            implicitWidth: 120
            implicitHeight: 32
            model: root.player ? root.player.subtitleTracks : []
            textRole: "name"
            valueRole: "id"

            background: Rectangle {
                color: "#1e293b"
                radius: 4
                border.color: "#334155"
            }

            contentItem: Text {
                leftPadding: 8
                rightPadding: 8
                text: subTrackCombo.currentText.length > 0 ? subTrackCombo.currentText : "Subtitles"
                color: "#93c5fd"
                font.pixelSize: 11
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            onActivated: function(index) {
                if (root.player && model && model.length > index) {
                    var spuId = model[index].id
                    root.player.selectSubtitleTrack(spuId)
                }
            }
        }

        // Fullscreen Toggle
        Button {
            id: fsBtn
            implicitWidth: 32
            implicitHeight: 32
            background: Rectangle {
                color: fsBtn.hovered ? "#334155" : "#1e293b"
                radius: 4
            }
            contentItem: Text {
                text: "⛶"
                color: "#e2e8f0"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.toggleFullscreenRequested()
        }
    }
}
