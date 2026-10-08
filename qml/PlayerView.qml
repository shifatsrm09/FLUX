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
    readonly property bool buffering: (!!root.player && root.player.isBuffering)
    readonly property bool showCenterPlay: (!!root.player && !root.buffering && !root.hasError
                                            && (root.player.state === "Paused"
                                                || root.player.state === "Stopped"
                                                || root.player.state === "Ended"))

    // Spinner only appears if a buffer lasts longer than a blink, so quick
    // seeks never flash a loading indicator.
    property bool showSpinner: false

    // On-screen indicator (volume / seek feedback)
    property bool osdVisible: false
    property string osdText: ""
    property real osdLevel: -1
    property real wheelAcc: 0

    function closeMenus() {
        controlsBar.closeMenus()
    }

    function wakeControls() {
        root.showControls = true
        hideControlsTimer.restart()
    }

    function showOsd(text, level) {
        root.osdText = text
        root.osdLevel = (level === undefined) ? -1 : level
        root.osdVisible = true
        osdTimer.restart()
    }

    // Volume goes 0..200% (libVLC software amplification)
    function adjustVolume(delta) {
        if (!root.player) return
        var v = Math.max(0, Math.min(200, root.player.volume + delta))
        if (root.player.muted && v > 0) root.player.muted = false
        root.player.volume = v
        root.showOsd("Volume " + v + "%", v)
    }

    function toggleMute() {
        if (!root.player) return
        root.player.muted = !root.player.muted
        if (root.player.muted) {
            root.showOsd("Muted", 0)
        } else {
            root.showOsd("Volume " + root.player.volume + "%", root.player.volume)
        }
    }

    function seekBy(ms) {
        if (!root.player) return
        root.player.seekRelative(ms)
        var s = Math.round(Math.abs(ms) / 1000)
        root.showOsd((ms < 0 ? "\u2212" : "+") + s + "s", -1)
    }

    onVisibleChanged: {
        if (root.visible) root.wakeControls()
    }

    onBufferingChanged: {
        if (root.buffering) {
            spinnerDelay.restart()
        } else {
            spinnerDelay.stop()
            root.showSpinner = false
        }
    }

    Timer {
        id: hideControlsTimer
        interval: 3500
        running: !!root.player && root.player.isPlaying && !controlsBar.isUserInteracting && !topHover.hovered
        repeat: false
        onTriggered: root.showControls = false
    }

    Timer {
        id: spinnerDelay
        interval: 450
        repeat: false
        onTriggered: root.showSpinner = root.buffering
    }

    Timer {
        id: osdTimer
        interval: 1200
        repeat: false
        onTriggered: root.osdVisible = false
    }

    // Single click toggles pause; waiting briefly lets a double click (fullscreen)
    // cancel it instead of pausing and resuming.
    Timer {
        id: clickTimer
        interval: 250
        repeat: false
        onTriggered: {
            if (root.player) root.player.togglePlay()
        }
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

    // Mouse: click = play/pause, double click = fullscreen, wheel = volume
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.showControls ? Qt.ArrowCursor : Qt.BlankCursor

        onPositionChanged: root.wakeControls()

        onClicked: {
            root.wakeControls()
            clickTimer.restart()
        }

        onDoubleClicked: {
            clickTimer.stop()
            root.toggleFullscreenRequested()
        }

        onWheel: function(wheel) {
            // 120 units (one mouse notch) = 5%; smooth touchpads accumulate
            root.wheelAcc += wheel.angleDelta.y
            var steps = Math.trunc(root.wheelAcc / 24)
            if (steps !== 0) {
                root.wheelAcc -= steps * 24
                root.adjustVolume(steps)
            }
            wheel.accepted = true
        }
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

    // Buffering ring (only for buffers that last longer than a blink)
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12
        visible: root.showSpinner && root.buffering

        FluxSpinner {
            size: 64
            thickness: 4
            running: root.showSpinner && root.buffering
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

    // On-screen indicator: volume level / seek amount
    Rectangle {
        id: osd

        anchors.horizontalCenter: parent.horizontalCenter
        y: 118
        implicitHeight: 44
        width: osdRow.implicitWidth + 40
        radius: 22
        color: "#E6101015"
        border.width: 1
        border.color: "#26FFFFFF"
        opacity: root.osdVisible ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 150 } }

        RowLayout {
            id: osdRow
            anchors.centerIn: parent
            spacing: 14

            Text {
                text: root.osdText
                color: (root.osdLevel > 100) ? Theme.warning : "#FFFFFF"
                font.pixelSize: 14
                font.weight: Font.Bold
            }

            Rectangle {
                visible: root.osdLevel >= 0
                implicitWidth: 120
                implicitHeight: 4
                radius: 2
                color: "#40FFFFFF"
                Layout.alignment: Qt.AlignVCenter

                Rectangle {
                    width: parent.width * Math.min(1.0, Math.max(0.0, root.osdLevel / 200.0))
                    height: parent.height
                    radius: 2
                    color: (root.osdLevel > 100) ? Theme.warning : "#FFFFFF"
                }

                // 100% marker
                Rectangle {
                    x: parent.width / 2 - 1
                    y: -2
                    width: 2
                    height: parent.height + 4
                    color: "#80FFFFFF"
                }
            }
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
