.pragma library

// ============================================================================
// FLUX design tokens
// Imported everywhere as:  import "Theme.js" as Theme   (or "components/Theme.js")
// ============================================================================

// ---- Brand -----------------------------------------------------------------
var accent        = "#E50914"
var accentHover   = "#F6121D"
var accentPressed = "#B20710"
var accentSoft    = "#26E50914"   // 15% accent wash
var accentRing    = "#80E50914"   // 50% accent for borders

// ---- Surfaces (near-black, cinematic) ---------------------------------------
var bg         = "#0A0A0D"
var bgRaised   = "#0F0F13"
var surface    = "#15151A"
var surfaceHi  = "#1C1C23"
var surfaceTop = "#25252E"
var border     = "#23232B"
var borderHi   = "#363641"

// ---- Text --------------------------------------------------------------------
var text     = "#F5F5F7"
var textDim  = "#A9A9B6"
var textMute = "#6C6C7A"

// ---- Status ------------------------------------------------------------------
var success = "#46D369"
var warning = "#F5B83D"
var danger  = "#FF5D5D"

// ---- Typography --------------------------------------------------------------
var fontFamily = "Segoe UI"
var monoFamily = "Consolas"

// ---- Poster palette ----------------------------------------------------------
// [top, bottom] pairs. Rich, deep tones so generated poster art always looks
// cinematic. A title always maps to the same pair.
var posterPalette = [
    ["#8A1C2B", "#16070B"],
    ["#1F3A6E", "#070B16"],
    ["#4B2380", "#0C0716"],
    ["#0E5A57", "#041211"],
    ["#8A4B14", "#150B04"],
    ["#7A1F5C", "#14060F"],
    ["#1B5E86", "#06111A"],
    ["#6B6A12", "#121105"],
    ["#2C6B2A", "#08130A"],
    ["#8C2A3F", "#150810"],
    ["#3A3F9E", "#0A0B1A"],
    ["#A0521C", "#180B04"]
]

// ---- Helpers -----------------------------------------------------------------

// Deterministic string hash (Java-style), always a non-negative integer.
function hash(str) {
    var h = 0
    var s = str ? String(str) : ""
    for (var i = 0; i < s.length; i++) {
        h = ((h << 5) - h + s.charCodeAt(i)) | 0
    }
    return Math.abs(h)
}

function paletteIndex(key) {
    return hash(key) % posterPalette.length
}

function posterTop(key) {
    return posterPalette[paletteIndex(key)][0]
}

// Which of the 6 decorative poster compositions this title gets (stable per title)
function posterStyle(key) {
    return Math.floor(hash(String(key) + "#") / 7) % 6
}

function posterBottom(key) {
    return posterPalette[paletteIndex(key)][1]
}

// "#RRGGBB" + alpha(0..1)  ->  "#AARRGGBB"
function alpha(hex, a) {
    var h = String(hex).replace("#", "")
    if (h.length === 8) h = h.substring(2)
    var v = Math.round(Math.max(0, Math.min(1, a)) * 255).toString(16)
    if (v.length < 2) v = "0" + v
    return "#" + v + h
}

function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v))
}

// QML color -> CSS rgba() string (for Canvas 2D contexts)
function css(c) {
    return "rgba(" + Math.round(c.r * 255) + "," + Math.round(c.g * 255) + ","
           + Math.round(c.b * 255) + "," + c.a + ")"
}

// milliseconds -> "m:ss" or "h:mm:ss"
function formatTime(ms) {
    if (!ms || ms < 0 || isNaN(ms)) ms = 0
    var total = Math.floor(ms / 1000)
    var h = Math.floor(total / 3600)
    var m = Math.floor((total % 3600) / 60)
    var s = total % 60
    var mm = (m < 10 ? "0" : "") + m
    var ss = (s < 10 ? "0" : "") + s
    return h > 0 ? (h + ":" + mm + ":" + ss) : (mm + ":" + ss)
}

// Video dimensions -> badge label ("4K", "1080p", ...)
function qualityLabel(w, h) {
    if (!w || w <= 0) return ""
    if (w >= 3200 || h >= 2000) return "4K"
    if (w >= 2400 || h >= 1400) return "1440p"
    if (w >= 1800 || h >= 1000) return "1080p"
    if (w >= 1200 || h >= 700)  return "720p"
    return "SD"
}
