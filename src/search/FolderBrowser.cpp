#include "FolderBrowser.h"
#include "../core/Logger.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace Flux {

namespace {

bool isVideoExtension(const QString &ext) {
    static const QSet<QString> kVideo = {
        "MKV", "MP4", "AVI", "MOV", "WMV", "M4V", "TS", "M2TS", "WEBM", "FLV", "MPG", "MPEG"
    };
    return kVideo.contains(ext.toUpper());
}

// Keep folders + video files, hide dotfiles, order naturally ("Episode 2" before "Episode 10")
std::vector<SearchResult> postProcess(std::vector<SearchResult> items) {
    std::vector<SearchResult> out;
    out.reserve(items.size());
    for (auto &it : items) {
        if (it.displayName.isEmpty() || it.displayName.startsWith('.')) continue;
        if (it.isFolder || isVideoExtension(it.extension)) {
            out.push_back(std::move(it));
        }
    }

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    std::sort(out.begin(), out.end(), [&collator](const SearchResult &a, const SearchResult &b) {
        if (a.isFolder != b.isFolder) return a.isFolder > b.isFolder;
        return collator.compare(a.displayName, b.displayName) < 0;
    });
    return out;
}

QString decodedNoSlash(const QString &href) {
    QString d = QUrl::fromPercentEncoding(href.toUtf8());
    while (d.endsWith('/')) d.chop(1);
    return d;
}

// h5ai API response: { "items": [ {href, size, ...}, ... ] } (first entry is the folder itself)
std::vector<SearchResult> parseApiItems(const QByteArray &data, const QString &origin,
                                        const QString &requestedHref,
                                        const QString &libraryName, const QString &group) {
    std::vector<SearchResult> out;
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) return out;

    const QString self = decodedNoSlash(requestedHref);
    const QJsonArray arr = doc.object().value("items").toArray();
    for (const QJsonValue &v : arr) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();

        const QString href = o.value("href").toString();
        if (href.isEmpty()) continue;

        const QString decoded = decodedNoSlash(href);
        if (decoded == self) continue;                         // the folder itself
        if (!decoded.startsWith(self + QLatin1Char('/'))) continue;  // not inside this folder

        const QJsonValue sizeVal = o.value("size");
        const bool sizeNull = sizeVal.isNull() || sizeVal.isUndefined();
        const qint64 size = sizeNull ? -1 : sizeVal.toVariant().toLongLong();

        out.push_back(SearchResult::fromJson(href, size, sizeNull, origin, QString(), libraryName, group));
    }
    return out;
}

// Fallback: scrape <a href="..."> links from the server-rendered HTML index page
std::vector<SearchResult> parseHtmlListing(const QString &html, const QString &origin,
                                           const QString &baseHref,
                                           const QString &libraryName, const QString &group) {
    std::vector<SearchResult> out;
    QSet<QString> seen;

    const QString base = decodedNoSlash(baseHref);
    const QUrl originUrl(origin);

    static const QRegularExpression re(QStringLiteral("<a\\s+[^>]*href\\s*=\\s*[\"']([^\"']+)[\"']"),
                                       QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator it = re.globalMatch(html);
    while (it.hasNext()) {
        QString link = it.next().captured(1).trimmed();
        if (link.isEmpty() || link.startsWith('#') || link.startsWith('?')
            || link.startsWith(QLatin1String("mailto:")) || link.startsWith(QLatin1String("javascript:"))) continue;
        if (link == QLatin1String("../") || link == QLatin1String("..") || link == QLatin1String("./")) continue;

        if (link.startsWith(QLatin1String("http://")) || link.startsWith(QLatin1String("https://"))) {
            const QUrl u(link, QUrl::TolerantMode);
            if (u.host() != originUrl.host()) continue;
            link = u.path(QUrl::FullyEncoded);
        }

        const QString fullHref = link.startsWith('/') ? link : (baseHref + link);
        const QString decoded = decodedNoSlash(fullHref);
        if (!decoded.startsWith(base + QLatin1Char('/'))) continue;

        // Direct children only
        const QString rest = decoded.mid(base.size() + 1);
        if (rest.isEmpty() || rest.contains('/')) continue;

        if (seen.contains(decoded)) continue;
        seen.insert(decoded);

        const bool isDir = fullHref.endsWith('/');
        SearchResult r = SearchResult::fromJson(fullHref, isDir ? -1 : 0, isDir, origin, QString(), libraryName, group);
        if (!r.isFolder) r.formattedSize.clear();   // size unknown when scraping
        out.push_back(std::move(r));
    }
    return out;
}

bool hasEpisodeMarker(const QString &name) {
    static const QRegularExpression re(
        QStringLiteral("(?:^|[\\s._\\-\\[\\(])(?:S\\d{1,2}[\\s._\\-]*E\\d{1,3}|E\\d{1,3}|EP\\.?\\s?\\d{1,3}|Episode[\\s._\\-]*\\d{1,3})(?:$|[\\s._\\-\\]\\)])"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(name).hasMatch();
}

// Two files "belong together" if both look like episodes, or differ only by a short
// trailing part (e.g. "Show - 05.mkv" / "Show - 06.mkv"). This stops autoplay from
// jumping between unrelated movies that merely share a folder.
bool looksLikeSameSeries(const QString &a, const QString &b) {
    if (hasEpisodeMarker(a) && hasEpisodeMarker(b)) return true;

    const QString la = a.toLower();
    const QString lb = b.toLower();
    const int minLen = std::min(la.size(), lb.size());
    int cp = 0;
    while (cp < minLen && la[cp] == lb[cp]) ++cp;

    return minLen > 0
           && cp >= std::max(6, static_cast<int>(minLen * 0.75))
           && (la.size() - cp) <= 8 && (lb.size() - cp) <= 8;
}

} // namespace

// ============================================================================

FolderBrowser::FolderBrowser(QObject *parent)
    : QAbstractListModel(parent) {
}

int FolderBrowser::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_items.size());
}

QVariant FolderBrowser::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size())) {
        return QVariant();
    }
    const SearchResult &item = m_items[static_cast<size_t>(index.row())];

    switch (role) {
    case TitleRole:       return item.displayName;
    case PathRole:        return item.parentPath;
    case IsFolderRole:    return item.isFolder;
    case SizeRole:        return item.formattedSize;
    case PlayUrlRole:     return item.playUrl;
    case ExtensionRole:   return item.extension;
    case RawHrefRole:     return item.rawHref;
    case LibraryNameRole: return item.libraryName;
    case GroupRole:       return item.group;
    default:              return QVariant();
    }
}

QHash<int, QByteArray> FolderBrowser::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TitleRole]       = "title";
    roles[PathRole]        = "parentPath";
    roles[IsFolderRole]    = "isFolder";
    roles[SizeRole]        = "formattedSize";
    roles[PlayUrlRole]     = "playUrl";
    roles[ExtensionRole]   = "extension";
    roles[RawHrefRole]     = "rawHref";
    roles[LibraryNameRole] = "libraryName";
    roles[GroupRole]       = "group";
    return roles;
}

// ----------------------------------------------------------------------------
// Location
// ----------------------------------------------------------------------------

bool FolderBrowser::splitUrl(const QString &url, QString &origin, QString &href) {
    const QUrl u(url, QUrl::TolerantMode);
    if (!u.isValid() || u.host().isEmpty()) {
        origin.clear();
        href.clear();
        return false;
    }

    origin = u.scheme() + QStringLiteral("://") + u.host();
    const int port = u.port();
    const bool defaultPort = (port == -1) || (u.scheme() == QLatin1String("http") && port == 80)
                             || (u.scheme() == QLatin1String("https") && port == 443);
    if (!defaultPort) origin += QStringLiteral(":") + QString::number(port);

    href = u.path(QUrl::FullyEncoded);
    if (href.isEmpty()) href = QStringLiteral("/");
    return true;
}

QString FolderBrowser::keyHref(const QString &href) {
    return QUrl::fromPercentEncoding(href.toUtf8());
}

QString FolderBrowser::folderName() const {
    const QVariantList crumbs = breadcrumbs();
    if (crumbs.isEmpty()) return QString();
    return crumbs.last().toMap().value("name").toString();
}

QVariantList FolderBrowser::breadcrumbs() const {
    QVariantList list;
    const QStringList segments = m_href.split('/', Qt::SkipEmptyParts);
    QString accumulated;
    for (const QString &seg : segments) {
        accumulated += QLatin1Char('/') + seg;
        QVariantMap crumb;
        crumb["name"] = QUrl::fromPercentEncoding(seg.toUtf8());
        crumb["url"] = m_origin + accumulated + QLatin1Char('/');
        list.append(crumb);
    }
    return list;
}

// ----------------------------------------------------------------------------
// Navigation
// ----------------------------------------------------------------------------

void FolderBrowser::open(const QString &folderUrl, const QString &libraryName, const QString &group) {
    QString origin, href;
    if (!splitUrl(folderUrl, origin, href)) {
        FLUX_LOG_WARN("Browser", "open(): invalid folder URL: " + folderUrl);
        return;
    }
    if (!href.endsWith('/')) href += QLatin1Char('/');

    m_origin = origin;
    m_href = href;
    if (!libraryName.isEmpty()) m_libraryName = libraryName;
    if (!group.isEmpty()) m_group = group;

    FLUX_LOG_INFO("Browser", QString("Opening folder: %1%2").arg(m_origin, m_href));

    setActive(true);
    emit locationChanged();
    load();
}

void FolderBrowser::close() {
    ++m_loadToken;   // discard any in-flight listing
    setLoading(false);
    setError(QString());

    beginResetModel();
    m_items.clear();
    endResetModel();
    emit itemCountChanged();

    setActive(false);
}

void FolderBrowser::goUp() {
    const QStringList segments = m_href.split('/', Qt::SkipEmptyParts);
    if (segments.size() <= 1) {
        close();
        return;
    }
    QString parent;
    for (int i = 0; i < segments.size() - 1; ++i) {
        parent += QLatin1Char('/') + segments[i];
    }
    parent += QLatin1Char('/');
    open(m_origin + parent);
}

void FolderBrowser::goTo(int crumbIndex) {
    const QVariantList crumbs = breadcrumbs();
    if (crumbIndex < 0 || crumbIndex >= crumbs.size() - 1) return;   // last crumb = here
    open(crumbs.at(crumbIndex).toMap().value("url").toString());
}

void FolderBrowser::reload() {
    m_cache.remove(m_origin + keyHref(m_href));
    load();
}

void FolderBrowser::listFolder(const QString &folderUrl,
                               std::function<void(bool, std::vector<SearchResult>)> done) {
    QString origin, href;
    if (!splitUrl(folderUrl, origin, href)) {
        done(false, {});
        return;
    }
    if (!href.endsWith('/')) href += QLatin1Char('/');
    requestListing(origin, href, QString(), QString(), std::move(done));
}

// ----------------------------------------------------------------------------
// Loading
// ----------------------------------------------------------------------------

void FolderBrowser::load() {
    const quint64 token = ++m_loadToken;
    setLoading(true);
    setError(QString());

    beginResetModel();
    m_items.clear();
    endResetModel();
    emit itemCountChanged();

    QPointer<FolderBrowser> self(this);
    requestListing(m_origin, m_href, m_libraryName, m_group,
                   [self, token](bool ok, std::vector<SearchResult> items) {
        if (!self || token != self->m_loadToken) return;   // superseded by a newer navigation
        self->applyResult(ok, std::move(items));
    });
}

void FolderBrowser::applyResult(bool ok, std::vector<SearchResult> items) {
    setLoading(false);
    if (!ok) {
        setError(QStringLiteral("Couldn't load this folder. Check your connection to the BDIX network."));
        return;
    }

    beginResetModel();
    m_items = std::move(items);
    endResetModel();
    emit itemCountChanged();
    FLUX_LOG_INFO("Browser", QString("Folder listed: %1 items").arg(m_items.size()));
}

void FolderBrowser::storeCache(const QString &key, const std::vector<SearchResult> &items) {
    if (items.empty()) return;   // never cache a failed/empty parse
    if (m_cache.size() > 80) m_cache.clear();
    m_cache.insert(key, items);
}

void FolderBrowser::requestListing(const QString &origin, const QString &href,
                                   const QString &libraryName, const QString &group,
                                   ListingCallback done) {
    const QString cacheKey = origin + keyHref(href);
    const auto cached = m_cache.constFind(cacheKey);
    if (cached != m_cache.constEnd()) {
        done(true, cached.value());
        return;
    }

    QPointer<FolderBrowser> self(this);

    // 1) Preferred: the server's JSON API
    postApi(origin, href, [self, origin, href, libraryName, group, cacheKey, done](bool ok, const QByteArray &data) {
        if (!self) return;

        std::vector<SearchResult> items;
        if (ok) items = parseApiItems(data, origin, href, libraryName, group);
        if (!items.empty()) {
            items = postProcess(std::move(items));
            self->storeCache(cacheKey, items);
            done(true, std::move(items));
            return;
        }

        // 2) Fallback: scrape the HTML index page
        self->getHtml(origin, href, [self, origin, href, libraryName, group, cacheKey, done](bool ok2, const QByteArray &html) {
            if (!self) return;
            if (!ok2) {
                done(false, {});
                return;
            }
            std::vector<SearchResult> items2 = parseHtmlListing(QString::fromUtf8(html), origin, href, libraryName, group);
            items2 = postProcess(std::move(items2));
            self->storeCache(cacheKey, items2);
            done(true, std::move(items2));
        });
    });
}

void FolderBrowser::postApi(const QString &origin, const QString &href,
                            std::function<void(bool, const QByteArray &)> done) {
    QNetworkRequest request{QUrl(origin + href, QUrl::TolerantMode)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=utf-8");
    request.setTransferTimeout(15000);

    QJsonObject items;
    items["href"] = href;
    items["what"] = 1;
    QJsonObject root;
    root["action"] = "get";
    root["items"] = items;

    QNetworkReply *reply = m_network.post(request, QJsonDocument(root).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        done(ok, ok ? reply->readAll() : QByteArray());
    });
}

void FolderBrowser::getHtml(const QString &origin, const QString &href,
                            std::function<void(bool, const QByteArray &)> done) {
    QNetworkRequest request{QUrl(origin + href, QUrl::TolerantMode)};
    request.setRawHeader("Accept", "text/html");
    request.setTransferTimeout(15000);

    QNetworkReply *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [reply, done]() {
        reply->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300;
        done(ok, ok ? reply->readAll() : QByteArray());
    });
}

// ----------------------------------------------------------------------------
// Next episode
// ----------------------------------------------------------------------------

void FolderBrowser::clearNext() {
    ++m_nextToken;
    if (!m_nextUrl.isEmpty() || !m_nextName.isEmpty()) {
        m_nextUrl.clear();
        m_nextName.clear();
        emit nextChanged();
    }
}

void FolderBrowser::findNext(const QString &currentFileUrl) {
    clearNext();
    const quint64 token = m_nextToken;

    // Downloaded (offline) files: look beside the file on disk, no network involved
    const QUrl asUrl(currentFileUrl, QUrl::TolerantMode);
    if (asUrl.isLocalFile()) {
        findNextLocal(asUrl.toLocalFile());
        return;
    }

    QString origin, fileHref;
    if (!splitUrl(currentFileUrl, origin, fileHref)) return;

    const int slash = fileHref.lastIndexOf('/');
    if (slash < 0) return;
    const QString parentHref = fileHref.left(slash + 1);

    const QString currentKey = keyHref(fileHref).toLower();

    QPointer<FolderBrowser> self(this);
    requestListing(origin, parentHref, QString(), QString(),
                   [self, token, currentKey](bool ok, std::vector<SearchResult> items) {
        if (!self || !ok || token != self->m_nextToken) return;

        std::vector<const SearchResult *> files;
        for (const SearchResult &r : items) {
            if (!r.isFolder) files.push_back(&r);
        }

        int idx = -1;
        for (size_t i = 0; i < files.size(); ++i) {
            const QString key = QUrl::fromPercentEncoding(files[i]->rawHref.toUtf8()).toLower();
            if (key == currentKey) { idx = static_cast<int>(i); break; }
        }
        if (idx < 0 || static_cast<size_t>(idx) + 1 >= files.size()) return;

        const SearchResult &cur = *files[static_cast<size_t>(idx)];
        const SearchResult &next = *files[static_cast<size_t>(idx) + 1];
        if (!looksLikeSameSeries(cur.displayName, next.displayName)) return;

        self->m_nextUrl = next.playUrl;
        self->m_nextName = next.displayName;
        emit self->nextChanged();
        FLUX_LOG_INFO("Browser", QString("Next episode resolved: %1").arg(next.displayName));
    });
}

// ----------------------------------------------------------------------------
// Next episode (files already on disk)
// ----------------------------------------------------------------------------

void FolderBrowser::findNextLocal(const QString &filePath) {
    const QFileInfo cur(filePath);
    if (!cur.exists()) return;

    QFileInfoList files = cur.dir().entryInfoList(QDir::Files | QDir::Readable, QDir::NoSort);
    files.erase(std::remove_if(files.begin(), files.end(),
                               [](const QFileInfo &f) { return !isVideoExtension(f.suffix()); }),
                files.end());

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(), [&collator](const QFileInfo &a, const QFileInfo &b) {
        return collator.compare(a.fileName(), b.fileName()) < 0;
    });

    int idx = -1;
    for (int i = 0; i < files.size(); ++i) {
        if (files.at(i).fileName().compare(cur.fileName(), Qt::CaseInsensitive) == 0) { idx = i; break; }
    }
    if (idx < 0 || idx + 1 >= files.size()) return;

    const QFileInfo &next = files.at(idx + 1);
    if (!looksLikeSameSeries(cur.fileName(), next.fileName())) return;

    m_nextUrl = QString::fromLatin1(QUrl::fromLocalFile(next.absoluteFilePath()).toEncoded());
    m_nextName = next.fileName();
    emit nextChanged();
    FLUX_LOG_INFO("Browser", QString("Next offline episode resolved: %1").arg(next.fileName()));
}

// ----------------------------------------------------------------------------
// State helpers
// ----------------------------------------------------------------------------

void FolderBrowser::setActive(bool active) {
    if (m_active != active) {
        m_active = active;
        emit activeChanged();
    }
}

void FolderBrowser::setLoading(bool loading) {
    if (m_loading != loading) {
        m_loading = loading;
        emit isLoadingChanged();
    }
}

void FolderBrowser::setError(const QString &msg) {
    if (m_error != msg) {
        m_error = msg;
        emit errorChanged();
    }
}

} // namespace Flux
