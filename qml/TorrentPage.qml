import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components"
import "components/Theme.js" as Theme

Item {
    id: root

    property int topInset: 48
    property bool active: false
    property bool autoPlay: true
    property bool autoPlayTriggered: false
    signal playRequested(string url, string title)

    Connections {
        target: fluxTorrent
        function onTorrentReadyToPlay(url, title) {
            if (root.autoPlay && !root.autoPlayTriggered && url && url.length > 0) {
                root.autoPlayTriggered = true
                root.playRequested(url, title)
            }
        }
    }

    // Full-page drag and drop for .torrent files
    DropArea {
        id: fullPageDropArea
        anchors.fill: parent
        onEntered: function(drag) {
            if (drag.hasUrls) {
                for (var i = 0; i < drag.urls.length; i++) {
                    var u = drag.urls[i].toString()
                    if (u.toLowerCase().endsWith(".torrent")) {
                        drag.acceptProposedAction()
                        return
                    }
                }
            }
        }
        onDropped: function(drop) {
            if (drop.hasUrls && drop.urls.length > 0) {
                var urlStr = drop.urls[0].toString()
                root.handleInput(urlStr)
                drop.acceptProposedAction()
            }
        }
    }

    function handleInput(val) {
        var trimmed = (val || "").trim()
        if (trimmed.length > 0 && fluxTorrent) {
            root.autoPlayTriggered = false
            fluxTorrent.startTorrent(trimmed)
        }
    }

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.topMargin: root.topInset
        contentWidth: width
        contentHeight: contentCol.implicitHeight + 80
        clip: true

        ColumnLayout {
            id: contentCol
            width: Math.min(parent.width - 64, 960)
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 24

            Item { implicitHeight: 12 }

            // ---- Header ----
            ColumnLayout {
                spacing: 8
                Layout.fillWidth: true

                RowLayout {
                    spacing: 12
                    Rectangle {
                        width: 36
                        height: 36
                        radius: 10
                        color: Theme.accentSoft
                        border.width: 1
                        border.color: Theme.accent

                        FluxIcon {
                            anchors.centerIn: parent
                            name: "download"
                            size: 18
                            color: Theme.accent
                        }
                    }

                    Text {
                        text: "Torrent Streamer"
                        color: "#FFFFFF"
                        font.pixelSize: 26
                        font.weight: Font.Bold
                    }
                }

                Text {
                    text: "Stream videos instantly from magnet links or .torrent files without waiting for full download."
                    color: Theme.textDim
                    font.pixelSize: 14
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                }
            }

            // ---- Engine Setup Banner (shown if downloading TorrServer) ----
            Rectangle {
                visible: !!fluxTorrent && fluxTorrent.isSettingUp
                Layout.fillWidth: true
                implicitHeight: setupCol.implicitHeight + 24
                radius: 12
                color: Theme.surface
                border.width: 1
                border.color: Theme.accent

                ColumnLayout {
                    id: setupCol
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 10

                    RowLayout {
                        spacing: 8
                        BusyIndicator {
                            running: true
                            implicitWidth: 18
                            implicitHeight: 18
                        }
                        Text {
                            text: fluxTorrent ? fluxTorrent.setupStatus : "Setting up torrent engine..."
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                    }

                    ProgressBar {
                        Layout.fillWidth: true
                        value: fluxTorrent ? fluxTorrent.setupProgress : 0
                    }
                }
            }

            // ---- INPUT SECTION (Active when idle or error) ----
            Rectangle {
                visible: !fluxTorrent || fluxTorrent.status === "idle" || fluxTorrent.status === "error"
                Layout.fillWidth: true
                implicitHeight: inputSectionCol.implicitHeight + 36
                radius: 16
                color: Theme.surface
                border.width: 1
                border.color: Theme.border

                ColumnLayout {
                    id: inputSectionCol
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 20

                    // Magnet Link Input Row
                    ColumnLayout {
                        spacing: 8
                        Layout.fillWidth: true

                        Text {
                            text: "Paste Magnet Link"
                            color: Theme.text
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 10

                            Rectangle {
                                Layout.fillWidth: true
                                height: 46
                                radius: 10
                                color: Theme.surfaceHi
                                border.width: 1
                                border.color: magnetInput.activeFocus ? Theme.accent : Theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 14
                                    anchors.rightMargin: 10
                                    spacing: 10

                                    FluxIcon {
                                        name: "search"
                                        size: 16
                                        color: Theme.textDim
                                    }

                                    TextInput {
                                        id: magnetInput
                                        Layout.fillWidth: true
                                        color: Theme.text
                                        font.pixelSize: 14
                                        verticalAlignment: TextInput.AlignVCenter
                                        clip: true
                                        selectByMouse: true

                                        Text {
                                            anchors.fill: parent
                                            text: "magnet:?xt=urn:btih:... or torrent URL"
                                            color: Theme.textDim
                                            font.pixelSize: 14
                                            verticalAlignment: Text.AlignVCenter
                                            visible: !magnetInput.text && !magnetInput.activeFocus
                                        }

                                        onAccepted: root.handleInput(magnetInput.text)
                                    }

                                    // Clear button
                                    Rectangle {
                                        visible: magnetInput.text.length > 0
                                        width: 24
                                        height: 24
                                        radius: 12
                                        color: "transparent"
                                        FluxIcon {
                                            anchors.centerIn: parent
                                            name: "close"
                                            size: 12
                                            color: Theme.textDim
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: magnetInput.text = ""
                                        }
                                    }
                                }
                            }

                            // Start Stream Button
                            Rectangle {
                                implicitWidth: startBtnRow.implicitWidth + 28
                                height: 46
                                radius: 10
                                color: startMouse.containsMouse ? Theme.accentHi : Theme.accent
                                border.width: 1
                                border.color: Theme.accent

                                RowLayout {
                                    id: startBtnRow
                                    anchors.centerIn: parent
                                    spacing: 8

                                    FluxIcon {
                                        name: "play"
                                        size: 14
                                        color: "#FFFFFF"
                                    }

                                    Text {
                                        text: "Start Stream"
                                        color: "#FFFFFF"
                                        font.pixelSize: 13
                                        font.weight: Font.Bold
                                    }
                                }

                                MouseArea {
                                    id: startMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: root.handleInput(magnetInput.text)
                                }
                            }
                        }
                    }

                    // Divider "OR"
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.border
                        }

                        Text {
                            text: "OR"
                            color: Theme.textDim
                            font.pixelSize: 12
                            font.weight: Font.Bold
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 1
                            color: Theme.border
                        }
                    }

                    // Drag & Drop Box
                    Rectangle {
                        Layout.fillWidth: true
                        height: 130
                        radius: 12
                        color: dropAreaMouse.containsMouse ? Theme.surfaceHi : Theme.surface
                        border.width: 2
                        border.color: dropAreaMouse.containsMouse ? Theme.accent : Theme.border

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 8

                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                width: 44
                                height: 44
                                radius: 22
                                color: Theme.surfaceHi
                                border.width: 1
                                border.color: Theme.border

                                FluxIcon {
                                    anchors.centerIn: parent
                                    name: "download"
                                    size: 20
                                    color: Theme.accent
                                }
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignHCenter
                                spacing: 4

                                Text {
                                    text: "Drag and drop a "
                                    color: Theme.textDim
                                    font.pixelSize: 13
                                }

                                Text {
                                    text: ".torrent file"
                                    color: Theme.text
                                    font.weight: Font.Bold
                                    font.pixelSize: 13
                                }

                                Text {
                                    text: " here, or "
                                    color: Theme.textDim
                                    font.pixelSize: 13
                                }

                                Text {
                                    text: "Browse"
                                    color: Theme.accent
                                    font.weight: Font.Bold
                                    font.pixelSize: 13
                                }
                            }
                        }

                        MouseArea {
                            id: dropAreaMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (fluxTorrent) {
                                    var chosen = fluxTorrent.browseTorrentFile()
                                    if (chosen && chosen.length > 0) {
                                        root.handleInput(chosen)
                                    }
                                }
                            }
                        }
                    }

                    // Error text if any
                    Text {
                        visible: !!fluxTorrent && fluxTorrent.errorMessage.length > 0
                        text: fluxTorrent ? fluxTorrent.errorMessage : ""
                        color: "#FF4D5E"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            // ---- ACTIVE TORRENT CARD (when loaded) ----
            Rectangle {
                visible: !!fluxTorrent && fluxTorrent.status !== "idle" && fluxTorrent.status !== "error"
                Layout.fillWidth: true
                implicitHeight: activeCol.implicitHeight + 40
                radius: 16
                color: Theme.surface
                border.width: 1
                border.color: Theme.borderHi

                ColumnLayout {
                    id: activeCol
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 20

                    // Title & Status Badge
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4

                            Text {
                                text: fluxTorrent ? fluxTorrent.torrentName : "Torrent Stream"
                                color: "#FFFFFF"
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: fluxTorrent ? ("Total Size: " + fluxTorrent.formattedSize) : ""
                                color: Theme.textDim
                                font.pixelSize: 13
                            }
                        }

                        // Status pill
                        Rectangle {
                            implicitHeight: 28
                            implicitWidth: statusPillRow.implicitWidth + 20
                            radius: 14
                            color: {
                                var st = fluxTorrent ? fluxTorrent.status : ""
                                if (st === "ready") return "#1A22C55E"
                                if (st === "preload") return "#1A06B6D4"
                                return "#1AEAB308"
                            }
                            border.width: 1
                            border.color: {
                                var st = fluxTorrent ? fluxTorrent.status : ""
                                if (st === "ready") return "#22C55E"
                                if (st === "preload") return "#06B6D4"
                                return "#EAB308"
                            }

                            RowLayout {
                                id: statusPillRow
                                anchors.centerIn: parent
                                spacing: 6

                                Rectangle {
                                    width: 6
                                    height: 6
                                    radius: 3
                                    color: parent.parent.border.color
                                }

                                Text {
                                    text: fluxTorrent ? fluxTorrent.statusMessage : ""
                                    color: parent.parent.border.color
                                    font.pixelSize: 12
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }

                    // Stats Metrics Row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        // Download Speed
                        Rectangle {
                            Layout.fillWidth: true
                            height: 60
                            radius: 10
                            color: Theme.surfaceHi
                            border.width: 1
                            border.color: Theme.border

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "DOWNLOAD SPEED"
                                    color: Theme.textDim
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: fluxTorrent ? fluxTorrent.formattedDownloadSpeed : "0 KB/s"
                                    color: "#22C55E"
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }

                        // Upload Speed
                        Rectangle {
                            Layout.fillWidth: true
                            height: 60
                            radius: 10
                            color: Theme.surfaceHi
                            border.width: 1
                            border.color: Theme.border

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "UPLOAD SPEED"
                                    color: Theme.textDim
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: fluxTorrent ? fluxTorrent.formattedUploadSpeed : "0 KB/s"
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }

                        // Peers
                        Rectangle {
                            Layout.fillWidth: true
                            height: 60
                            radius: 10
                            color: Theme.surfaceHi
                            border.width: 1
                            border.color: Theme.border

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: 2
                                Text {
                                    text: "PEERS"
                                    color: Theme.textDim
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                                Text {
                                    text: fluxTorrent ? (fluxTorrent.activePeers + " / " + fluxTorrent.totalPeers) : "0"
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                    Layout.alignment: Qt.AlignHCenter
                                }
                            }
                        }
                    }

                    // Progress Bar
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: "Buffer / Cache Progress"
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: fluxTorrent ? Math.round(fluxTorrent.bufferProgress * 100) + "%" : "0%"
                                color: Theme.text
                                font.pixelSize: 12
                                font.weight: Font.Bold
                            }
                        }

                        Rectangle {
                            Layout.fillWidth: true
                            height: 6
                            radius: 3
                            color: Theme.surfaceHi

                            Rectangle {
                                width: parent.width * (fluxTorrent ? fluxTorrent.bufferProgress : 0)
                                height: parent.height
                                radius: 3
                                color: Theme.accent
                            }
                        }
                    }

                    // File Selection Section (if multiple files exist)
                    ColumnLayout {
                        visible: !!fluxTorrent && fluxTorrent.files.length > 0
                        Layout.fillWidth: true
                        spacing: 8

                        Text {
                            text: "Torrent Files (" + (fluxTorrent ? fluxTorrent.files.length : 0) + ")"
                            color: Theme.text
                            font.pixelSize: 13
                            font.weight: Font.Bold
                        }

                        ListView {
                            id: fileListView
                            Layout.fillWidth: true
                            implicitHeight: Math.min(contentHeight, 180)
                            clip: true
                            model: fluxTorrent ? fluxTorrent.files : []
                            spacing: 6

                            delegate: Rectangle {
                                width: fileListView.width
                                height: 38
                                radius: 8
                                color: modelData.selected ? Theme.accentSoft : (fileMouse.containsMouse ? Theme.surfaceHi : "transparent")
                                border.width: 1
                                border.color: modelData.selected ? Theme.accent : Theme.border

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    spacing: 10

                                    FluxIcon {
                                        name: modelData.isVideo ? "play" : "file"
                                        size: 14
                                        color: modelData.selected ? Theme.accent : Theme.textDim
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: modelData.selected ? "#FFFFFF" : Theme.text
                                        font.pixelSize: 13
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        text: modelData.formattedSize
                                        color: Theme.textDim
                                        font.pixelSize: 12
                                    }
                                }

                                MouseArea {
                                    id: fileMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (fluxTorrent) {
                                            fluxTorrent.selectFile(modelData.id)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Sequential Streaming Notice & Auto-play toggle
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        RowLayout {
                            spacing: 8
                            Rectangle {
                                width: 8
                                height: 8
                                radius: 4
                                color: (fluxTorrent && fluxTorrent.status === "preload") ? "#06B6D4" : ((fluxTorrent && fluxTorrent.status === "ready") ? "#22C55E" : "#EAB308")

                                SequentialAnimation on opacity {
                                    running: !!fluxTorrent && fluxTorrent.status === "preload"
                                    loops: Animation.Infinite
                                    NumberAnimation { to: 0.3; duration: 600 }
                                    NumberAnimation { to: 1.0; duration: 600 }
                                }
                            }
                            Text {
                                text: (fluxTorrent && fluxTorrent.status === "preload")
                                    ? "Sequential downloading active — play anytime"
                                    : ((fluxTorrent && fluxTorrent.status === "ready")
                                        ? "Sequential buffer ready · ready to stream"
                                        : "Connecting to swarm...")
                                color: Theme.textDim
                                font.pixelSize: 12
                            }
                        }

                        Item { Layout.fillWidth: true }

                        RowLayout {
                            spacing: 8
                            Text {
                                text: "Auto-play when ready"
                                color: Theme.text
                                font.pixelSize: 13
                            }
                            ToggleSwitch {
                                id: autoPlayToggle
                                checked: root.autoPlay
                                onToggled: function(val) {
                                    root.autoPlay = val
                                }
                            }
                        }
                    }

                    // Action Buttons Row (Stream Now, Stop)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        // PRIMARY STREAM BUTTON
                        Rectangle {
                            Layout.fillWidth: true
                            height: 48
                            radius: 12
                            color: streamMouse.containsMouse ? Theme.accentHover : Theme.accent
                            opacity: (fluxTorrent && fluxTorrent.streamUrl.length > 0) ? 1.0 : 0.5

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 8

                                FluxIcon {
                                    name: "play"
                                    size: 16
                                    color: "#FFFFFF"
                                }

                                Text {
                                    text: (fluxTorrent && fluxTorrent.isReadyToPlay) ? "Stream Video Now (Ready)" : "Stream Video Now"
                                    color: "#FFFFFF"
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                }
                            }

                            MouseArea {
                                id: streamMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                enabled: fluxTorrent && fluxTorrent.streamUrl.length > 0
                                onClicked: {
                                    if (fluxTorrent && fluxTorrent.streamUrl.length > 0) {
                                        root.autoPlayTriggered = true
                                        var t = fluxTorrent.selectedFileName.length > 0 ? fluxTorrent.selectedFileName : fluxTorrent.torrentName
                                        root.playRequested(fluxTorrent.streamUrl, t)
                                    }
                                }
                            }
                        }

                        // Stop / Cancel Button
                        Rectangle {
                            implicitWidth: 120
                            height: 48
                            radius: 12
                            color: stopMouse.containsMouse ? Theme.surfaceHi : Theme.surface
                            border.width: 1
                            border.color: Theme.border

                            RowLayout {
                                anchors.centerIn: parent
                                spacing: 6

                                FluxIcon {
                                    name: "close"
                                    size: 14
                                    color: Theme.textDim
                                }

                                Text {
                                    text: "Stop"
                                    color: Theme.text
                                    font.pixelSize: 14
                                    font.weight: Font.DemiBold
                                }
                            }

                            MouseArea {
                                id: stopMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    if (fluxTorrent) fluxTorrent.stopTorrent()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
