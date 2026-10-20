import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Right-click menu for a file / folder card:  Open  |  Bookmark / Remove bookmark  |  Download
Menu {
    id: menu

    property bool isFolder: false
    property bool bookmarked: false
    property bool canBookmark: true
    property bool canDownload: true
    property bool canShowInFolder: false
    property bool canPlayInTeleparty: false

    signal openRequested()
    signal bookmarkToggled()
    signal downloadRequested()
    signal showInFolderRequested()
    signal playInTelepartyRequested()

    padding: 6
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 110 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 80 }
    }

    // One row of the menu
    component Entry: MenuItem {
        id: entry

        property color tint: Theme.text

        implicitWidth: 224
        implicitHeight: visible ? 40 : 0
        height: visible ? 40 : 0
        leftPadding: 12
        rightPadding: 12

        contentItem: Text {
            text: entry.text
            color: entry.enabled ? entry.tint : Theme.textMute
            font.pixelSize: 13
            font.weight: Font.Medium
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8
            color: entry.highlighted ? "#26FFFFFF" : "transparent"
        }
    }

    background: Rectangle {
        implicitWidth: 236
        radius: 12
        color: "#F2121216"
        border.width: 1
        border.color: Theme.borderHi
    }

    Entry {
        text: "Open"
        onTriggered: menu.openRequested()
    }

    Entry {
        visible: menu.canPlayInTeleparty
        text: "Play in TELEPARTY"
        tint: "#FF6B75"
        onTriggered: menu.playInTelepartyRequested()
    }

    Entry {
        visible: menu.canBookmark
        text: menu.bookmarked ? "Remove bookmark" : "Bookmark"
        tint: menu.bookmarked ? Theme.danger : Theme.text
        onTriggered: menu.bookmarkToggled()
    }

    Entry {
        visible: menu.canShowInFolder
        text: "Show in folder"
        onTriggered: menu.showInFolderRequested()
    }

    Entry {
        visible: menu.canDownload
        text: menu.isFolder ? "Download folder" : "Download"
        enabled: menu.canDownload
        onTriggered: menu.downloadRequested()
    }
}
