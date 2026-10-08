import QtQuick
import QtQuick.Controls

// Slim, auto-hiding scrollbar.
//   ScrollBar.vertical: FluxScrollBar { }
ScrollBar {
    id: bar

    policy: ScrollBar.AsNeeded
    minimumSize: 0.08
    padding: 2

    contentItem: Rectangle {
        implicitWidth: 6
        implicitHeight: 6
        radius: 3
        color: bar.pressed ? "#99FFFFFF" : (bar.hovered ? "#66FFFFFF" : "#40FFFFFF")
        opacity: (bar.policy === ScrollBar.AlwaysOn || bar.active) ? 1.0 : 0.0

        Behavior on opacity { NumberAnimation { duration: 200 } }
    }

    background: Item { }
}
