import QtQuick
import QtQuick.Window

Item {
    id: root

    property var window: null
    property int resizeMargin: 6

    anchors.fill: parent
    z: 9999
    visible: window && window.visibility !== Window.Maximized && window.visibility !== Window.FullScreen

    // Top edge
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.resizeMargin
        anchors.rightMargin: root.resizeMargin
        height: root.resizeMargin
        cursorShape: Qt.SizeVerCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.TopEdge)
            }
        }
    }

    // Bottom edge
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: root.resizeMargin
        anchors.rightMargin: root.resizeMargin
        height: root.resizeMargin
        cursorShape: Qt.SizeVerCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.BottomEdge)
            }
        }
    }

    // Left edge
    MouseArea {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.resizeMargin
        anchors.bottomMargin: root.resizeMargin
        width: root.resizeMargin
        cursorShape: Qt.SizeHorCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.LeftEdge)
            }
        }
    }

    // Right edge
    MouseArea {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: root.resizeMargin
        anchors.bottomMargin: root.resizeMargin
        width: root.resizeMargin
        cursorShape: Qt.SizeHorCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.RightEdge)
            }
        }
    }

    // Top-Left corner
    MouseArea {
        anchors.top: parent.top
        anchors.left: parent.left
        width: root.resizeMargin
        height: root.resizeMargin
        cursorShape: Qt.SizeFDiagCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
            }
        }
    }

    // Top-Right corner
    MouseArea {
        anchors.top: parent.top
        anchors.right: parent.right
        width: root.resizeMargin
        height: root.resizeMargin
        cursorShape: Qt.SizeBDiagCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.TopEdge | Qt.RightEdge)
            }
        }
    }

    // Bottom-Left corner
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: root.resizeMargin
        height: root.resizeMargin
        cursorShape: Qt.SizeBDiagCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
            }
        }
    }

    // Bottom-Right corner
    MouseArea {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: root.resizeMargin
        height: root.resizeMargin
        cursorShape: Qt.SizeFDiagCursor
        acceptedButtons: Qt.LeftButton
        onPressed: function(mouse) {
            if (mouse.button === Qt.LeftButton && root.window) {
                root.window.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
            }
        }
    }
}
