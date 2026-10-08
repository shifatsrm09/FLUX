.pragma library

function formatMedia(rawTitle, isFolder, formattedSize) {
    if (!rawTitle || rawTitle.length === 0) {
        return { title: "", subtitle: "" };
    }

    // Strip common media file extensions
    var clean = rawTitle.replace(/\.(mkv|mp4|avi|mov|wmv|ts|m4v)$/i, "").trim();

    var title = "";
    var metaTokens = [];

    // 1. Detect TV Series episode format (e.g. Title S01E02 ...)
    var seriesMatch = clean.match(/^(.*?)[ ._-\[]+S(\d{1,2})E(\d{1,2})(.*)$/i);
    if (seriesMatch) {
        title = seriesMatch[1].replace(/[._]/g, " ").replace(/[-–]+$/, "").trim();
        var sNum = parseInt(seriesMatch[2], 10);
        var eNum = parseInt(seriesMatch[3], 10);
        var rest = seriesMatch[4];

        metaTokens.push("Season " + sNum + " · Episode " + eNum);

        var resMatch = rest.match(/\b(2160p|4K|1080p|720p|480p)\b/i);
        if (resMatch) metaTokens.push(resMatch[1].toLowerCase());

        var srcMatch = rest.match(/\b(BluRay|BRRip|WEBRip|WEB-DL|HDTV)\b/i);
        if (srcMatch) metaTokens.push(srcMatch[1]);

        if (/\b(HEVC|x265)\b/i.test(rest)) metaTokens.push("HEVC");
        else if (/\b(x264|AVC)\b/i.test(rest)) metaTokens.push("H.264");

        if (/\bDual Audio\b/i.test(rest)) metaTokens.push("Dual Audio");
    } else {
        // 2. Detect Movie with Year (e.g. Title (2024) ... or Title 2024 ...)
        var movieMatch = clean.match(/^(.*?)[ ._-\[\(]+(19\d{2}|20\d{2})[ ._-\]\)](.*)$/i);
        if (movieMatch) {
            title = movieMatch[1].replace(/[._]/g, " ").replace(/[\(\[\)\]]/g, "").replace(/[-–]+$/, "").trim();
            var year = movieMatch[2];
            var remainder = movieMatch[3];

            metaTokens.push(year);

            var resMatch = remainder.match(/\b(2160p|4K|1080p|720p|480p)\b/i);
            if (resMatch) metaTokens.push(resMatch[1].toLowerCase());

            var srcMatch = remainder.match(/\b(BluRay|BRRip|WEBRip|WEB-DL|HDTV)\b/i);
            if (srcMatch) metaTokens.push(srcMatch[1]);

            if (/\b(HEVC|x265)\b/i.test(remainder)) metaTokens.push("HEVC");
            else if (/\b(x264|AVC)\b/i.test(remainder)) metaTokens.push("H.264");

            if (/\bDual Audio\b/i.test(remainder)) metaTokens.push("Dual Audio");
        } else {
            // 3. Fallback: clean dots and underscores
            title = clean.replace(/[._]/g, " ").replace(/[-–]+$/, "").trim();

            var resFallback = clean.match(/\b(2160p|4K|1080p|720p|480p)\b/i);
            if (resFallback) metaTokens.push(resFallback[1].toLowerCase());
        }
    }

    if (title.length === 0) {
        title = rawTitle;
    }

    // Add size or folder indicator to metadata
    if (isFolder) {
        metaTokens.push("Directory");
    } else if (formattedSize && formattedSize.length > 0 && formattedSize !== "Folder") {
        metaTokens.push(formattedSize);
    }

    return {
        title: title,
        subtitle: metaTokens.join(" · ")
    };
}

// Presentation helper: splits a raw filename into individual badge fields so the
// UI can render quality chips / year / codec separately. Purely additive; the
// title/subtitle values come straight from formatMedia() above.
function describe(rawTitle, isFolder, formattedSize) {
    var base = formatMedia(rawTitle, isFolder, formattedSize);
    var clean = (rawTitle || "").replace(/\.(mkv|mp4|avi|mov|wmv|ts|m4v)$/i, "");

    var info = {
        title: base.title,
        subtitle: base.subtitle,
        resolution: "",
        year: "",
        episode: "",
        source: "",
        codec: "",
        hdr: "",
        dualAudio: false
    };

    if (isFolder) return info;

    var res = clean.match(/\b(2160p|4K|1080p|720p|480p)\b/i);
    if (res) {
        var rv = res[1].toLowerCase();
        info.resolution = (rv === "2160p" || rv === "4k") ? "4K" : rv;
    }

    var ep = clean.match(/\bS(\d{1,2})E(\d{1,3})\b/i);
    if (ep) {
        info.episode = "S" + parseInt(ep[1], 10) + " · E" + parseInt(ep[2], 10);
    }

    var yr = clean.match(/(?:^|[ ._\-\[\(])((?:19|20)\d{2})(?=[ ._\-\]\)]|$)/);
    if (yr) info.year = yr[1];

    var src = clean.match(/\b(BluRay|BRRip|WEBRip|WEB-DL|HDTV)\b/i);
    if (src) info.source = src[1];

    if (/\b(HEVC|x265|H\.?265)\b/i.test(clean)) info.codec = "HEVC";
    else if (/\b(x264|AVC|H\.?264)\b/i.test(clean)) info.codec = "H.264";

    var hdr = clean.match(/\b(HDR10\+?|HDR|Dolby Vision|DV)\b/i);
    if (hdr) info.hdr = /dolby|^dv$/i.test(hdr[1]) ? "DV" : "HDR";

    info.dualAudio = /\bDual[ ._-]?Audio\b/i.test(clean);

    return info;
}
