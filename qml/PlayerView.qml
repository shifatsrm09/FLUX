import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Flux.Media 1.0
import "components"
import "components/Theme.js" as Theme
import "components/MediaFormatter.js" as Formatter

Rectangle {
    id: root

    property var player: null
    property string mediaTitle: ""
    property bool isFullscreen: false
    signal toggleFullscreenRequested()
    signal backRequested()
    signal playNextRequested(string url, string name)

    // ---- Next episode / autoplay ---------------------------------------------------
    property string nextUrl: ""
    property string nextName: ""
    property bool autoplayNext: true

    readonly property bool hasNext: root.nextUrl.length > 0
    readonly property bool ended: (!!root.player && root.player.state === "Ended")
    readonly property real remainingMs: (!!root.player && root.player.durationMs > 0)
                                        ? (root.player.durationMs - root.player.timeMs) : -1
    readonly property bool nearEnd: (root.hasNext && !!root.player && root.player.durationMs > 120000
                                     && root.remainingMs >= 0 && root.remainingMs < 30000)
    property bool nextDismissed: false
    readonly property bool showNextCard: (root.hasNext && !root.nextDismissed && (root.ended || root.nearEnd))
    readonly property var nextInfo: root.nextName.length > 0
                                    ? Formatter.describe(root.nextName, false, "")
                                    : ({ title: "", episode: "", year: "" })
    property int countdown: -1          // seconds until autoplay, -1 = not counting
    readonly property int countdownTotal: 6

    // ---- Resume toast -----------------------------------------------------------------
    property real resumeMs: 0
    property bool resumeToastVisible: false

    function offerResume(ms) {
        root.resumeMs = ms
        root.resumeToastVisible = ms > 0
        if (ms > 0) resumeTimer.restart()
    }

    function playNext() {
        if (!root.hasNext) return
        var u = root.nextUrl
        var n = root.nextName
        countdownTimer.stop()
        root.countdown = -1
        root.playNextRequested(u, n)
    }

    function dismissNext() {
        root.nextDismissed = true
        countdownTimer.stop()
        root.countdown = -1
    }

    onNextUrlChanged: {
        root.nextDismissed = false
        countdownTimer.stop()
        root.countdown = -1
    }

    // When the episode finishes, count down and continue automatically
    onEndedChanged: {
        if (root.ended && root.hasNext && !root.nextDismissed && root.autoplayNext) {
            root.countdown = root.countdownTotal
            countdownTimer.restart()
        } else {
            countdownTimer.stop()
            root.countdown = -1
        }
    }

    Timer {
        id: countdownTimer
        interval: 1000
        repeat: true
        onTriggered: {
            root.countdown = root.countdown - 1
            if (root.countdown <= 0) {
                countdownTimer.stop()
                root.playNext()
            }
        }
    }

    Timer {
        id: resumeTimer
        interval: 7000
        repeat: false
        onTriggered: root.resumeToastVisible = false
    }

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

        // Pause/resume on the very first click, with no waiting to see whether a second
        // click follows. A double click then undoes that toggle and goes fullscreen.
        onClicked: {
            root.wakeControls()
            if (root.player) root.player.togglePlay()
        }

        onDoubleClicked: {
            if (root.player) root.player.togglePlay()
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

        Behavior on opacity { NumberAnimation { duration: 90 } }
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
            anchors.rightMargin: 32
            anchors.topMargin: 28
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

                Item {
                    Layout.fillWidth: true
                    Layout.preferredHeight: titleText.implicitHeight
                    implicitHeight: titleText.implicitHeight

                    Text {
                        id: titleText
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, parent.width * 0.80)
                        text: root.mediaTitle
                        color: "#FFFFFF"
                        font.pixelSize: 24
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    // Stream quality chip (optically aligned with title glyphs, stuck to right wall)
                    Rectangle {
                        id: qualityChip
                        readonly property string label: root.player ? Theme.qualityLabel(root.player.videoWidth, root.player.videoHeight) : ""

                        visible: label.length > 0
                        anchors.right: parent.right
                        anchors.verticalCenter: titleText.verticalCenter
                        anchors.verticalCenterOffset: 4
                        implicitHeight: 24
                        implicitWidth: qualityText.implicitWidth + 18
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

        Behavior on opacity { NumberAnimation { duration: 90 } }
        Behavior on scale { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }

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
        hasNext: root.hasNext
        opacity: root.showControls ? 1.0 : 0.0
        visible: opacity > 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        onToggleFullscreenRequested: root.toggleFullscreenRequested()
        onNextRequested: root.playNext()
    }

    // =========================================================================
    // Floating cards: resume toast (left) and "Up next" (right). They glide up and
    // down with the controls bar.
    // =========================================================================
    ResumeToast {
        id: resumeToast

        anchors.left: parent.left
        anchors.leftMargin: 32
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.showControls ? 150 : 40
        text: "Resumed from " + Theme.formatTime(root.resumeMs)
        opacity: root.resumeToastVisible ? 1.0 : 0.0
        visible: opacity > 0.0
        scale: root.resumeToastVisible ? 1.0 : 0.94

        Behavior on anchors.bottomMargin { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 260; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 320; easing.type: Easing.OutCubic } }

        onStartOver: {
            if (root.player) root.player.seek(0)
            root.resumeToastVisible = false
        }
    }

    NextEpisodeCard {
        id: nextCard

        anchors.right: parent.right
        anchors.rightMargin: 32
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.showControls ? 150 : 40
        title: root.nextInfo.title
        subtitle: root.nextInfo.episode.length > 0 ? root.nextInfo.episode : root.nextInfo.year
        countdownSeconds: root.countdown
        countdownFraction: root.countdown >= 0 ? (root.countdown / root.countdownTotal) : 0
        opacity: root.showNextCard ? 1.0 : 0.0
        visible: opacity > 0.0
        scale: root.showNextCard ? 1.0 : 0.94

        Behavior on anchors.bottomMargin { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 340; easing.type: Easing.OutCubic } }

        onPlayNow: root.playNext()
        onDismissed: root.dismissNext()
    }
}
