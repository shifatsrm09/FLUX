import QtQuick
import "Theme.js" as Theme

// Vector icon set drawn on a 24x24 grid.
// Rendered at 2x and scaled down for crisp edges on HiDPI screens.
//
//   FluxIcon { name: "play"; size: 24; color: "#FFFFFF" }
//
// Names: play pause back chevronLeft chevronRight chevronDown chevronUp close
//        check search replay10 forward10 volume volumeLow volumeMuted subtitles
//        fullscreen fullscreenExit folder dots info home refresh alert
Item {
    id: root

    property string name: "play"
    property color color: "#FFFFFF"
    property real size: 24
    property real strokeWidth: 2.0

    width: size
    height: size

    onNameChanged: canvas.requestPaint()
    onColorChanged: canvas.requestPaint()
    onStrokeWidthChanged: canvas.requestPaint()

    // Build a path from a flat [x1, y1, x2, y2, ...] array
    function poly(ctx, pts, close) {
        ctx.beginPath()
        ctx.moveTo(pts[0], pts[1])
        for (var i = 2; i < pts.length; i += 2) {
            ctx.lineTo(pts[i], pts[i + 1])
        }
        if (close) ctx.closePath()
    }

    function drawIcon(ctx, n) {
        var PI = Math.PI

        switch (n) {
        case "play":
            poly(ctx, [7, 4.5, 19.5, 12, 7, 19.5], true)
            ctx.fill()
            ctx.lineWidth = 1.4
            ctx.stroke()
            break

        case "pause":
            ctx.beginPath()
            ctx.roundedRect(5.5, 4.5, 4.4, 15, 1.4, 1.4)
            ctx.fill()
            ctx.beginPath()
            ctx.roundedRect(14.1, 4.5, 4.4, 15, 1.4, 1.4)
            ctx.fill()
            break

        case "back":
            poly(ctx, [20, 12, 5, 12], false)
            ctx.stroke()
            poly(ctx, [11, 5.5, 4.5, 12, 11, 18.5], false)
            ctx.stroke()
            break

        case "chevronLeft":
            poly(ctx, [15, 5, 8, 12, 15, 19], false)
            ctx.stroke()
            break

        case "chevronRight":
            poly(ctx, [9, 5, 16, 12, 9, 19], false)
            ctx.stroke()
            break

        case "chevronDown":
            poly(ctx, [5.5, 9, 12, 15.5, 18.5, 9], false)
            ctx.stroke()
            break

        case "chevronUp":
            poly(ctx, [5.5, 15, 12, 8.5, 18.5, 15], false)
            ctx.stroke()
            break

        case "close":
            poly(ctx, [5.5, 5.5, 18.5, 18.5], false)
            ctx.stroke()
            poly(ctx, [18.5, 5.5, 5.5, 18.5], false)
            ctx.stroke()
            break

        case "check":
            poly(ctx, [4.5, 12.5, 9.5, 17.5, 19.5, 6.5], false)
            ctx.stroke()
            break

        case "search":
            ctx.beginPath()
            ctx.arc(10.5, 10.5, 6.5, 0, PI * 2, false)
            ctx.stroke()
            poly(ctx, [15.4, 15.4, 20.5, 20.5], false)
            ctx.stroke()
            break

        case "replay10":
        case "forward10":
            ctx.save()
            if (n === "replay10") {
                ctx.translate(24, 0)
                ctx.scale(-1, 1)
            }
            ctx.beginPath()
            ctx.arc(12, 13, 8, -50 * PI / 180, 270 * PI / 180, false)
            ctx.stroke()
            poly(ctx, [10.2, 2.4, 13.4, 5, 10.2, 7.6], false)
            ctx.stroke()
            ctx.restore()
            ctx.font = "bold 8px Segoe UI"
            ctx.textAlign = "center"
            ctx.textBaseline = "middle"
            ctx.fillText("10", 12, 13.4)
            break

        case "next":
            poly(ctx, [5.5, 5, 16, 12, 5.5, 19], true)
            ctx.fill()
            ctx.lineWidth = 1.4
            ctx.stroke()
            ctx.lineWidth = 2.6
            poly(ctx, [19.5, 5, 19.5, 19], false)
            ctx.stroke()
            break

        case "volume":
        case "volumeLow":
        case "volumeMuted":
            poly(ctx, [3.5, 9.5, 7.5, 9.5, 12.5, 5, 12.5, 19, 7.5, 14.5, 3.5, 14.5], true)
            ctx.fill()
            ctx.lineWidth = 1.2
            ctx.stroke()
            ctx.lineWidth = root.strokeWidth
            if (n === "volumeMuted") {
                poly(ctx, [16.5, 9.5, 21.5, 14.5], false)
                ctx.stroke()
                poly(ctx, [21.5, 9.5, 16.5, 14.5], false)
                ctx.stroke()
            } else {
                ctx.beginPath()
                ctx.arc(12.5, 12, 5, -0.8, 0.8, false)
                ctx.stroke()
                if (n === "volume") {
                    ctx.beginPath()
                    ctx.arc(12.5, 12, 9, -0.75, 0.75, false)
                    ctx.stroke()
                }
            }
            break

        case "subtitles":
            ctx.beginPath()
            ctx.roundedRect(3, 5, 18, 14, 3, 3)
            ctx.stroke()
            poly(ctx, [7, 10.5, 10.5, 10.5], false)
            ctx.stroke()
            poly(ctx, [13, 10.5, 17, 10.5], false)
            ctx.stroke()
            poly(ctx, [7, 14.5, 12, 14.5], false)
            ctx.stroke()
            poly(ctx, [14.5, 14.5, 17, 14.5], false)
            ctx.stroke()
            break

        case "fullscreen":
            poly(ctx, [4, 9, 4, 4, 9, 4], false)
            ctx.stroke()
            poly(ctx, [15, 4, 20, 4, 20, 9], false)
            ctx.stroke()
            poly(ctx, [20, 15, 20, 20, 15, 20], false)
            ctx.stroke()
            poly(ctx, [9, 20, 4, 20, 4, 15], false)
            ctx.stroke()
            break

        case "fullscreenExit":
            poly(ctx, [9, 4, 9, 9, 4, 9], false)
            ctx.stroke()
            poly(ctx, [15, 4, 15, 9, 20, 9], false)
            ctx.stroke()
            poly(ctx, [20, 15, 15, 15, 15, 20], false)
            ctx.stroke()
            poly(ctx, [4, 15, 9, 15, 9, 20], false)
            ctx.stroke()
            break

        case "folder":
            poly(ctx, [3, 6.5, 9.5, 6.5, 11.5, 9, 21, 9, 21, 18.5, 3, 18.5], true)
            ctx.stroke()
            break

        case "dots":
            ctx.beginPath()
            ctx.arc(5, 12, 1.7, 0, PI * 2, false)
            ctx.fill()
            ctx.beginPath()
            ctx.arc(12, 12, 1.7, 0, PI * 2, false)
            ctx.fill()
            ctx.beginPath()
            ctx.arc(19, 12, 1.7, 0, PI * 2, false)
            ctx.fill()
            break

        case "info":
            ctx.beginPath()
            ctx.arc(12, 12, 9, 0, PI * 2, false)
            ctx.stroke()
            poly(ctx, [12, 11, 12, 16.5], false)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(12, 7.8, 1, 0, PI * 2, false)
            ctx.fill()
            break

        case "home":
            poly(ctx, [3.5, 11, 12, 4, 20.5, 11], false)
            ctx.stroke()
            poly(ctx, [6, 9.5, 6, 19.5, 18, 19.5, 18, 9.5], false)
            ctx.stroke()
            break

        case "refresh":
            ctx.beginPath()
            ctx.arc(12, 12, 7.5, -50 * PI / 180, 270 * PI / 180, false)
            ctx.stroke()
            poly(ctx, [10, 1.8, 13.4, 4.5, 10, 7.2], false)
            ctx.stroke()
            break

        case "alert":
            poly(ctx, [12, 3.5, 21.5, 20, 2.5, 20], true)
            ctx.stroke()
            poly(ctx, [12, 9.5, 12, 14], false)
            ctx.stroke()
            ctx.beginPath()
            ctx.arc(12, 17, 0.9, 0, PI * 2, false)
            ctx.fill()
            break
        }
    }

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
            ctx.save()

            var s = width / 24
            ctx.scale(s, s)

            var col = Theme.css(root.color)
            ctx.fillStyle = col
            ctx.strokeStyle = col
            ctx.lineWidth = root.strokeWidth
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            root.drawIcon(ctx, root.name)
            ctx.restore()
        }
    }
}
