import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

// Download manager popup (opened from the download button at the bottom-right).
Popup {
    id: panel

    // Distance from the window edge (matches the dock)
    property real edgeMargin: 22
    property real bottomReserve: 82

    parent: Overlay.overlay
    x: parent ? parent.width - width - edgeMargin : 0
    y: parent ? parent.height - height - bottomReserve : 0
    width: Math.min(440, (parent ? parent.width : 440) - 2 * edgeMargin)
    height: Math.min(Math.max(240, 132 + list.contentHeight),
                     (parent ? parent.height : 600) - bottomReserve - 70)
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
            Layout.bottomMargin: 10
            spacing: 8

            FluxIcon {
                name: "download"
                size: 20
                color: Theme.text
                Layout.alignment: Qt.AlignVCenter
            }

            Text {
                text: "Downloads"
                color: Theme.text
                font.pixelSize: 17
                font.weight: Font.Bold
                Layout.fillWidth: true
            }

            Text {
                visible: !!fluxDownloads && fluxDownloads.activeCount > 0
                text: fluxDownloads ? (fluxDownloads.activeCount + " active") : ""
                color: Theme.textDim
                font.pixelSize: 12
            }

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

        // ---- Jobs ----
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

                add: Transition {
                    NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 }
                }
                displaced: Transition {
                    NumberAnimation { properties: "y"; duration: 200; easing.type: Easing.OutCubic }
                }

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

                    readonly property bool isFailed: status === "failed"
                    readonly property bool isDone: status === "done"
                    readonly property bool canRetry: status === "failed" || status === "cancelled"

                    width: ListView.view.width
                    height: 78

                    Rectangle {
                        anchors.fill: parent
                        color: rowHover.hovered ? "#12FFFFFF" : "transparent"
                    }

                    HoverHandler { id: rowHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 12
                        spacing: 12

                        // Kind badge
                        Rectangle {
                            implicitWidth: 38
                            implicitHeight: 38
                            radius: 10
                            color: row.isFailed ? "#26FF5D5D" : (row.isDone ? "#2646D369" : Theme.surfaceHi)
                            Layout.alignment: Qt.AlignVCenter

                            FluxIcon {
                                anchors.centerIn: parent
                                name: row.isFailed ? "alert" : (row.isDone ? "check" : (row.kind === "pack" ? "folder" : "download"))
                                size: 18
                                color: row.isFailed ? Theme.danger : (row.isDone ? Theme.success : Theme.textDim)
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
                                color: row.isFailed ? Theme.danger : Theme.textDim
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
                                visible: row.status !== "cancelled"

                                Rectangle {
                                    width: row.status === "scanning" ? 0 : parent.width * Math.max(0, Math.min(1, row.progress))
                                    height: parent.height
                                    radius: 2
                                    color: row.isFailed ? Theme.danger : (row.isDone ? Theme.success : Theme.accent)

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
                                        running: row.status === "scanning"
                                        loops: Animation.Infinite

                                        NumberAnimation { from: 0; to: track.width * 0.7; duration: 900; easing.type: Easing.InOutSine }
                                        NumberAnimation { from: track.width * 0.7; to: 0; duration: 900; easing.type: Easing.InOutSine }
                                    }
                                }
                            }
                        }

                        // Actions
                        IconButton {
                            visible: row.isActive
                            iconName: "close"
                            iconSize: 16
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Cancel"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.cancel(row.jobId)
                        }

                        IconButton {
                            visible: row.canRetry
                            iconName: "refresh"
                            iconSize: 16
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Retry (resumes where it stopped)"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.retry(row.jobId)
                        }

                        IconButton {
                            visible: row.isDone
                            iconName: "folder"
                            iconSize: 16
                            implicitWidth: 34
                            implicitHeight: 34
                            tip: "Show in folder"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.openFolder(row.jobId)
                        }

                        IconButton {
                            visible: !row.isActive
                            iconName: "close"
                            iconSize: 14
                            iconColor: Theme.textDim
                            implicitWidth: 30
                            implicitHeight: 30
                            tip: "Remove from list"
                            tipAbove: false
                            Layout.alignment: Qt.AlignVCenter
                            onClicked: fluxDownloads.remove(row.jobId)
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

                // Empty state
                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 60
                    spacing: 8
                    visible: list.count === 0

                    FluxIcon {
                        name: "download"
                        size: 30
                        color: Theme.textMute
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "No downloads yet"
                        color: Theme.text
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignHCenter
                    }

                    Text {
                        text: "Hover over a movie, episode or folder and click its download button."
                        color: Theme.textMute
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Theme.border
        }

        // ---- Footer ----
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 14
            spacing: 8

            FluxButton {
                text: "Open downloads folder"
                variant: "secondary"
                implicitHeight: 38
                onClicked: if (fluxDownloads) fluxDownloads.openRoot()
            }

            Item { Layout.fillWidth: true }

            FluxButton {
                text: "Clear finished"
                variant: "ghost"
                implicitHeight: 38
                visible: !!fluxDownloads && fluxDownloads.hasFinished
                onClicked: fluxDownloads.clearFinished()
            }
        }
    }
}
