#pragma once

#include <QString>
#include <QUrl>

namespace Flux {

struct SearchResult {
    QString rawHref;        // URL-encoded path relative to media server, e.g. "/DHAKA-FLIX-14/.../movie.mkv"
    QString playUrl;        // Complete playable URL: "http://172.16.50.14" + rawHref
    QString displayName;    // Decoded filename or directory name
    QString parentPath;     // Decoded directory path containing this item
    bool isFolder = false;
    qint64 sizeBytes = -1;
    QString formattedSize;
    QString extension;

    static SearchResult fromJson(const QString &href, qint64 size, bool isSizeNull);
    static QString formatBytes(qint64 bytes);
};

} // namespace Flux
