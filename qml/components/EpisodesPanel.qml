import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme
import "MediaFormatter.js" as Formatter

Item {
    id: root

    property bool opened: false
    property var episodes: (fluxBrowser ? fluxBrowser.currentEpisodes : [])
    property int currentIndex: (fluxBrowser ? fluxBrowser.currentEpisodeIndex : -1)
    property string seasonName: (fluxBrowser ? fluxBrowser.currentSeasonName : "")

    signal episodeSelected(string url, string name)
    signal closeRequested()

    anchors.fill: parent
    visible: opacity > 0.0
    opacity: root.opened ? 1.0 : 0.0

    Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

    // Click outside drawer to close
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        onClicked: root.closeRequested()
        onWheel: function(wheel) { wheel.accepted = true }
    }

    // Semi-transparent backdrop scrim on the left
    Rectangle {
        anchors.fill: parent
        color: "#66000000"
        opacity: root.opened ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 220 } }
    }

    // Sliding Drawer
    Rectangle {
        id: drawer

        width: Math.min(420, Math.max(320, root.width * 0.38))
        height: root.height
        y: 0
        x: root.opened ? (root.width - width) : root.width

        Behavior on x { NumberAnimation { duration: 280; easing.type: Easing.OutCubic } }

        color: "#F01E1F22" // Discord dark theme background
        border.width: 1
        border.color: "#35373C"

        // Prevent clicks and wheel scrolls in the drawer from bubbling to the player underneath
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onWheel: function(wheel) { wheel.accepted = true }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.bottomMargin: 20
            anchors.topMargin: (window && !window.isFullscreen) ? 50 : 20
            spacing: 16

            // Header Row: close button on the left to avoid colliding with window titlebar controls
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                IconButton {
                    iconName: "close"
                    iconSize: 20
                    tip: "Close (Esc)"
                    hoverColor: "#33FFFFFF"
                    onClicked: root.closeRequested()
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3

                    Text {
                        Layout.fillWidth: true
                        text: root.seasonName.length > 0 ? root.seasonName : "Episodes"
                        color: "#FFFFFF"
                        font.pixelSize: 17
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }

                    Text {
                        text: (root.episodes ? root.episodes.length : 0) + " Episodes"
                        color: Theme.textDim
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: "#35373C"
            }

            // Episode List
            ListView {
                id: epList
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 8
                model: root.episodes
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: FluxScrollBar { }

                onModelChanged: {
                    if (root.opened && root.currentIndex >= 0) {
                        epList.positionViewAtIndex(root.currentIndex, ListView.Center)
                    }
                }

                Connections {
                    target: root
                    function onOpenedChanged() {
                        if (root.opened && root.currentIndex >= 0) {
                            epList.positionViewAtIndex(root.currentIndex, ListView.Center)
                        }
                    }
                    function onCurrentIndexChanged() {
                        if (root.opened && root.currentIndex >= 0) {
                            epList.positionViewAtIndex(root.currentIndex, ListView.Center)
                        }
                    }
                }

                delegate: Rectangle {
                    id: epItem

                    required property var modelData
                    required property int index

                    readonly property bool isCurrent: modelData.isCurrent === true
                    readonly property bool isUpNext: !isCurrent && (index === root.currentIndex + 1)
                    readonly property var desc: Formatter.describe(modelData.name, false, modelData.size || "")
                    readonly property var prog: (fluxUser && modelData.url) ? fluxUser.progressFor(modelData.url) : ({})
                    readonly property bool isWatched: prog.watched === true

                    width: epList.width
                    height: 64
                    radius: 10
                    color: epMouse.containsMouse
                           ? (isCurrent ? "#335865F2" : "#1FFFFFFF")
                           : (isCurrent ? "#265865F2" : "#10FFFFFF")
                    border.width: isCurrent ? 1.5 : (epMouse.containsMouse ? 1 : 0)
                    border.color: isCurrent ? Theme.accentHover : "#2EFFFFFF"

                    Behavior on color { ColorAnimation { duration: 150 } }
                    Behavior on border.color { ColorAnimation { duration: 150 } }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 14
                        spacing: 12

                        // Episode number or equalizer icon
                        Rectangle {
                            Layout.preferredWidth: 38
                            Layout.preferredHeight: 38
                            radius: 8
                            color: epItem.isCurrent ? Theme.accent : (epItem.isUpNext ? "#265865F2" : "#14FFFFFF")

                            Equalizer {
                                anchors.centerIn: parent
                                running: epItem.isCurrent && (!!fluxPlayer && fluxPlayer.isPlaying)
                                color: "#FFFFFF"
                                visible: epItem.isCurrent
                            }

                            Text {
                                anchors.centerIn: parent
                                visible: !epItem.isCurrent
                                text: (epItem.index + 1 < 10 ? "0" : "") + (epItem.index + 1)
                                color: epItem.isUpNext ? Theme.accentHover : "#B3FFFFFF"
                                font.pixelSize: 13
                                font.weight: Font.Bold
                            }
                        }

                        // Title and info
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Text {
                                Layout.fillWidth: true
                                text: epItem.desc.title.length > 0 ? epItem.desc.title : epItem.modelData.name
                                color: epItem.isCurrent ? Theme.accentHover : "#FFFFFF"
                                font.pixelSize: 13
                                font.weight: epItem.isCurrent ? Font.Bold : Font.DemiBold
                                elide: Text.ElideRight
                            }

                            RowLayout {
                                spacing: 8

                                // "NOW PLAYING" tag
                                Rectangle {
                                    visible: epItem.isCurrent
                                    implicitHeight: 18
                                    implicitWidth: nowPlayingText.implicitWidth + 10
                                    radius: 4
                                    color: Theme.accent

                                    Text {
                                        id: nowPlayingText
                                        anchors.centerIn: parent
                                        text: "NOW PLAYING"
                                        color: "#FFFFFF"
                                        font.pixelSize: 9
                                        font.weight: Font.Bold
                                    }
                                }

                                // "UP NEXT" tag
                                Rectangle {
                                    visible: epItem.isUpNext
                                    implicitHeight: 18
                                    implicitWidth: upNextText.implicitWidth + 10
                                    radius: 4
                                    color: "#335865F2"

                                    Text {
                                        id: upNextText
                                        anchors.centerIn: parent
                                        text: "UP NEXT"
                                        color: Theme.accentHover
                                        font.pixelSize: 9
                                        font.weight: Font.Bold
                                    }
                                }

                                Text {
                                    visible: !epItem.isCurrent && epItem.desc.episode.length > 0
                                    text: epItem.desc.episode
                                    color: Theme.textDim
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }

                                Text {
                                    visible: epItem.modelData.size && epItem.modelData.size.length > 0
                                    text: epItem.modelData.size
                                    color: Theme.textMute
                                    font.pixelSize: 11
                                }

                                Text {
                                    visible: epItem.isWatched && !epItem.isCurrent
                                    text: "\u2713 Watched"
                                    color: Theme.success
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                }
                            }
                        }

                        // Hover play button
                        Rectangle {
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            radius: 14
                            color: "#FFFFFF"
                            visible: epMouse.containsMouse && !epItem.isCurrent
                            scale: epMouse.containsMouse ? 1.0 : 0.8
                            Behavior on scale { NumberAnimation { duration: 150 } }

                            FluxIcon {
                                anchors.centerIn: parent
                                anchors.horizontalCenterOffset: 1
                                name: "play"
                                size: 14
                                color: "#1E1F22"
                            }
                        }
                    }

                    MouseArea {
                        id: epMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.episodeSelected(epItem.modelData.url, epItem.modelData.name)
                        }
                    }
                }
            }

            // Empty state if no episodes found
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !root.episodes || root.episodes.length === 0

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: 10

                    FluxIcon {
                        Layout.alignment: Qt.AlignHCenter
                        name: "folder"
                        size: 40
                        color: "#40FFFFFF"
                    }

                    Text {
                        Layout.alignment: Qt.AlignHCenter
                        text: "No other episodes in this folder"
                        color: Theme.textDim
                        font.pixelSize: 13
                    }
                }
            }
        }
    }
}
