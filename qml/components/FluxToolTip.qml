import QtQuick
import QtQuick.Controls
import "Theme.js" as Theme

// Dark, rounded tooltip. Declare it as a child of the item it describes:
//   FluxToolTip { visible: mouse.containsMouse; text: "Hello"; above: true }
ToolTip {
    id: tip

    property bool above: false

    delay: 400
    leftPadding: 10
    rightPadding: 10
    topPadding: 6
    bottomPadding: 6

    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? (above ? -height - 8 : parent.height + 8) : 0

    contentItem: Text {
        text: tip.text
        color: Theme.text
        font.pixelSize: 12
    }

    background: Rectangle {
        radius: 8
        color: Theme.surfaceTop
        border.width: 1
        border.color: Theme.borderHi
    }
}
