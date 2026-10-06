import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var player: null
    property bool isUserInteracting: (seekSlider.pressed || volumeSlider.pressed || audioTrackCombo.popup.visible || subTrackCombo.popup.visible)
    signal toggleFullscreenRequested()

    implicitHeight: 60
    color: "#D90B0E14"
    radius: 8
    border.color: "#1A1E29"
    border.width: 1

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        spacing: 16

        // Play / Pause Button
        Button {
            id: playBtn
            implicitWidth: 36
            implicitHeight: 36
            background: Rectangle {
                radius: 18
                color: playBtn.down ? "#0284C7" : (playBtn.hovered ? "#0EA5E9" : "#1A202C")
            }
            contentItem: Text {
                text: root.player && root.player.isPlaying ? "❙❙" : "▶"
                color: "#FFFFFF"
                font.pixelSize: 13
                font.bold: true
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: {
                if (root.player) root.player.togglePlay()
            }
        }

        // Current Playback Time
        Text {
            id: currentTimeLabel
            text: root.player ? root.player.formattedTime : "00:00"
            color: "#E2E8F0"
            font.pixelSize: 12
            font.family: "Consolas, Segoe UI, monospace"
        }

        // Seek Bar (Timeline Slider)
        Slider {
            id: seekSlider
            Layout.fillWidth: true
            from: 0.0
            to: 1.0
            value: root.player ? root.player.position : 0.0

            background: Rectangle {
                x: seekSlider.leftPadding
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                implicitWidth: 200
                implicitHeight: 4
                width: seekSlider.availableWidth
                height: implicitHeight
                radius: 2
                color: "#1E222D"

                Rectangle {
                    width: seekSlider.visualPosition * parent.width
                    height: parent.height
                    color: "#38BDF8"
                    radius: 2
                }
            }

            handle: Rectangle {
                x: seekSlider.leftPadding + seekSlider.visualPosition * (seekSlider.availableWidth - width)
                y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                implicitWidth: 12
                implicitHeight: 12
                radius: 6
                color: seekSlider.pressed ? "#38BDF8" : "#FFFFFF"
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
            color: "#8F96A3"
            font.pixelSize: 12
            font.family: "Consolas, Segoe UI, monospace"
        }

        // Volume Mute Button
        Button {
            id: muteBtn
            implicitWidth: 32
            implicitHeight: 32
            background: Rectangle { color: "transparent" }
            contentItem: Text {
                text: root.player && root.player.muted ? "VOL ✕" : "VOL"
                color: muteBtn.hovered ? "#38BDF8" : "#8F96A3"
                font.pixelSize: 11
                font.weight: Font.Medium
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
            implicitWidth: 70
            from: 0
            to: 100
            value: root.player ? root.player.volume : 100

            background: Rectangle {
                x: volumeSlider.leftPadding
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                implicitWidth: 70
                implicitHeight: 3
                width: volumeSlider.availableWidth
                height: implicitHeight
                radius: 1.5
                color: "#1E222D"

                Rectangle {
                    width: volumeSlider.visualPosition * parent.width
                    height: parent.height
                    color: "#8F96A3"
                    radius: 1.5
                }
            }

            handle: Rectangle {
                x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
                y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                implicitWidth: 10
                implicitHeight: 10
                radius: 5
                color: "#FFFFFF"
            }

            onMoved: {
                if (root.player) root.player.setVolume(Math.round(value))
            }
        }

        // Audio Track Selector
        ComboBox {
            id: audioTrackCombo
            implicitWidth: 100
            implicitHeight: 30
            model: root.player ? root.player.audioTracks : []
            textRole: "name"
            valueRole: "id"

            background: Rectangle {
                color: "#101218"
                radius: 4
                border.color: "#1E222D"
            }

            contentItem: Text {
                leftPadding: 8
                rightPadding: 8
                text: audioTrackCombo.currentText.length > 0 ? audioTrackCombo.currentText : "Audio"
                color: "#C7CBD4"
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
            implicitWidth: 100
            implicitHeight: 30
            model: root.player ? root.player.subtitleTracks : []
            textRole: "name"
            valueRole: "id"

            background: Rectangle {
                color: "#101218"
                radius: 4
                border.color: "#1E222D"
            }

            contentItem: Text {
                leftPadding: 8
                rightPadding: 8
                text: subTrackCombo.currentText.length > 0 ? subTrackCombo.currentText : "Subtitles"
                color: "#C7CBD4"
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

        // Fullscreen Toggle Button
        Button {
            id: fsBtn
            implicitWidth: 32
            implicitHeight: 32
            background: Rectangle {
                color: fsBtn.hovered ? "#1E222D" : "transparent"
                radius: 4
            }
            contentItem: Text {
                text: "⛶"
                color: fsBtn.hovered ? "#38BDF8" : "#8F96A3"
                font.pixelSize: 14
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            onClicked: root.toggleFullscreenRequested()
        }
    }
}
