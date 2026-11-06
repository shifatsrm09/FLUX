import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Right-click menu for a file / folder card:
// In Library: Open | Bookmark | Copy | Paste (if folder) | Rename | Delete | Show in folder
// Outside Library (Search/Online): Open | Bookmark | Download
Menu {
    id: menu

    property bool isFolder: false
    property bool bookmarked: false
    property bool canBookmark: true
    property bool canDownload: true
    property bool canShowInFolder: false
    property bool isLibraryItem: false
    property bool canPaste: false
    property string sizeText: ""

    signal openRequested()
    signal bookmarkToggled()
    signal downloadRequested()
    signal showInFolderRequested()
    signal copyRequested()
    signal pasteRequested()
    signal renameRequested()
    signal deleteRequested()

    padding: 0
    topPadding: 3
    bottomPadding: 3
    leftPadding: 0
    rightPadding: 0
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: 100 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 80 }
    }

    // One row of the menu
    component Entry: MenuItem {
        id: entry

        property color tint: Theme.text

        implicitWidth: 238
        implicitHeight: visible ? 31 : 0
        height: visible ? 31 : 0
        leftPadding: 18
        rightPadding: 18

        contentItem: Text {
            text: entry.text
            color: entry.enabled ? (entry.highlighted ? "#FFFFFF" : (entry.tint !== Theme.text ? entry.tint : "#DCDEDF")) : "#67707B"
            font.pixelSize: 13
            font.weight: Font.Normal
            font.family: Theme.fontFamily
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 0
            color: entry.highlighted ? "#212B3B" : "transparent"
        }
    }

    component Sep: Item {
        implicitWidth: 238
        implicitHeight: visible ? 7 : 0
        height: visible ? 7 : 0
        Rectangle {
            anchors.centerIn: parent
            width: parent.width
            height: 1
            color: "#67707B"
        }
    }

    background: Rectangle {
        implicitWidth: 238
        radius: 2
        color: "#3D4450"
        border.width: 1
        border.color: "#171D25"
    }

    Entry {
        text: "Open"
        onTriggered: menu.openRequested()
    }

    Entry {
        visible: menu.canBookmark
        text: menu.bookmarked ? "Remove bookmark" : "Bookmark"
        tint: menu.bookmarked ? Theme.danger : Theme.text
        onTriggered: menu.bookmarkToggled()
    }

    Sep {
        visible: menu.isLibraryItem
    }

    Entry {
        visible: menu.isLibraryItem
        text: "Copy"
        onTriggered: menu.copyRequested()
    }

    Entry {
        visible: menu.isLibraryItem && menu.isFolder
        text: "Paste"
        enabled: menu.canPaste
        onTriggered: menu.pasteRequested()
    }

    Entry {
        visible: menu.isLibraryItem
        text: "Rename"
        onTriggered: menu.renameRequested()
    }

    Entry {
        visible: menu.isLibraryItem
        text: "Delete"
        tint: Theme.danger
        onTriggered: menu.deleteRequested()
    }

    Sep {
        visible: menu.canShowInFolder || menu.canDownload
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

    Sep {
        visible: menu.sizeText.length > 0
    }

    Entry {
        visible: menu.sizeText.length > 0
        text: "Size: " + menu.sizeText
        enabled: false
    }
}