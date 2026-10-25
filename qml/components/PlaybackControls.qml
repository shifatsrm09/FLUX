import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Streaming-style transport controls: full-width seek bar on top, button row below.
Item {
    id: root

    property var player: null
    property bool isFullscreen: false
    property bool hasNext: false
    property bool hasEpisodes: false
    property bool episodesOpen: false

    readonly property bool menuOpen: tracksPopup.visible || root.episodesOpen || serverPopup.visible
    property bool isUserInteracting: (seekSlider.pressed || seekGraceArea.pressed || volumeSlider.pressed || tracksPopup.visible || controlsHover.hovered || root.episodesOpen || serverPopup.visible)

    // Used to stop the "press outside closes popup" + "click toggles popup" double-fire
    property double popupClosedAt: 0
    property double serverPopupClosedAt: 0

    signal toggleFullscreenRequested()
    signal nextRequested()
    signal toggleEpisodesRequested()

    function closeMenus() {
        tracksPopup.close()
        serverPopup.close()
    }

    implicitHeight: controlsColumn.implicitHeight

    HoverHandler { id: controlsHover }

    // Swallow clicks on empty parts of the bar so they don't pause the video underneath
    MouseArea {
        anchors.fill: parent
        z: -1
        acceptedButtons: Qt.LeftButton
    }

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
                Layout.alignment: Qt.AlignVCenter
                horizontalAlignment: Text.AlignRight
                text: root.player ? root.player.formattedTime : "00:00"
                color: Theme.text
                font.pixelSize: 13
                font.weight: Font.DemiBold
            }

            Slider {
                id: seekSlider

                Layout.fillWidth: true
                Layout.preferredHeight: 36
                from: 0.0
                to: 1.0
                value: root.player ? root.player.position : 0.0
                padding: 0
                focusPolicy: Qt.NoFocus
                hoverEnabled: true

                // Slider input can replace its initial QML value binding. Keep it
                // explicitly aligned with the player's optimistic resume position.
                Connections {
                    target: root.player
                    function onPositionChanged() {
                        if (root.player) seekSlider.value = root.player.position
                    }
                }

                Component.onCompleted: {
                    if (root.player) seekSlider.value = root.player.position
                }

                // Invisible grace area above the slider to make seeking effortless
                MouseArea {
                    id: seekGraceArea
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.top
                    height: 40
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor

                    property real hoverX: 0

                    onPositionChanged: function(mouse) {
                        hoverX = mouse.x
                        if (pressed) {
                            var frac = Theme.clamp((mouse.x - seekSlider.leftPadding) / Math.max(1, seekSlider.availableWidth), 0, 1)
                            seekSlider.value = frac
                            if (root.player) root.player.seek(frac)
                        }
                    }

                    onPressed: function(mouse) {
                        hoverX = mouse.x
                        var frac = Theme.clamp((mouse.x - seekSlider.leftPadding) / Math.max(1, seekSlider.availableWidth), 0, 1)
                        seekSlider.value = frac
                        if (root.player) root.player.seek(frac)
                    }
                }

                readonly property bool engaged: seekSlider.hovered || seekSlider.pressed || seekGraceArea.containsMouse || seekGraceArea.pressed
                readonly property real previewCursorX: seekGraceArea.containsMouse ? seekGraceArea.hoverX : seekHover.point.position.x

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
                    visible: (seekHover.hovered || seekGraceArea.containsMouse) && !!root.player && root.player.durationMs > 0
                    x: Theme.clamp(seekSlider.previewCursorX - width / 2, 0, seekSlider.width - width)
                    y: -height - 8
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
                                                 * Theme.clamp((seekSlider.previewCursorX - seekSlider.leftPadding) / Math.max(1, seekSlider.availableWidth), 0, 1))
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
                Layout.alignment: Qt.AlignVCenter
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
                iconName: (root.player && (root.player.isPlaying || root.player.isBuffering)) ? "pause" : "play"
                iconSize: 30
                tip: (root.player && (root.player.isPlaying || root.player.isBuffering)) ? "Pause (Space)" : "Play (Space)"

                onClicked: {
                    if (window && (Date.now() - window.lastActivatedAt < 350)) return
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

            IconButton {
                visible: root.hasNext
                iconName: "next"
                iconSize: 28
                tip: "Next episode"

                onClicked: root.nextRequested()
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
                        if (root.player.volume < 60) return "volumeLow"
                        return "volume"
                    }
                    iconSize: 28
                    tip: (root.player && root.player.muted) ? "Unmute (M)" : "Mute (M)"

                    onClicked: {
                        if (root.player) root.player.muted = !root.player.muted
                    }
                }

                Item {
                    width: volumeGroup.expanded ? 170 : 0
                    height: muteBtn.height
                    clip: true

                    Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

                    Text {
                        id: volumeLabel
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: 46
                        horizontalAlignment: Text.AlignRight
                        text: Math.round(volumeSlider.value) + "%"
                        color: volumeSlider.value > 100 ? Theme.warning : Theme.textDim
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }

                    Slider {
                        id: volumeSlider

                        anchors.left: parent.left
                        anchors.leftMargin: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 108
                        height: 24
                        from: 0
                        to: 200
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
                                    color: volumeSlider.value > 100 ? Theme.warning : "#FFFFFF"
                                }

                                // 100% marker: everything right of it is software boost
                                Rectangle {
                                    x: parent.width / 2 - 1
                                    y: -3
                                    width: 2
                                    height: parent.height + 6
                                    color: "#80FFFFFF"
                                }
                            }
                        }

                        handle: Rectangle {
                            x: volumeSlider.leftPadding + volumeSlider.visualPosition * volumeSlider.availableWidth - width / 2
                            y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
                            width: 13
                            height: 13
                            radius: 6.5
                            color: volumeSlider.value > 100 ? Theme.warning : "#FFFFFF"
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

            // Server toggle button with animated arrow (Top Picks only)
            Item {
                id: serverBtn
                readonly property bool canSwitchServer: {
                    if (!window || !window.currentIsTopPick || !window.currentTopPickItem) return false
                    var item = window.currentTopPickItem
                    var count = (item.url ? 1 : 0) + (item.altUrl ? 1 : 0) + (item.circleUrl ? 1 : 0)
                    return count > 1
                }
                visible: canSwitchServer
                onVisibleChanged: {
                    if (!visible && serverPopup.visible) {
                        serverPopup.close()
                    }
                }
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 38
                implicitHeight: 38

                readonly property string currentServer: window ? window.currentServerName : ""
                readonly property bool isOpen: serverPopup.visible

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: serverBtnMouse.pressed ? "#40FFFFFF"
                         : (serverBtnMouse.containsMouse || serverBtn.isOpen ? "#26FFFFFF" : "transparent")

                    Behavior on color { ColorAnimation { duration: 120 } }
                }

                FluxIcon {
                    id: serverArrowIcon
                    anchors.centerIn: parent
                    name: "chevronUp"
                    size: 20
                    color: serverBtn.isOpen ? Theme.accentHover : (serverBtnMouse.containsMouse ? "#FFFFFF" : "#D0FFFFFF")
                    rotation: serverBtn.isOpen ? 180 : 0

                    Behavior on rotation {
                        NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
                    }
                    Behavior on color {
                        ColorAnimation { duration: 120 }
                    }
                }

                MouseArea {
                    id: serverBtnMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (serverPopup.visible) {
                            serverPopup.close()
                        } else if (Date.now() - root.serverPopupClosedAt > 250) {
                            serverPopup.open()
                        }
                    }
                }

                FluxToolTip {
                    visible: serverBtnMouse.containsMouse && !serverPopup.visible
                    text: "Server (" + (window && window.currentServerName ? window.currentServerName : "Server") + ")"
                    above: true
                }
            }

            // Episode list
            IconButton {
                id: episodesBtn
                iconName: "episodes"
                iconSize: 28
                tip: "Episodes"
                iconColor: root.episodesOpen ? Theme.accentHover : (root.hasEpisodes ? "#FFFFFF" : "#88FFFFFF")

                onClicked: root.toggleEpisodesRequested()
            }

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

    // ---- Server selection popup -----------------------------------------------------------
    Popup {
        id: serverPopup

        parent: serverBtn
        x: (serverBtn.width - width) / 2
        y: -height - 12
        width: 170
        padding: 10
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        readonly property string activeServer: window ? window.currentServerName : ""
        readonly property var topPickItem: window ? window.currentTopPickItem : null

        onClosed: root.serverPopupClosedAt = Date.now()

        enter: Transition {
            NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 140 }
            NumberAnimation { property: "scale"; from: 0.95; to: 1.0; duration: 140; easing.type: Easing.OutCubic }
        }
        exit: Transition {
            NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 100 }
            NumberAnimation { property: "scale"; from: 1.0; to: 0.95; duration: 100; easing.type: Easing.InCubic }
        }

        background: Rectangle {
            radius: 14
            color: "#F2101015"
            border.width: 1
            border.color: "#33FFFFFF"
        }

        contentItem: ColumnLayout {
            spacing: 4

            Text {
                text: "SERVER"
                color: Theme.textDim
                font.pixelSize: 10
                font.weight: Font.Bold
                font.letterSpacing: 1.2
                Layout.leftMargin: 8
                Layout.topMargin: 2
                Layout.bottomMargin: 2
            }

            // Option 1: Dhaka Flix
            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: 8
                visible: !!(serverPopup.topPickItem && serverPopup.topPickItem.url)
                color: dhakaOptionMouse.containsMouse ? "#20FFFFFF" : (serverPopup.activeServer === "Dhaka Flix" ? "#18FFFFFF" : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: "\u2713"
                        color: Theme.accentHover
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        visible: serverPopup.activeServer === "Dhaka Flix"
                        Layout.preferredWidth: 14
                    }
                    Item {
                        visible: serverPopup.activeServer !== "Dhaka Flix"
                        Layout.preferredWidth: 14
                    }

                    Text {
                        text: "Dhaka Flix"
                        color: serverPopup.activeServer === "Dhaka Flix" ? "#FFFFFF" : Theme.textDim
                        font.pixelSize: 13
                        font.weight: serverPopup.activeServer === "Dhaka Flix" ? Font.DemiBold : Font.Normal
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: dhakaOptionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        serverPopup.close()
                        if (serverPopup.activeServer !== "Dhaka Flix" && window && window.switchTopPickToServer) {
                            window.switchTopPickToServer(1)
                        }
                    }
                }
            }

            // Option 2: ICC
            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: 8
                visible: !!(serverPopup.topPickItem && serverPopup.topPickItem.altUrl)
                color: iccOptionMouse.containsMouse ? "#20FFFFFF" : (serverPopup.activeServer === "ICC FTP" ? "#18FFFFFF" : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: "\u2713"
                        color: Theme.accentHover
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        visible: serverPopup.activeServer === "ICC FTP"
                        Layout.preferredWidth: 14
                    }
                    Item {
                        visible: serverPopup.activeServer !== "ICC FTP"
                        Layout.preferredWidth: 14
                    }

                    Text {
                        text: "ICC"
                        color: serverPopup.activeServer === "ICC FTP" ? "#FFFFFF" : Theme.textDim
                        font.pixelSize: 13
                        font.weight: serverPopup.activeServer === "ICC FTP" ? Font.DemiBold : Font.Normal
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: iccOptionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        serverPopup.close()
                        if (serverPopup.activeServer !== "ICC FTP" && window && window.switchTopPickToServer) {
                            window.switchTopPickToServer(2)
                        }
                    }
                }
            }

            // Option 3: Circle FTP
            Rectangle {
                Layout.fillWidth: true
                height: 36
                radius: 8
                visible: !!(serverPopup.topPickItem && serverPopup.topPickItem.circleUrl)
                color: circleOptionMouse.containsMouse ? "#20FFFFFF" : (serverPopup.activeServer === "Circle FTP" ? "#18FFFFFF" : "transparent")

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 10
                    anchors.rightMargin: 10
                    spacing: 8

                    Text {
                        text: "\u2713"
                        color: Theme.accentHover
                        font.pixelSize: 14
                        font.weight: Font.Bold
                        visible: serverPopup.activeServer === "Circle FTP"
                        Layout.preferredWidth: 14
                    }
                    Item {
                        visible: serverPopup.activeServer !== "Circle FTP"
                        Layout.preferredWidth: 14
                    }

                    Text {
                        text: "Circle FTP"
                        color: serverPopup.activeServer === "Circle FTP" ? "#FFFFFF" : Theme.textDim
                        font.pixelSize: 13
                        font.weight: serverPopup.activeServer === "Circle FTP" ? Font.DemiBold : Font.Normal
                        Layout.fillWidth: true
                    }
                }

                MouseArea {
                    id: circleOptionMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        serverPopup.close()
                        if (serverPopup.activeServer !== "Circle FTP" && window && window.switchTopPickToServer) {
                            window.switchTopPickToServer(3)
                        }
                    }
                }
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

        contentItem: ColumnLayout {
            spacing: 16

            RowLayout {
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
                Layout.preferredWidth: 215
                Layout.alignment: Qt.AlignTop
                spacing: 10

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    Text {
                        text: "Subtitles"
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.Bold
                    }

                    Item { Layout.fillWidth: true }

                    // Indicator for pre-rendered bitmap subtitles (e.g. Blu-ray PGS in Oppenheimer)
                    Text {
                        text: "PGS Image Subtitle"
                        color: Theme.textMute
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        visible: !!root.player && root.player.currentSubtitleType === "bitmap" && root.player.selectedSubtitleTrack >= 0
                    }

                    // Indicator for author-styled SSA/ASS vector subtitles (e.g. Princess Mononoke)
                    Text {
                        text: "ASS Styled Subtitle"
                        color: Theme.textMute
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        visible: !!root.player && root.player.currentSubtitleType === "ass" && root.player.selectedSubtitleTrack >= 0
                    }

                    // Real-time subtitle scale stepper (active only for scalable text subtitles)
                    RowLayout {
                        spacing: 2
                        visible: !!root.player && root.player.isCurrentSubtitleScalable

                        Item {
                            implicitWidth: 24
                            implicitHeight: 24

                            readonly property bool canDecrease: !!(root.player && root.player.subtitleScale > 50)

                            Rectangle {
                                anchors.fill: parent
                                radius: width / 2
                                color: downMouse.pressed ? "#40FFFFFF"
                                     : (downMouse.containsMouse ? "#26FFFFFF" : "transparent")
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }

                            FluxIcon {
                                anchors.centerIn: parent
                                name: "chevronDown"
                                size: 14
                                color: parent.canDecrease
                                       ? (downMouse.containsMouse ? "#FFFFFF" : "#D0FFFFFF")
                                       : "#35FFFFFF"
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }

                            MouseArea {
                                id: downMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: parent.canDecrease ? Qt.PointingHandCursor : Qt.ArrowCursor
                                enabled: parent.canDecrease
                                onClicked: {
                                    if (root.player) root.player.decreaseSubtitleScale()
                                }
                            }

                            FluxToolTip {
                                visible: downMouse.containsMouse && parent.canDecrease
                                text: "Decrease subtitle size"
                                above: true
                            }
                        }

                        Text {
                            text: (root.player ? root.player.subtitleScale : 100) + "%"
                            color: Theme.textDim
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            Layout.preferredWidth: 32
                            horizontalAlignment: Text.AlignHCenter
                        }

                        Item {
                            implicitWidth: 24
                            implicitHeight: 24

                            readonly property bool canIncrease: !!(root.player && root.player.subtitleScale < 200)

                            Rectangle {
                                anchors.fill: parent
                                radius: width / 2
                                color: upMouse.pressed ? "#40FFFFFF"
                                     : (upMouse.containsMouse ? "#26FFFFFF" : "transparent")
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }

                            FluxIcon {
                                anchors.centerIn: parent
                                name: "chevronUp"
                                size: 14
                                color: parent.canIncrease
                                       ? (upMouse.containsMouse ? "#FFFFFF" : "#D0FFFFFF")
                                       : "#35FFFFFF"
                                Behavior on color { ColorAnimation { duration: 120 } }
                            }

                            MouseArea {
                                id: upMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: parent.canIncrease ? Qt.PointingHandCursor : Qt.ArrowCursor
                                enabled: parent.canIncrease
                                onClicked: {
                                    if (root.player) root.player.increaseSubtitleScale()
                                }
                            }

                            FluxToolTip {
                                visible: upMouse.containsMouse && parent.canIncrease
                                text: "Increase subtitle size"
                                above: true
                            }
                        }
                    }
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

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: "#1FFFFFFF"
            }

            // Playback preference
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        text: "Autoplay next episode"
                        color: Theme.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }

                    Text {
                        text: "Continue with the next file in the folder when this one ends"
                        color: Theme.textMute
                        font.pixelSize: 11
                    }
                }

                ToggleSwitch {
                    checked: !!fluxUser && fluxUser.autoplayNext
                    onToggled: function(value) {
                        if (fluxUser) fluxUser.autoplayNext = value
                    }
                }
            }
        }
    }
}
