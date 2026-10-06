#include "SearchResult.h"
#include <QFileInfo>
#include <QStringList>

namespace Flux {

SearchResult SearchResult::fromJson(const QString &href,
                                     qint64 size,
                                     bool isSizeNull,
                                     const QString &serverOrigin,
                                     const QString &libraryId,
                                     const QString &libraryName,
                                     const QString &group) {
    SearchResult result;
    result.rawHref = href;
    result.sizeBytes = isSizeNull ? -1 : size;
    result.isFolder = isSizeNull || href.endsWith('/');
    result.libraryId = libraryId;
    result.libraryName = libraryName;
    result.group = group;

    // Construct full playable URL without altering the server-provided percent-encoding
    // Ensure serverOrigin has no trailing slash and href begins with a slash
    QString origin = serverOrigin;
    if (origin.endsWith('/')) {
        origin.chop(1);
    }
    QString path = href;
    if (!path.startsWith('/')) {
        path = "/" + path;
    }
    result.playUrl = origin + path;

    // URL-decode for clean user-facing display
    QString decoded = QUrl::fromPercentEncoding(href.toUtf8());

    if (result.isFolder) {
        // Strip trailing slash for parsing parent and name
        QString trimmed = decoded;
        if (trimmed.endsWith('/')) {
            trimmed.chop(1);
        }

        int lastSlash = trimmed.lastIndexOf('/');
        if (lastSlash >= 0) {
            result.displayName = trimmed.mid(lastSlash + 1);
            result.parentPath = trimmed.left(lastSlash);
        } else {
            result.displayName = trimmed;
            result.parentPath = "/";
        }

        result.extension = "DIR";
        result.formattedSize = "Folder";
    } else {
        int lastSlash = decoded.lastIndexOf('/');
        if (lastSlash >= 0) {
            result.displayName = decoded.mid(lastSlash + 1);
            result.parentPath = decoded.left(lastSlash);
        } else {
            result.displayName = decoded;
            result.parentPath = "/";
        }

        int lastDot = result.displayName.lastIndexOf('.');
        if (lastDot >= 0 && lastDot < result.displayName.length() - 1) {
            result.extension = result.displayName.mid(lastDot + 1).toUpper();
        } else {
            result.extension = "FILE";
        }

        result.formattedSize = formatBytes(size);
    }

    return result;
}

QString SearchResult::formatBytes(qint64 bytes) {
    if (bytes <= 0) return "0 B";

    const double k = 1024.0;
    const double m = k * 1024.0;
    const double g = m * 1024.0;

    double dBytes = static_cast<double>(bytes);

    if (dBytes >= g) {
        return QString("%1 GB").arg(QString::number(dBytes / g, 'f', 2));
    }
    if (dBytes >= m) {
        return QString("%1 MB").arg(QString::number(dBytes / m, 'f', 1));
    }
    if (dBytes >= k) {
        return QString("%1 KB").arg(QString::number(dBytes / k, 'f', 1));
    }
    return QString("%1 B").arg(bytes);
}

} // namespace Flux
