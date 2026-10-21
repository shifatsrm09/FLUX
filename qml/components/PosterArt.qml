import QtQuick
import "Theme.js" as Theme

// Typographic and iconographic artwork for items without a poster image.
// Episodes, movies, and folders each have clean, tailored visual designs that
// strictly fit within vertical card bounds with zero overflow or clipping.
Item {
    id: root

    property string seed: ""
    property string bigText: ""
    property bool isFolder: false
    property bool isEpisode: false
    property string episodeText: ""
    property string yearText: ""
    property string mediaTitle: ""
    property bool hovered: false

    // Auto-detect if this is an episode if not explicitly set
    readonly property bool effectiveIsEpisode: {
        if (root.isFolder) return false
        if (root.isEpisode) return true
        if (root.episodeText && root.episodeText.length > 0) return true
        if (root.bigText && (/^E\d+/i.test(root.bigText) || /^S\d+/i.test(root.bigText))) return true
        if (root.seed && /\bS\d{1,2}E\d{1,3}\b/i.test(root.seed)) return true
        return false
    }

    // Clean, bounded badge text (e.g. "S1 · E02", "2024", "MOVIE")
    readonly property string badgeText: {
        if (root.isFolder) return ""
        if (root.episodeText && root.episodeText.length > 0) {
            return root.episodeText.toUpperCase()
        }
        if (root.yearText && root.yearText.length > 0) {
            return root.yearText
        }
        if (root.bigText && root.bigText.length > 0) {
            if (/^\d{4}$/.test(root.bigText)) return root.bigText
            if (/^E\d+/i.test(root.bigText)) return "EPISODE " + root.bigText.substring(1)
            if (/^S\d+/i.test(root.bigText)) return root.bigText.toUpperCase()
        }
        var ym = root.seed ? root.seed.match(/\b(19\d{2}|20\d{2})\b/) : null
        if (ym) return ym[1]
        return root.effectiveIsEpisode ? "EPISODE" : "MOVIE"
    }

    // Folder art proportions: Maintain a classic landscape aspect ratio (~1.48:1)
    // so it never stretches vertically regardless of whether the card is wide or tall.
    readonly property real folderW: Math.round(Math.max(76, Math.min(root.width * 0.54, 108)))
    readonly property real folderH: Math.round(folderW * 0.68)

    // Emblem dimensions for movies & episodes
    readonly property real emblemSize: Math.round(Math.min(64, Math.max(46, root.width * 0.34)))

    // Artwork container with subtle hover lift
    Item {
        id: art
        anchors.fill: parent
        transformOrigin: Item.Center
        scale: root.hovered ? 1.04 : 1.0

        Behavior on scale { NumberAnimation { duration: 350; easing.type: Easing.OutCubic } }

        // ==============================================================================
        // Movie & Episode Fallback Art
        // ==============================================================================
        Item {
            visible: !root.isFolder
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.round(root.height * 0.38) // Preserve lower space for title block

            // Elegant center column containing media emblem and metadata badge
            Column {
                anchors.centerIn: parent
                anchors.verticalCenterOffset: -4
                spacing: 12
                width: parent.width
                z: 1

                // Center emblem
                Rectangle {
                    id: emblemBox
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: root.emblemSize
                    height: width
                    radius: Math.round(width * 0.32)
                    color: root.hovered ? "#24FFFFFF" : "#18FFFFFF"
                    border.width: 1
                    border.color: root.hovered ? "#3AFFFFFF" : "#28FFFFFF"

                    // Subtle atmospheric radial glow
                    Rectangle {
                        anchors.centerIn: parent
                        width: Math.round(parent.width * 2.2)
                        height: width
                        radius: width / 2
                        color: root.effectiveIsEpisode ? "#105865F2" : "#12FFFFFF"
                        z: -1
                    }

                    Behavior on color { ColorAnimation { duration: 200 } }
                    Behavior on border.color { ColorAnimation { duration: 200 } }

                    FluxIcon {
                        name: root.effectiveIsEpisode ? "tv" : "film"
                        size: Math.round(emblemBox.width * 0.52)
                        color: root.hovered ? "#FFFFFF" : "#E2E8F0"
                        anchors.centerIn: parent
                        Behavior on color { ColorAnimation { duration: 200 } }
                    }
                }

                // Metadata pill badge (EPISODE 02 / 2024 / MOVIE)
                Rectangle {
                    visible: root.badgeText.length > 0
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width - 24, badgeLabel.implicitWidth + 20)
                    height: 22
                    radius: 11
                    color: "#1FFFFFFF"
                    border.width: 1
                    border.color: "#2EFFFFFF"
                    clip: true

                    Text {
                        id: badgeLabel
                        anchors.centerIn: parent
                        width: Math.min(implicitWidth, parent.width - 12)
                        text: root.badgeText
                        color: "#E2E8F0"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        font.letterSpacing: 0.9
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
        }

        // ==============================================================================
        // Folder Art (Stacked sheets with folder icon)
        // ==============================================================================
        Item {
            visible: root.isFolder
            anchors.fill: parent

            // Back tilted folder sheet for visual depth
            Rectangle {
                width: Math.round(root.folderW * 0.94)
                height: root.folderH
                radius: 8
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: 6
                y: folderBody.y - Math.round(root.folderH * 0.14)
                rotation: 4.5
                color: "#0EFFFFFF"
                border.width: 1
                border.color: "#18FFFFFF"
            }

            // Document sheet peeking from behind the folder tab
            Rectangle {
                width: Math.round(root.folderW * 0.78)
                height: Math.round(root.folderH * 0.58)
                radius: 5
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -2
                y: folderBody.y - Math.round(root.folderH * 0.20)
                color: "#14FFFFFF"
                border.width: 1
                border.color: "#24FFFFFF"
            }

            // Main front folder body
            Rectangle {
                id: folderBody

                width: root.folderW
                height: root.folderH
                radius: 8
                anchors.horizontalCenter: parent.horizontalCenter
                y: Math.round(root.height * 0.35 - root.folderH / 2)
                color: "#1CFFFFFF"
                border.width: 1
                border.color: "#30FFFFFF"

                // Folder tab (top-left)
                Rectangle {
                    x: 10
                    y: -Math.round(root.folderH * 0.16)
                    width: Math.round(root.folderW * 0.36)
                    height: Math.round(root.folderH * 0.22)
                    radius: 5
                    color: "#1CFFFFFF"
                    border.width: 1
                    border.color: "#30FFFFFF"
                }

                // Seamless blend cover for the tab's bottom border
                Rectangle {
                    x: 11
                    y: 0
                    width: Math.round(root.folderW * 0.36) - 2
                    height: 2
                    color: folderBody.color
                }

                // Centered folder icon vector
                FluxIcon {
                    name: "folder"
                    size: Math.round(root.folderH * 0.44)
                    color: "#5AFFFFFF"
                    anchors.centerIn: parent
                    anchors.verticalCenterOffset: 1
                }
            }
        }
    }
}
