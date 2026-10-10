#pragma once

#include <QString>
#include <QUrl>

namespace Flux {

struct SearchResult {
    QString rawHref;        // URL-encoded path relative to media server, e.g. "/DHAKA-FLIX-14/.../movie.mkv"
    QString playUrl;        // Complete playable URL: serverOrigin + rawHref
    QString displayName;    // Decoded filename or directory name
    QString parentPath;     // Decoded directory path containing this item
    bool isFolder = false;
    qint64 sizeBytes = -1;
    QString formattedSize;
    QString extension;
    QString libraryId;
    QString libraryName;
    QString group;
    int matchScore = 3;     // search relevance: 3 exact, 2 all words, 1 close match (see SearchQuery)

    static SearchResult fromJson(const QString &href,
                                 qint64 size,
                                 bool isSizeNull,
                                 const QString &serverOrigin,
                                 const QString &libraryId = "",
                                 const QString &libraryName = "",
                                 const QString &group = "");

    static QString formatBytes(qint64 bytes);
};

} // namespace Flux
