import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Theme.js" as Theme

Popup {
    id: dialog

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    width: Math.min(440, (Overlay.overlay ? Overlay.overlay.width : 440) - 36)
    modal: true
    focus: true
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    Overlay.modal: Rectangle {
        color: "#CC000000"
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 180 }
        NumberAnimation { property: "scale"; from: 0.94; to: 1; duration: 220; easing.type: Easing.OutBack }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 140 }
    }

    background: Rectangle {
        radius: 20
        color: Theme.bgRaised
        border.width: 1
        border.color: Theme.borderHi
    }

    contentItem: ColumnLayout {
        spacing: 0

        ColumnLayout {
            Layout.fillWidth: true
            Layout.margins: 28
            Layout.bottomMargin: 24
            spacing: 16

            // Warning Icon Badge
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                implicitWidth: 64
                implicitHeight: 64
                radius: 32
                color: "#26FF9D00"
                border.width: 1
                border.color: "#66FF9D00"

                FluxIcon {
                    anchors.centerIn: parent
                    name: "alert"
                    size: 30
                    color: "#FFAA00"
                }
            }

            // Title
            Text {
                text: "Network Notice"
                color: Theme.text
                font.pixelSize: 20
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                Layout.fillWidth: true
            }

            // Description
            Text {
                text: "You cannot stream any media.\nConnect to a BDIX supported Wi-Fi to access content."
                color: Theme.textDim
                font.pixelSize: 14
                lineHeight: 1.45
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Item { implicitHeight: 6 }

            // Action Buttons
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 10

                FluxButton {
                    text: "Explore FLUX"
                    variant: "primary"
                    implicitHeight: 42
                    Layout.fillWidth: true
                    onClicked: dialog.close()
                }

                FluxButton {
                    text: "Retry Connection"
                    variant: "ghost"
                    implicitHeight: 36
                    Layout.fillWidth: true
                    onClicked: {
                        if (fluxUser) fluxUser.autoDetectServer()
                        dialog.close()
                    }
                }
            }
        }
    }
}
