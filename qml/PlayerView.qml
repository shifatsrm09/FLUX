import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Flux.Media 1.0
import "components"
import "components/Theme.js" as Theme

Rectangle {
    id: root

    property var player: null
    property string mediaTitle: ""
    property bool isFullscreen: false
    signal toggleFullscreenRequested()
    signal backRequested()

    color: "#000000"
    clip: true

    property bool showControls: true

    readonly property bool menuOpen: controlsBar.menuOpen
    readonly property bool hasError: (!!root.player && root.player.state === "Error")
    readonly property bool showCenterPlay: (!!root.player && !root.player.isPlaying && !root.player.isBuffering && !root.hasError)

    function closeMenus() {
        controlsBar.closeMenus()
    }

    function wakeControls() {
        root.showControls = true
        hideControlsTimer.restart()
    }

    onVisibleChanged: {
        if (root.visible) root.wakeControls()
    }

    Timer {
        id: hideControlsTimer
        interval: 3500
        running: !!root.player && root.player.isPlaying && !controlsBar.isUserInteracting && !topHover.hovered
        repeat: false
        onTriggered: root.showControls = false
    }

    // Surface any state change (pause, buffering, error) by showing the chrome
    Connections {
        target: root.player

        function onStateChanged() {
            root.wakeControls()
        }
    }

    // Embedded VLC Video Surface
    VLCVideoItem {
        id: videoSurface
        anchors.fill: parent
        player: root.player
    }

    // Mouse tracking: wake controls, double click toggles fullscreen
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.showControls ? Qt.ArrowCursor : Qt.BlankCursor
        onPositionChanged: root.wakeControls()
        onClicked: root.wakeControls()
        onDoubleClicked: root.toggleFullscreenRequested()
    }

    // Paused dimmer
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: root.showCenterPlay ? 0.35 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 220 } }
    }

    // =========================================================================
    // Top bar: back, title, quality
    // =========================================================================
    Rectangle {
        id: topBar

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 130
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#D9000000" }
            GradientStop { position: 1.0; color: "#00000000" }
        }

        HoverHandler { id: topHover }

        RowLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.leftMargin: 24
            anchors.rightMargin: 180
            anchors.topMargin: 22
            spacing: 16

            IconButton {
                id: backBtn
                iconName: "back"
                iconSize: 26
                filled: true
                tip: "Back (Esc)"
                tipAbove: false
                Layout.alignment: Qt.AlignTop

                onClicked: {
                    if (root.player) {
                        root.player.pause()
                    }
                    root.backRequested()
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Text {
                    text: "NOW PLAYING"
                    color: Theme.accentHover
                    font.pixelSize: 10
                    font.weight: Font.Bold
                    font.letterSpacing: 2
                }

                Text {
                    Layout.fillWidth: true
                    text: root.mediaTitle
                    color: "#FFFFFF"
                    font.pixelSize: 24
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                }
            }

            // Stream quality chip
            Rectangle {
                readonly property string label: root.player ? Theme.qualityLabel(root.player.videoWidth, root.player.videoHeight) : ""

                visible: label.length > 0
                Layout.alignment: Qt.AlignVCenter
                implicitHeight: 26
                implicitWidth: qualityText.implicitWidth + 20
                radius: 6
                color: "#33000000"
                border.width: 1
                border.color: "#59FFFFFF"

                Text {
                    id: qualityText
                    anchors.centerIn: parent
                    text: parent.label
                    color: parent.label === "4K" ? Theme.warning : "#FFFFFF"
                    font.pixelSize: 12
                    font.weight: Font.Bold
                    font.letterSpacing: 0.6
                }
            }
        }
    }

    // =========================================================================
    // Centre overlays
    // =========================================================================

    // Big play button while paused / stopped
    Item {
        id: centerPlay

        anchors.centerIn: parent
        width: 96
        height: 96
        opacity: root.showCenterPlay ? 1.0 : 0.0
        visible: opacity > 0.0
        scale: root.showCenterPlay ? 1.0 : 0.8

        Behavior on opacity { NumberAnimation { duration: 200 } }
        Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutBack } }

        Rectangle {
            anchors.fill: parent
            radius: width / 2
            color: centerMouse.containsMouse ? "#66FFFFFF" : "#40FFFFFF"
            border.width: 2
            border.color: "#99FFFFFF"

            Behavior on color { ColorAnimation { duration: 120 } }
        }

        FluxIcon {
            anchors.centerIn: parent
            anchors.horizontalCenterOffset: 3
            name: "play"
            size: 44
            color: "#FFFFFF"
        }

        MouseArea {
            id: centerMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (root.player) root.player.togglePlay()
            }
        }
    }

    // Buffering ring
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        visible: !!root.player && root.player.isBuffering

        FluxSpinner {
            size: 64
            thickness: 4
            running: !!root.player && root.player.isBuffering
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            readonly property int pct: root.player ? Math.round(root.player.bufferingPercent) : 0

            Layout.alignment: Qt.AlignHCenter
            text: (pct > 0 && pct < 100) ? ("Buffering " + pct + "%") : "Buffering"
            color: "#E6FFFFFF"
            font.pixelSize: 13
            font.weight: Font.Medium
        }
    }

    // Playback error
    Rectangle {
        anchors.centerIn: parent
        visible: root.hasError
        width: Math.min(root.width - 64, 460)
        implicitHeight: errorColumn.implicitHeight + 56
        height: implicitHeight
        radius: 18
        color: "#F2101015"
        border.width: 1
        border.color: "#33FF5D5D"

        ColumnLayout {
            id: errorColumn
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 28
            anchors.rightMargin: 28
            spacing: 10

            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 56
                implicitHeight: 56
                radius: 28
                color: "#26FF5D5D"

                FluxIcon {
                    anchors.centerIn: parent
                    name: "alert"
                    size: 26
                    color: Theme.danger
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 4
                text: "Playback error"
                color: Theme.text
                font.pixelSize: 18
                font.weight: Font.Bold
            }

            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: (root.player && root.player.errorMessage.length > 0)
                      ? root.player.errorMessage
                      : "This stream could not be played."
                color: Theme.textDim
                font.pixelSize: 13
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 10
                spacing: 10

                FluxButton {
                    text: "Try again"
                    iconName: "refresh"
                    iconSize: 18

                    onClicked: {
                        if (root.player) root.player.play()
                    }
                }

                FluxButton {
                    text: "Go back"
                    variant: "secondary"

                    onClicked: root.backRequested()
                }
            }
        }
    }

    // =========================================================================
    // Bottom: gradient + transport controls
    // =========================================================================
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 230
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        gradient: Gradient {
            GradientStop { position: 0.0; color: "#00000000" }
            GradientStop { position: 1.0; color: "#E6000000" }
        }
    }

    PlaybackControls {
        id: controlsBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        anchors.bottomMargin: 18
        player: root.player
        isFullscreen: root.isFullscreen
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        onToggleFullscreenRequested: root.toggleFullscreenRequested()
    }
}
