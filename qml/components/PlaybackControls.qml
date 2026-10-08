import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Streaming-style transport controls: full-width seek bar on top, button row below.
Item {
    id: root

    property var player: null
    property bool isFullscreen: false

    readonly property bool menuOpen: tracksPopup.visible
    property bool isUserInteracting: (seekSlider.pressed || volumeSlider.pressed || tracksPopup.visible || controlsHover.hovered)

    // Used to stop the "press outside closes popup" + "click toggles popup" double-fire
    property double popupClosedAt: 0

    signal toggleFullscreenRequested()

    function closeMenus() {
        tracksPopup.close()
    }

    implicitHeight: controlsColumn.implicitHeight

    HoverHandler { id: controlsHover }

    // Row in the track panel
    component TrackItem: Rectangle {
        id: trackItem

        property string label: ""
        property bool selected: false
        signal activated()

        implicitHeight: 38
        radius: 8
        color: trackMouse.containsMouse ? "#1FFFFFFF" : "transparent"

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 10

            Item {
                implicitWidth: 16
                implicitHeight: 16

                Text {
                    anchors.centerIn: parent
                    visible: trackItem.selected
                    text: "\u2713"
                    color: Theme.accentHover
                    font.pixelSize: 14
                    font.weight: Font.Bold
                }
            }

            Text {
                Layout.fillWidth: true
                text: trackItem.label
                color: trackItem.selected ? Theme.text : Theme.textDim
                font.pixelSize: 13
                font.weight: trackItem.selected ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }
        }

        MouseArea {
            id: trackMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: trackItem.activated()
        }
    }

    ColumnLayout {
        id: controlsColumn

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 2

        // ---- Timeline -----------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 14

            Text {
                Layout.preferredWidth: 64
                horizontalAlignment: Text.AlignRight
                text: root.player ? root.player.formattedTime : "00:00"
                color: Theme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Slider {
                id: seekSlider

                Layout.fillWidth: true
                Layout.preferredHeight: 28
                from: 0.0
                to: 1.0
                value: root.player ? root.player.position : 0.0
                padding: 0
                focusPolicy: Qt.NoFocus
                hoverEnabled: true

                readonly property bool engaged: seekSlider.hovered || seekSlider.pressed

                HoverHandler { id: seekHover }

                background: Item {
                    x: seekSlider.leftPadding
                    y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                    width: seekSlider.availableWidth
                    height: 20

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: seekSlider.engaged ? 7 : 4
                        radius: height / 2
                        color: "#4DFFFFFF"

                        Behavior on height { NumberAnimation { duration: 120 } }

                        Rectangle {
                            width: seekSlider.visualPosition * parent.width
                            height: parent.height
                            radius: parent.radius
                            color: Theme.accent
                        }
                    }
                }

                handle: Rectangle {
                    x: seekSlider.leftPadding + seekSlider.visualPosition * seekSlider.availableWidth - width / 2
                    y: seekSlider.topPadding + seekSlider.availableHeight / 2 - height / 2
                    width: 17
                    height: 17
                    radius: 8.5
                    color: Theme.accent
                    scale: seekSlider.engaged ? 1.0 : 0.0

                    Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                }

                // Hover time preview
                Rectangle {
                    visible: seekHover.hovered && !!root.player && root.player.durationMs > 0
                    x: Theme.clamp(seekHover.point.position.x - width / 2, 0, seekSlider.width - width)
                    y: -height - 4
                    implicitWidth: previewText.implicitWidth + 18
                    implicitHeight: 26
                    radius: 7
                    color: Theme.surfaceTop
                    border.width: 1
                    border.color: Theme.borderHi

                    Text {
                        id: previewText
                        anchors.centerIn: parent
                        text: root.player
                              ? Theme.formatTime(root.player.durationMs
                                                 * Theme.clamp((seekHover.point.position.x - seekSlider.leftPadding) / Math.max(1, seekSlider.availableWidth), 0, 1))
                              : ""
                        color: Theme.text
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }

                onMoved: {
                    if (root.player) {
                        root.player.seek(value)
                    }
                }
            }

            Text {
                Layout.preferredWidth: 64
                text: root.player ? root.player.formattedDuration : "00:00"
                color: Theme.textDim
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }
        }

        // ---- Buttons ------------------------------------------------------------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 4

            IconButton {
                iconName: (root.player && root.player.isPlaying) ? "pause" : "play"
                iconSize: 30
                tip: (root.player && root.player.isPlaying) ? "Pause (Space)" : "Play (Space)"

                onClicked: {
                    if (root.player) root.player.togglePlay()
                }
            }

            IconButton {
                iconName: "replay10"
                iconSize: 28
                tip: "Back 10 seconds (\u2190)"

                onClicked: {
                    if (root.player) root.player.seekRelative(-10000)
                }
            }

            IconButton {
                iconName: "forward10"
                iconSize: 28
                tip: "Forward 10 seconds (\u2192)"

                onClicked: {
                    if (root.player) root.player.seekRelative(10000)
                }
            }

            // Volume: icon + slider that slides open on hover
            Row {
                id: volumeGroup

                Layout.alignment: Qt.AlignVCenter

                readonly property bool expanded: volumeHover.hovered || volumeSlider.pressed

                HoverHandler { id: volumeHover }

                IconButton {
                    id: muteBtn
                    iconName: {
                        if (!root.player || root.player.muted || root.player.volume === 0) return "volumeMuted"
                        if (root.player.volume < 50) return "volumeLow"
                        return "volume"
                    }
                    iconSize: 28
                    tip: (root.player && root.player.muted) ? "Unmute (M)" : "Mute (M)"

                    onClicked: {
                        if (root.player) root.player.muted = !root.player.muted
                    }
                }

                Item {
                    width: volumeGroup.expanded ? 108 : 0
                    height: muteBtn.height
                    clip: true

                    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                    Slider {
                        id: volumeSlider

                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 96
                        height: 24
                        from: 0
                        to: 100
                        padding: 0
                        focusPolicy: Qt.NoFocus
                        value: root.player ? (root.player.muted ? 0 : root.player.volume) : 100

                        background: Item {
                            x: volumeSlider.leftPadding
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: volumeSlider.availableWidth
                            height: 20

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: parent.width
                                height: 4
                                radius: 2
                                color: "#4DFFFFFF"

                                Rectangle {
                                    width: volumeSlider.visualPosition * parent.width
                                    height: parent.height
                                    radius: 2
                                    color: "#FFFFFF"
                                }
                            }
                        }

                        handle: Rectangle {
                            x: volumeSlider.leftPadding + volumeSlider.visualPosition * volumeSlider.availableWidth - width / 2
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: 13
                            height: 13
                            radius: 6.5
                            color: "#FFFFFF"
                        }

                        onMoved: {
                            if (root.player) {
                                if (root.player.muted && volumeSlider.value > 0) root.player.muted = false
                                root.player.volume = Math.round(volumeSlider.value)
                            }
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }

            // Audio & subtitles
            IconButton {
                id: tracksBtn
                iconName: "subtitles"
                iconSize: 28
                tip: "Audio & Subtitles"

                onClicked: {
                    if (tracksPopup.visible) {
                        tracksPopup.close()
                    } else if (Date.now() - root.popupClosedAt > 250) {
                        tracksPopup.open()
                    }
                }
            }

            IconButton {
                iconName: root.isFullscreen ? "fullscreenExit" : "fullscreen"
                iconSize: 28
                tip: root.isFullscreen ? "Exit fullscreen (F)" : "Fullscreen (F)"

                onClicked: root.toggleFullscreenRequested()
            }
        }
    }

    // ---- Audio & subtitles panel ----------------------------------------------------------
    Popup {
        id: tracksPopup

        parent: tracksBtn
        x: tracksBtn.width - width
        y: -height - 14
        width: 460
        padding: 20
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        onClosed: root.popupClosedAt = Date.now()

        // Re-read the live audio/subtitle selection so the checkmarks are accurate
        onAboutToShow: {
            if (root.player) root.player.refreshTracks()
        }

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 140 }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 100 }
        }

        background: Rectangle {
            radius: 16
            color: "#F2101015"
            border.width: 1
            border.color: "#2EFFFFFF"
        }

        contentItem: RowLayout {
            spacing: 24

            // Audio
            ColumnLayout {
                Layout.preferredWidth: 200
                Layout.alignment: Qt.AlignTop
                spacing: 10

                Text {
                    text: "Audio"
                    color: Theme.text
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }

                Text {
                    visible: audioList.count === 0
                    text: "No audio tracks"
                    color: Theme.textMute
                    font.pixelSize: 12
                }

                ListView {
                    id: audioList

                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(audioList.contentHeight, 232)
                    clip: true
                    spacing: 2
                    interactive: audioList.contentHeight > audioList.height
                    model: root.player ? root.player.audioTracks : []

                    ScrollBar.vertical: FluxScrollBar { }

                    delegate: TrackItem {
                        required property var modelData

                        width: audioList.width
                        label: modelData.name
                        selected: !!root.player && modelData.id === root.player.selectedAudioTrack

                        onActivated: {
                            if (root.player) root.player.selectAudioTrack(modelData.id)
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillHeight: true
                implicitWidth: 1
                color: "#1FFFFFFF"
            }

            // Subtitles
            ColumnLayout {
                Layout.preferredWidth: 200
                Layout.alignment: Qt.AlignTop
                spacing: 10

                Text {
                    text: "Subtitles"
                    color: Theme.text
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }

                Text {
                    visible: subList.count === 0
                    text: "No subtitles available"
                    color: Theme.textMute
                    font.pixelSize: 12
                }

                ListView {
                    id: subList

                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(subList.contentHeight, 232)
                    clip: true
                    spacing: 2
                    interactive: subList.contentHeight > subList.height
                    model: root.player ? root.player.subtitleTracks : []

                    ScrollBar.vertical: FluxScrollBar { }

                    delegate: TrackItem {
                        required property var modelData

                        width: subList.width
                        label: modelData.name
                        selected: !!root.player && modelData.id === root.player.selectedSubtitleTrack

                        onActivated: {
                            if (root.player) root.player.selectSubtitleTrack(modelData.id)
                        }
                    }
                }
            }
        }
    }
}
