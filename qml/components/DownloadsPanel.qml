import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Downloads popup (opened from the download button at the bottom-right).
// Shows only what is downloading right now (scanning, queued or in progress).
// Finished downloads live in the Library page.
Popup {
    id: panel

    // Ask the app to open the Library page
    signal libraryRequested()

    // Distance from the window edge (matches the dock)
    property real edgeMargin: 22
    property real bottomReserve: 82

    readonly property real maxHeight: (parent ? parent.height : 600) - bottomReserve - 70
    readonly property int activeCount: fluxDownloads ? fluxDownloads.activeCount : 0

    parent: Overlay.overlay
    x: parent ? parent.width - width - edgeMargin : 0
    y: parent ? parent.height - height - bottomReserve : 0
    width: Math.min(460, (parent ? parent.width : 460) - 2 * edgeMargin)
    height: Math.min(Math.max(240, 62 + list.contentHeight), maxHeight)
    modal: false
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 140 }
        NumberAnimation { property: "y"; from: panel.y + 12; to: panel.y; duration: 200; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 110 }
    }

    background: Rectangle {
        radius: 18
        color: Theme.bgRaised
        border.width: 1
        border.color: Theme.borderHi
    }

    contentItem: ColumnLayout {
        spacing: 0

        // ---- Header ----
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 12
            Layout.topMargin: 16
            Layout.bottomMargin: 12
            spacing: 8

            FluxIcon {
                name: "download"
                size: 20
                color: Theme.text
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: "Downloading"
                color: Theme.text
                font.pixelSize: 17
                font.weight: Font.Bold
                Layout.alignment: Qt.AlignVCenter
            }

            Rectangle {
                visible: panel.activeCount > 0
                implicitWidth: Math.max(20, countLabel.implicitWidth + 12)
                implicitHeight: 20
                radius: 10
                color: Theme.accent
                Layout.alignment: Qt.AlignVCenter

                Text {
                    id: countLabel
                    anchors.centerIn: parent
                    text: panel.activeCount
                    color: "#FFFFFF"
                    font.pixelSize: 11
                    font.weight: Font.Bold
                }
            }

            Item { Layout.fillWidth: true }

            IconButton {
                iconName: "close"
                iconSize: 16
                implicitWidth: 32
                implicitHeight: 32
                tip: "Close"
                tipAbove: false
                onClicked: panel.close()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        // ---- Active downloads ----
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ListView {
                id: list

                anchors.fill: parent
                clip: true
                model: fluxDownloads
                spacing: 0
                boundsBehavior: Flickable.StopAtBounds

                ScrollBar.vertical: FluxScrollBar { }

                displaced: Transition {
                    NumberAnimation { properties: "y"; duration: 200; easing.type: Easing.OutCubic }
                }

                // The model also holds finished / failed / cancelled jobs; they are collapsed
                // here so only live downloads are listed.
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

                    width: ListView.view.width
                    height: row.isActive ? 78 : 0
                    visible: height > 0
                    opacity: row.isActive ? 1.0 : 0.0
                    clip: true

                    Behavior on height { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                    Behavior on opacity { NumberAnimation { duration: 150 } }

                    Rectangle {
                        anchors.fill: parent
                        color: rowHover.hovered ? "#12FFFFFF" : "transparent"
                    }

                    HoverHandler { id: rowHover }

                    RowLayout {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 20
                        anchors.rightMargin: 12
                        spacing: 12

                        // Kind badge
                        Rectangle {
                            implicitWidth: 38
                            implicitHeight: 38
                            radius: 10
                            color: Theme.surfaceHi
                            Layout.alignment: Qt.AlignVCenter

                            FluxIcon {
                                anchors.centerIn: parent
                                name: row.kind === "pack" ? "folder" : "download"
                                size: 18
                                color: Theme.textDim
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
                                    color: Theme.text
                                    font.pixelSize: 13
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }

                                Text {
                                    text: row.category
                                    color: Theme.textMute
                                    font.pixelSize: 10
                                    font.weight: Font.Bold
                                    font.letterSpacing: 0.8
                                }
                            }

                            Text {
                                text: row.detail
                                color: Theme.textDim
                                font.pixelSize: 11
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            // Progress bar
                            Rectangle {
                                id: track

                                Layout.fillWidth: true
                                implicitHeight: 4
                                radius: 2
                                color: "#2EFFFFFF"

                                Rectangle {
                                    width: row.status === "scanning" ? 0 : parent.width * Math.max(0, Math.min(1, row.progress))
                                    height: parent.height
                                    radius: 2
                                    color: Theme.accent

                                    Behavior on width { NumberAnimation { duration: 180 } }
                                }

                                // Indeterminate shimmer while scanning a folder
                                Rectangle {
                                    visible: row.status === "scanning"
                                    width: parent.width * 0.3
                                    height: parent.height
                                    radius: 2
                                    color: Theme.accent

                                    SequentialAnimation on x {
                                        running: row.status === "scanning" && row.isActive
                                        loops: Animation.Infinite

                                        NumberAnimation { from: 0; to: track.width * 0.7; duration: 900; easing.type: Easing.InOutSine }
                                        NumberAnimation { from: track.width * 0.7; to: 0; duration: 900; easing.type: Easing.InOutSine }
                                    }
                                }
                            }
                        }

                        IconButton {
                            iconName: "close"
                            iconSize: 16
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Cancel"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.cancel(row.jobId)
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 20
                        height: 1
                        color: Theme.border
                    }
                }
            }

            // Empty state
            ColumnLayout {
                anchors.centerIn: parent
                width: parent.width - 60
                spacing: 8
                visible: panel.activeCount === 0

                FluxIcon {
                    name: "download"
                    size: 30
                    color: Theme.textMute
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "Nothing downloading"
                    color: Theme.text
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "Hover over a movie, episode or folder and click its download button. Finished downloads are in your Library."
                    color: Theme.textMute
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                FluxButton {
                    text: "Open Library"
                    variant: "secondary"
                    implicitHeight: 36
                    Layout.alignment: Qt.AlignHCenter
                    Layout.topMargin: 6
                    onClicked: panel.libraryRequested()
                }
            }
        }
    }
}
