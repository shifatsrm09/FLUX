import QtQuick
import "Theme.js" as Theme

// Circular loading ring with a rotating red arc.
Item {
    id: root

    property real size: 56
    property color color: Theme.accent
    property color trackColor: "#26FFFFFF"
    property real thickness: 4
    property bool running: true

    width: size
    height: size

    onColorChanged: canvas.requestPaint()
    onTrackColorChanged: canvas.requestPaint()
    onThicknessChanged: canvas.requestPaint()

    Canvas {
        id: canvas

        width: root.size * 2
        height: root.size * 2
        anchors.centerIn: parent
        scale: 0.5
        renderTarget: Canvas.Image
        renderStrategy: Canvas.Cooperative

        onWidthChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)

            var c = width / 2
            var t = root.thickness * 2
            var r = c - t / 2 - 2

            ctx.lineWidth = t
            ctx.lineCap = "round"

            ctx.strokeStyle = Theme.css(root.trackColor)
            ctx.beginPath()
            ctx.arc(c, c, r, 0, Math.PI * 2, false)
            ctx.stroke()

            ctx.strokeStyle = Theme.css(root.color)
            ctx.beginPath()
            ctx.arc(c, c, r, -Math.PI / 2, -Math.PI / 2 + Math.PI * 1.05, false)
            ctx.stroke()
        }

        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 950
            loops: Animation.Infinite
            running: root.running && root.visible
        }
    }
}
