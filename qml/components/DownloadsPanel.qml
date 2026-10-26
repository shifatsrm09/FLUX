import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Downloads popup (opened from the download button in the title bar).
// Shows active downloads and history with 1-click play, resume, and remove actions.
// Matches the FLUX Settings (Steam dark theme) design with square borders.
Popup {
    id: panel

    // Ask the app to open the Library page
    signal libraryRequested(string rel)
    // Ask the app to play a completed download file
    signal playRequested(string url, string title)

    // Distance from the window edge (matches the title bar position)
    property real edgeMargin: 16
    property real panelTopMargin: 44

    readonly property real maxHeight: (parent ? parent.height : 600) - panelTopMargin - 40
    readonly property int activeCount: fluxDownloads ? fluxDownloads.activeCount : 0
    readonly property int historyCount: (fluxDownloads && fluxDownloads.history) ? fluxDownloads.history.length : 0
    readonly property bool hasItems: activeCount > 0 || historyCount > 0

    // Steam Dark Theme Palette matching FLUX Settings
    readonly property color cSidebarBg: "#171a21"
    readonly property color cContentBg: "#1b1e24"
    readonly property color cBorder: "#2a2f3a"
    readonly property color cCardBg: "#21252d"
    readonly property color cCardBorder: "#2e3440"
    readonly property color cSteamBlue: "#1a9fff"
    readonly property color cTextWhite: "#F2F3F5"
    readonly property color cTextDim: "#949BA4"
    readonly property color cTextMute: "#80848E"
    readonly property color cButtonBg: "#242933"
    readonly property color cButtonBorder: "#3a4150"

    parent: Overlay.overlay
    x: parent ? Math.max(16, parent.width - width - edgeMargin) : 0
    y: panelTopMargin
    width: Math.min(460, (parent ? parent.width : 460) - 2 * edgeMargin)
    height: Math.min(Math.max(220, hasItems ? (62 + scrollContent.implicitHeight) : 240), maxHeight)
    modal: false
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Behavior on height { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 }
        NumberAnimation { property: "y"; from: panel.y - 8; to: panel.y; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 90 }
    }

    // Settings-style Square Border Background
    background: Rectangle {
        radius: 0
        color: panel.cContentBg
        border.width: 1
        border.color: panel.cBorder
    }

    // Settings-style Square Button Component
    component SquareButton: Rectangle {
        id: sbRoot
        property string text: ""
        property string iconName: ""
        property bool isAccent: false
        property bool isGhost: false
        property bool enabled: true
        signal clicked()

        opacity: sbRoot.enabled ? 1.0 : 0.45
        implicitHeight: 28
        implicitWidth: Math.max(64, sbLayout.implicitWidth + 16)
        radius: 0
        border.width: sbRoot.isGhost ? (sbMouse.containsMouse ? 1 : 0) : 1
        border.color: sbRoot.isAccent ? (sbMouse.containsMouse ? "#29a8ff" : panel.cSteamBlue)
                    : (sbMouse.containsMouse ? "#4e5666" : panel.cButtonBorder)
        color: sbRoot.isAccent ? (sbMouse.pressed ? "#1177cc" : sbMouse.containsMouse ? "#29a8ff" : panel.cSteamBlue)
             : sbRoot.isGhost ? (sbMouse.pressed ? "#171a21" : sbMouse.containsMouse ? "#242933" : "transparent")
             : (sbMouse.pressed ? "#1a2029" : sbMouse.containsMouse ? "#2e3542" : panel.cButtonBg)

        Behavior on color { ColorAnimation { duration: 100 } }
        Behavior on border.color { ColorAnimation { duration: 100 } }

        RowLayout {
            id: sbLayout
            anchors.centerIn: parent
            spacing: 6

            FluxIcon {
                visible: sbRoot.iconName !== ""
                name: sbRoot.iconName
                size: 13
                color: sbRoot.isAccent ? "#FFFFFF" : panel.cTextWhite
            }

            Text {
                text: sbRoot.text
                color: sbRoot.isGhost && !sbMouse.containsMouse ? panel.cTextDim : (sbRoot.isAccent ? "#FFFFFF" : panel.cTextWhite)
                font.family: "Segoe UI"
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }

        MouseArea {
            id: sbMouse
            anchors.fill: parent
            hoverEnabled: sbRoot.enabled
            cursorShape: sbRoot.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: if (sbRoot.enabled) sbRoot.clicked()
        }
    }

    // Settings-style Square Icon Button Component
    component SquareIconButton: Rectangle {
        id: sibRoot
        property string iconName: ""
        property int iconSize: 14
        property color iconColor: panel.cTextWhite
        property string tip: ""
        signal clicked()

        implicitWidth: 28
        implicitHeight: 28
        radius: 0
        border.width: 1
        border.color: sibMouse.containsMouse ? "#4e5666" : panel.cButtonBorder
        color: sibMouse.pressed ? "#1a2029" : sibMouse.containsMouse ? "#2e3542" : panel.cButtonBg

        Behavior on color { ColorAnimation { duration: 100 } }
        Behavior on border.color { ColorAnimation { duration: 100 } }

        FluxIcon {
            anchors.centerIn: parent
            name: sibRoot.iconName
            size: sibRoot.iconSize
            color: sibRoot.iconColor
        }

        FluxToolTip {
            visible: sibMouse.containsMouse && sibRoot.tip !== ""
            text: sibRoot.tip
        }

        MouseArea {
            id: sibMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: sibRoot.clicked()
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Header ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 16
            Layout.rightMargin: 12
            Layout.topMargin: 14
            Layout.bottomMargin: 12
            spacing: 8

            FluxIcon {
                name: "download"
                size: 18
                color: panel.cSteamBlue
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: "Downloads"
                color: panel.cTextWhite
                font.family: "Segoe UI"
                font.pixelSize: 15
                font.weight: Font.Bold
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                visible: panel.activeCount > 0
                implicitWidth: Math.max(18, countLabel.implicitWidth + 8)
                implicitHeight: 18
                radius: 0
                color: panel.cSteamBlue
                Layout.alignment: Qt.AlignVCenter

                Text {
                    id: countLabel
                    anchors.centerIn: parent
                    text: panel.activeCount
                    color: "#FFFFFF"
                    font.family: "Segoe UI"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }

            Item { Layout.fillWidth: true }

            SquareButton {
                visible: panel.historyCount > 0
                text: "Clear all"
                isGhost: true
                implicitHeight: 28
                onClicked: {
                    if (fluxDownloads) fluxDownloads.clearHistory()
                }
            }

            SquareIconButton {
                iconName: "close"
                iconSize: 14
                iconColor: panel.cTextDim
                implicitWidth: 28
                implicitHeight: 28
                tip: "Close"
                onClicked: panel.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: panel.cBorder
        }

        // ---- Content Area ----
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Empty state
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 60
                spacing: 8
                visible: !panel.hasItems

                FluxIcon {
                    name: "download"
                    size: 28
                    color: panel.cTextMute
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "No downloads yet"
                    color: panel.cTextWhite
                    font.family: "Segoe UI"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "Hover over a movie, episode or folder and click its download button."
                    color: panel.cTextMute
                    font.family: "Segoe UI"
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                SquareButton {
                    text: "Open Library"
                    implicitHeight: 32
                    implicitWidth: 120
                    enabled: !(fluxTeleparty && fluxTeleparty.active)
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 6
                    onClicked: panel.libraryRequested("")
                }
            }

            // Scrollable list when items exist
            Flickable {
                anchors.fill: parent
                clip: true
                visible: panel.hasItems
                contentWidth: width
                contentHeight: scrollContent.implicitHeight
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: FluxScrollBar { }

                ColumnLayout {
                    id: scrollContent
                    width: parent.width
                    spacing: 0

                    // ---- Active Downloads ----
                    Repeater {
                        model: fluxDownloads
                        delegate: Item {
                            id: row

                            required property int jobId
                            required property string jobTitle
                            required property string kind
                            required property string category
                            required property string status
                            required property real progress
                            required property string detail
                            required property bool isActive

                            width: scrollContent.width
                            height: row.isActive ? 76 : 0
                            visible: height > 0
                            opacity: row.isActive ? 1.0 : 0.0
                            clip: true

                            Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                            Behavior on opacity { NumberAnimation { duration: 150 } }

                            Rectangle {
                                anchors.fill: parent
                                color: rowHover.hovered ? panel.cCardBg : "transparent"
                            }

                            HoverHandler { id: rowHover }

                            RowLayout {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 16
                                anchors.rightMargin: 12
                                spacing: 12

                                // Kind badge
                                Rectangle {
                                    implicitWidth: 36
                                    implicitHeight: 36
                                    radius: 0
                                    color: panel.cSidebarBg
                                    border.width: 1
                                    border.color: panel.cCardBorder
                                    Layout.alignment: Qt.AlignVCenter

                                    FluxIcon {
                                        anchors.centerIn: parent
                                        name: row.kind === "pack" ? "folder" : "download"
                                        size: 16
                                        color: panel.cSteamBlue
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 3

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Text {
                                            text: row.jobTitle
                                            color: panel.cTextWhite
                                            font.family: "Segoe UI"
                                            font.pixelSize: 13
                                            font.weight: Font.DemiBold
                                            elide: Text.ElideMiddle
                                            Layout.fillWidth: true
                                        }

                                        Text {
                                            text: row.category
                                            color: panel.cTextMute
                                            font.family: "Segoe UI"
                                            font.pixelSize: 10
                                            font.weight: Font.Bold
                                            font.letterSpacing: 0.8
                                        }
                                    }

                                    Text {
                                        text: row.detail
                                        color: panel.cTextDim
                                        font.family: "Segoe UI"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }

                                    // Progress bar
                                    Rectangle {
                                        id: track

                                        Layout.fillWidth: true
                                        implicitHeight: 4
                                        radius: 0
                                        color: panel.cSidebarBg
                                        border.width: 1
                                        border.color: panel.cCardBorder

                                        Rectangle {
                                            width: row.status === "scanning" ? 0 : parent.width * Math.max(0, Math.min(1, row.progress))
                                            height: parent.height
                                            radius: 0
                                            color: row.status === "paused" ? panel.cTextDim : panel.cSteamBlue

                                            Behavior on width { NumberAnimation { duration: 180 } }
                                            Behavior on color { ColorAnimation { duration: 150 } }
                                        }

                                        // Indeterminate shimmer while scanning a folder
                                        Rectangle {
                                            visible: row.status === "scanning"
                                            width: parent.width * 0.3
                                            height: parent.height
                                            radius: 0
                                            color: panel.cSteamBlue

                                            SequentialAnimation on x {
                                                running: row.status === "scanning" && row.isActive
                                                loops: Animation.Infinite

                                                NumberAnimation { from: 0; to: track.width * 0.7; duration: 900; easing.type: Easing.InOutSine }
                                                NumberAnimation { from: track.width * 0.7; to: 0; duration: 900; easing.type: Easing.InOutSine }
                                            }
                                        }
                                    }
                                }

                                RowLayout {
                                    spacing: 4
                                    Layout.alignment: Qt.AlignVCenter

                                    SquareIconButton {
                                        iconName: row.status === "paused" ? "play" : "pause"
                                        iconSize: 13
                                        iconColor: panel.cTextWhite
                                        implicitWidth: 28
                                        implicitHeight: 28
                                        tip: row.status === "paused" ? "Resume" : "Pause"
                                        onClicked: fluxDownloads.togglePause(row.jobId)
                                    }

                                    SquareIconButton {
                                        iconName: "close"
                                        iconSize: 13
                                        iconColor: panel.cTextWhite
                                        implicitWidth: 28
                                        implicitHeight: 28
                                        tip: "Cancel"
                                        onClicked: fluxDownloads.cancel(row.jobId)
                                    }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 16
                                height: 1
                                color: panel.cBorder
                            }
                        }
                    }

                    // Section separator if active downloads exist above history
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 32
                        color: "transparent"
                        visible: panel.activeCount > 0 && panel.historyCount > 0

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            text: "Downloads & History"
                            color: panel.cSteamBlue
                            font.family: "Segoe UI"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            font.letterSpacing: 0.7
                        }
                    }

                    // ---- Completed & Unfinished History ----
                    Repeater {
                        model: fluxDownloads ? fluxDownloads.history : []
                        delegate: Item {
                            id: histItem
                            required property var modelData

                            readonly property bool isDone: modelData.status === "done"
                            readonly property bool isPack: modelData.kind === "pack" || modelData.isPack === true

                            function activateItem() {
                                if (histItem.isDone) {
                                    if (histItem.isPack) {
                                        var rel = modelData.libraryRel || (fluxDownloads ? fluxDownloads.offlineRelForPath(modelData.destination) : "")
                                        panel.libraryRequested(rel)
                                    } else {
                                        var fullPath = modelData.filePath || (modelData.destination + "/" + (modelData.fileName || ""))
                                        var url = modelData.fileUrl || ("file:///" + fullPath.split("\\").join("/"))
                                        panel.playRequested(url, modelData.title || "")
                                    }
                                } else {
                                    fluxDownloads.resumeHistory(modelData.key)
                                }
                            }

                            width: scrollContent.width
                            height: 62

                            Rectangle {
                                anchors.fill: parent
                                color: histHover.hovered ? panel.cCardBg : "transparent"
                            }

                            HoverHandler {
                                id: histHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            TapHandler {
                                acceptedButtons: Qt.LeftButton
                                onTapped: histItem.activateItem()
                            }

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 12
                                spacing: 12

                                Rectangle {
                                    implicitWidth: 36
                                    implicitHeight: 36
                                    radius: 0
                                    color: histItem.isDone ? "#163326" : panel.cSidebarBg
                                    border.width: 1
                                    border.color: histItem.isDone ? "#23A55A" : panel.cCardBorder
                                    Layout.alignment: Qt.AlignVCenter

                                    FluxIcon {
                                        anchors.centerIn: parent
                                        name: histItem.isPack ? "folder" : (histItem.isDone ? "check" : "download")
                                        size: 16
                                        color: histItem.isDone ? "#23A55A" : panel.cTextDim
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    spacing: 3

                                    Text {
                                        text: modelData.title || "Download"
                                        color: panel.cTextWhite
                                        font.family: "Segoe UI"
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideMiddle
                                        Layout.fillWidth: true
                                    }

                                    Text {
                                        text: {
                                            var st = (histItem.isDone ? "Completed" :
                                                      modelData.status === "interrupted" ? "Interrupted" :
                                                      modelData.status === "failed" ? "Failed" :
                                                      modelData.status === "cancelled" ? "Cancelled" : modelData.status)
                                            return st + (modelData.detail ? (" \u00B7 " + modelData.detail) : "")
                                        }
                                        color: histItem.isDone ? panel.cTextMute : panel.cTextDim
                                        font.family: "Segoe UI"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }

                                RowLayout {
                                    spacing: 4
                                    Layout.alignment: Qt.AlignVCenter

                                    // Action button: Play (if done) or Resume (if unfinished)
                                    SquareIconButton {
                                        iconName: "play"
                                        iconSize: 13
                                        iconColor: histItem.isDone ? "#23A55A" : panel.cTextWhite
                                        implicitWidth: 28
                                        implicitHeight: 28
                                        tip: histItem.isDone ? (histItem.isPack ? "Open folder in Library" : "Play") : "Resume download"
                                        onClicked: histItem.activateItem()
                                    }

                                    // Cross button: Remove from log / history
                                    SquareIconButton {
                                        iconName: "close"
                                        iconSize: 13
                                        iconColor: panel.cTextDim
                                        implicitWidth: 28
                                        implicitHeight: 28
                                        tip: "Remove from list"
                                        onClicked: fluxDownloads.removeHistory(modelData.key)
                                    }
                                }
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 16
                                height: 1
                                color: panel.cBorder
                            }
                        }
                    }
                }
            }
        }
    }
}
