import QtQuick
import "Theme.js" as Theme

// Typographic "poster" artwork. There is no image data in the library, so each title gets a
// stable, hand-composed layout (one of six) built from simple shapes + a giant outlined
// year / episode / initial. Drawn on top of the card's palette gradient.
Item {
    id: root

    property string seed: ""
    property string bigText: ""
    property bool isFolder: false
    property bool hovered: false

    readonly property int composition: Theme.posterStyle(root.seed)

    // Everything drifts slightly on hover for a subtle parallax
    Item {
        id: art

        anchors.fill: parent
        transformOrigin: Item.Center
        scale: root.hovered ? 1.06 : 1.0
        x: root.hovered ? -4 : 0

        Behavior on scale { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }
        Behavior on x { NumberAnimation { duration: 500; easing.type: Easing.OutCubic } }

        // ---- 0: Bokeh ----------------------------------------------------------------
        Item {
            visible: !root.isFolder && root.composition === 0
            anchors.fill: parent

            Rectangle {
                width: root.height * 1.15
                height: width
                radius: width / 2
                x: root.width * 0.52
                y: -root.height * 0.3
                color: "#0DFFFFFF"
            }

            Rectangle {
                width: root.height * 0.75
                height: width
                radius: width / 2
                x: root.width * 0.68
                y: root.height * 0.42
                color: "#1AE50914"
            }

            Rectangle {
                width: root.height * 0.34
                height: width
                radius: width / 2
                x: root.width * 0.40
                y: root.height * 0.1
                color: "#0AFFFFFF"
            }
        }

        // ---- 1: Diagonal stripes -----------------------------------------------------
        Item {
            visible: !root.isFolder && root.composition === 1
            anchors.fill: parent

            Repeater {
                model: (!root.isFolder && root.composition === 1) ? 7 : 0

                delegate: Rectangle {
                    required property int index

                    width: root.height * 2.6
                    height: (index % 2 === 0) ? 10 : 4
                    rotation: -32
                    x: root.width * 0.72 + (index - 3) * 12.7 - width / 2
                    y: root.height * 0.5 + (index - 3) * 20.4 - height / 2
                    color: "#0DFFFFFF"
                }
            }
        }

        // ---- 2: Concentric rings -----------------------------------------------------
        Item {
            visible: !root.isFolder && root.composition === 2
            anchors.fill: parent

            Repeater {
                model: (!root.isFolder && root.composition === 2) ? 4 : 0

                delegate: Rectangle {
                    required property int index

                    width: root.height * (0.35 + index * 0.3)
                    height: width
                    radius: width / 2
                    x: root.width * 0.80 - width / 2
                    y: root.height * 0.42 - height / 2
                    color: "transparent"
                    border.width: 1
                    border.color: index === 0 ? "#33E50914" : "#16FFFFFF"
                }
            }
        }

        // ---- 3: Horizon / sliced sun -------------------------------------------------
        Item {
            visible: !root.isFolder && root.composition === 3
            anchors.fill: parent

            Rectangle {
                id: sun
                width: root.height * 0.66
                height: width
                radius: width / 2
                x: root.width * 0.72 - width / 2
                y: root.height * 0.42 - height / 2
                color: "#18FFFFFF"
            }

            Repeater {
                model: (!root.isFolder && root.composition === 3) ? 4 : 0

                delegate: Rectangle {
                    required property int index

                    x: sun.x - 2
                    width: sun.width + 4
                    height: 2 + index * 2
                    y: sun.y + sun.height * (0.5 + index * 0.13)
                    color: Theme.posterBottom(root.seed)
                }
            }
        }

        // ---- 4: Monolith -------------------------------------------------------------
        Item {
            visible: !root.isFolder && root.composition === 4
            anchors.fill: parent

            Rectangle {
                x: root.width * 0.70
                width: root.width * 0.18
                height: root.height
                color: "#0AFFFFFF"
            }

            Rectangle {
                x: root.width * 0.70
                width: 2
                height: root.height
                color: "#4DE50914"
            }

            Rectangle {
                x: root.width * 0.52
                width: root.width * 0.05
                height: root.height
                color: "#06FFFFFF"
            }
        }

        // ---- 5: Dot grid -------------------------------------------------------------
        Item {
            visible: !root.isFolder && root.composition === 5
            anchors.fill: parent

            Repeater {
                model: (!root.isFolder && root.composition === 5) ? 30 : 0

                delegate: Rectangle {
                    required property int index

                    width: 4
                    height: 4
                    radius: 2
                    x: root.width * 0.58 + (index % 6) * 17
                    y: 18 + Math.floor(index / 6) * 17
                    color: ((index % 7) === 0) ? "#59E50914" : "#1AFFFFFF"
                }
            }
        }

        // ---- Giant outlined year / episode / initial ----------------------------------
        Text {
            visible: !root.isFolder && root.bigText.length > 0
            anchors.right: parent.right
            anchors.rightMargin: 14
            anchors.verticalCenter: parent.verticalCenter
            anchors.verticalCenterOffset: -root.height * 0.1
            text: root.bigText
            color: "#0DFFFFFF"
            style: Text.Outline
            styleColor: "#2EFFFFFF"
            font.weight: Font.Black
            font.pixelSize: Math.round(root.height * (root.bigText.length <= 1 ? 0.95
                                                      : root.bigText.length <= 2 ? 0.78
                                                      : root.bigText.length <= 4 ? 0.5 : 0.36))
        }

        // ---- Folder: stacked sheets ---------------------------------------------------
        Item {
            visible: root.isFolder
            anchors.fill: parent

            Rectangle {
                width: root.width * 0.46
                height: root.height * 0.40
                radius: 10
                x: root.width * 0.5 - width / 2 + 9
                y: root.height * 0.15
                rotation: 5
                color: "#0FFFFFFF"
                border.width: 1
                border.color: "#14FFFFFF"
            }

            Rectangle {
                id: folderBody

                width: root.width * 0.5
                height: root.height * 0.40
                radius: 10
                x: root.width * 0.5 - width / 2
                y: root.height * 0.26
                color: "#1AFFFFFF"
                border.width: 1
                border.color: "#2EFFFFFF"

                Rectangle {
                    x: 14
                    y: -7
                    width: folderBody.width * 0.34
                    height: 12
                    radius: 6
                    color: "#1AFFFFFF"
                    border.width: 1
                    border.color: "#2EFFFFFF"
                }
            }
        }
    }
}
