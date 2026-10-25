#include "FolderBrowser.h"
#include "../core/Logger.h"
#include "../core/UserStore.h"

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

// Two files "belong together" if both look like episodes, or differ only by numbers ("Show - 05.mkv"
// / "Show - 06.mkv"), or by a short trailing part. This stops autoplay from jumping between
// unrelated movies that merely share a folder.
QStringList digitRuns(const QString &s) {
    QStringList runs;
    QString cur;
    for (const QChar c : s) {
        if (c.isDigit()) {
            cur.append(c);
        } else if (!cur.isEmpty()) {
            runs.append(cur);
            cur.clear();
        }
    }
    if (!cur.isEmpty()) runs.append(cur);
    return runs;
}

bool isYearLike(const QString &run) {
    return run.size() == 4 && (run.startsWith(QLatin1String("19")) || run.startsWith(QLatin1String("20")));
}

QString stripExtension(QString s) {
    const int dot = s.lastIndexOf('.');
    if (dot > 0 && s.size() - dot <= 5) s.truncate(dot);
    return s;
}

// "Show - 05 [1080p].mkv" -> "show - # [#p]"
QString digitMask(const QString &s) {
    QString out;
    bool inDigits = false;
    for (const QChar c : stripExtension(s).toLower()) {
        if (c.isDigit()) {
            if (!inDigits) out.append('#');
            inDigits = true;
        } else {
            out.append(c);
            inDigits = false;
        }
    }
    return out;
}

// The whole number touching position `pos` (empty when there is none)
QString digitRunAround(const QString &s, int pos) {
    int l = pos;
    int r = pos;
    while (l > 0 && s[l - 1].isDigit()) --l;
    while (r < s.size() && s[r].isDigit()) ++r;
    return s.mid(l, r - l);
}

// "S01E02" / "E5" (empty when the name has no episode number)
QString episodeKey(const QString &name) {
    static const QRegularExpression se(QStringLiteral("S(\\d{1,2})[\\s._\\-]*E(\\d{1,3})"),
                                       QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression ep(
        QStringLiteral("(?:^|[\\s._\\-\\[\\(])(?:E|EP\\.?\\s?|Episode[\\s._\\-]*)(\\d{1,3})(?:$|[\\s._\\-\\]\\)])"),
        QRegularExpression::CaseInsensitiveOption);

    QRegularExpressionMatch m = se.match(name);
    if (m.hasMatch()) {
        return QStringLiteral("S%1E%2").arg(m.captured(1).toInt()).arg(m.captured(2).toInt());
    }
    m = ep.match(name);
    if (m.hasMatch()) return QStringLiteral("E%1").arg(m.captured(1).toInt());
    return QString();
}

bool looksLikeSameSeries(const QString &a, const QString &b) {
    if (hasEpisodeMarker(a) && hasEpisodeMarker(b)) return true;

    // Identical apart from numbers. Two different movies that differ only by release year
    // ("Movie (2012)" / "Movie (2015)") are not a series.
    if (digitMask(a) == digitMask(b)) {
        const QStringList ra = digitRuns(stripExtension(a));
        const QStringList rb = digitRuns(stripExtension(b));
        bool anyDifference = false;
        bool onlyYears = true;
        for (int i = 0; i < ra.size() && i < rb.size(); ++i) {
            if (ra.at(i) == rb.at(i)) continue;
            anyDifference = true;
            if (!(isYearLike(ra.at(i)) && isYearLike(rb.at(i)))) onlyYears = false;
        }
        return anyDifference && !onlyYears;
    }

    const QString la = a.toLower();
    const QString lb = b.toLower();
    const int minLen = std::min(la.size(), lb.size());
    int cp = 0;
    while (cp < minLen && la[cp] == lb[cp]) ++cp;

    if (minLen > 0
        && cp >= std::max(6, static_cast<int>(minLen * 0.75))
        && (la.size() - cp) <= 8 && (lb.size() - cp) <= 8) {
        return true;
    }

    // The names first differ inside a number ("... - 05 [1080p][ABCD]" vs "... - 06 [1080p][EF01]")
    if (cp >= 5 && cp < minLen) {
        const QString ra = digitRunAround(la, cp);
        const QString rb = digitRunAround(lb, cp);
        if (!ra.isEmpty() && !rb.isEmpty() && !(isYearLike(ra) && isYearLike(rb))) return true;
    }
    return false;
}

// Index of the episode that follows names[idx], or -1. Skips other copies of the same
// episode (e.g. the 720p and 1080p files of S01E01 sitting side by side).
int pickNextIndex(const QStringList &names, int idx) {
    const QString &cur = names.at(idx);
    const QString curKey = episodeKey(cur);
    for (int j = idx + 1; j < names.size(); ++j) {
        if (!curKey.isEmpty() && episodeKey(names.at(j)) == curKey) continue;
        return looksLikeSameSeries(cur, names.at(j)) ? j : -1;
    }
    return -1;
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
    case PosterUrlRole:   return item.posterUrl;
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
    roles[PosterUrlRole]   = "posterUrl";
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
    if (u.hasQuery()) {
        href += QStringLiteral("?") + u.query(QUrl::FullyEncoded);
    }
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
    if (m_href.contains(QLatin1String("player.php"))) {
        QVariantMap rootCrumb;
        rootCrumb["name"] = m_libraryName.isEmpty() ? QStringLiteral("TV Shows") : m_libraryName;
        rootCrumb["url"] = m_origin + QStringLiteral("/");
        list.append(rootCrumb);

        QVariantMap crumb;
        crumb["name"] = m_group.isEmpty() ? QStringLiteral("Episodes") : m_group;
        crumb["url"] = m_origin + m_href;
        list.append(crumb);
        return list;
    }
    if (m_href.contains(QLatin1String("category="))) {
        QVariantMap rootCrumb;
        rootCrumb["name"] = QStringLiteral("Home");
        rootCrumb["url"] = m_origin + QStringLiteral("/");
        list.append(rootCrumb);

        QVariantMap crumb;
        crumb["name"] = m_libraryName.isEmpty() ? QStringLiteral("Category") : m_libraryName;
        crumb["url"] = m_origin + m_href;
        list.append(crumb);
        return list;
    }

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
    if (!href.contains('?') && !href.endsWith('/')) href += QLatin1Char('/');

    if (m_active && !m_restoringPreviousLocation && (origin + href) != currentUrl())
        m_navigationHistory.append(currentUrl());

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
    m_navigationHistory.clear();
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

void FolderBrowser::goBack() {
    if (m_navigationHistory.isEmpty()) {
        close();
        return;
    }

    const QString previousUrl = m_navigationHistory.takeLast();
    m_restoringPreviousLocation = true;
    open(previousUrl);
    m_restoringPreviousLocation = false;
}

void FolderBrowser::goTo(int crumbIndex) {
    const QVariantList crumbs = breadcrumbs();
    if (crumbIndex < 0 || crumbIndex >= crumbs.size() - 1) return;   // last crumb = here
    m_navigationHistory.clear();
    m_restoringPreviousLocation = true;
    open(crumbs.at(crumbIndex).toMap().value("url").toString());
    m_restoringPreviousLocation = false;
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

    // Special handling for Circle FTP series/posts
    if (href.contains(QLatin1String("/api/posts/")) || origin.contains(QLatin1String("circleftp.net:5000"))) {
        self->getHtml(origin, href, [self, origin, href, libraryName, group, cacheKey, done](bool ok, const QByteArray &data) {
            if (!self) return;
            if (!ok) {
                done(false, {});
                return;
            }
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (!doc.isObject()) {
                done(false, {});
                return;
            }
            QJsonObject post = doc.object();
            QString showTitle = post["title"].toString().trimmed();
            QString image = post["image"].toString().trimmed();
            if (image.isEmpty()) image = post["imageSm"].toString().trimmed();
            QString pagePosterUrl;
            if (!image.isEmpty()) {
                if (image.startsWith(QLatin1String("http://")) || image.startsWith(QLatin1String("https://"))) {
                    pagePosterUrl = image;
                } else {
                    pagePosterUrl = QStringLiteral("http://new.circleftp.net:5000/uploads/") + image;
                }
            }

            std::vector<SearchResult> epItems;
            if (post.contains("content") && post["content"].isArray()) {
                QJsonArray seasons = post["content"].toArray();
                for (const auto &sVal : seasons) {
                    if (!sVal.isObject()) continue;
                    QJsonObject sObj = sVal.toObject();
                    QString sName = sObj["seasonName"].toString().trimmed();
                    if (sName.isEmpty()) sName = QStringLiteral("Season 1");
                    QJsonArray episodes = sObj["episodes"].toArray();
                    for (const auto &eVal : episodes) {
                        if (!eVal.isObject()) continue;
                        QJsonObject eObj = eVal.toObject();
                        QString epTitle = eObj["title"].toString().trimmed();
                        QString epLink = eObj["link"].toString().trimmed();
                        if (epLink.isEmpty()) continue;

                        QString filename = QUrl::fromPercentEncoding(QUrl(epLink).fileName().toUtf8());
                        SearchResult r;
                        r.displayName = epTitle.isEmpty() ? filename : epTitle;
                        r.parentPath = sName;
                        r.isFolder = false;
                        r.formattedSize = QString();
                        r.playUrl = epLink;
                        r.rawHref = epLink;
                        r.libraryName = libraryName.isEmpty() ? QStringLiteral("Circle FTP") : libraryName;
                        r.group = sName;
                        r.posterUrl = pagePosterUrl;
                        r.extension = QFileInfo(filename).suffix();
                        epItems.push_back(std::move(r));
                    }
                }
            } else if (post.contains("content") && post["content"].isString()) {
                QString link = post["content"].toString().trimmed();
                if (!link.isEmpty()) {
                    QString filename = QUrl::fromPercentEncoding(QUrl(link).fileName().toUtf8());
                    SearchResult r;
                    r.displayName = showTitle.isEmpty() ? filename : showTitle;
                    r.parentPath = QStringLiteral("Movies");
                    r.isFolder = false;
                    r.formattedSize = QString();
                    r.playUrl = link;
                    r.rawHref = link;
                    r.libraryName = libraryName.isEmpty() ? QStringLiteral("Circle FTP") : libraryName;
                    r.group = group;
                    r.posterUrl = pagePosterUrl;
                    r.extension = QFileInfo(filename).suffix();
                    epItems.push_back(std::move(r));
                }
            }

            if (!epItems.empty()) {
                const QString webFolderUrl = origin + href;
                if (!pagePosterUrl.isEmpty()) {
                    emit self->posterFound(webFolderUrl, pagePosterUrl, showTitle);
                }

                for (const auto &item : epItems) {
                    if (!item.playUrl.isEmpty()) {
                        self->m_mediaToWebFolder[item.playUrl] = webFolderUrl;
                        if (!pagePosterUrl.isEmpty()) {
                            emit self->posterFound(item.playUrl, pagePosterUrl, item.displayName);
                        }
                    }
                }

                epItems = postProcess(std::move(epItems));
                self->storeCache(cacheKey, epItems);
                done(true, std::move(epItems));
                return;
            }
            done(false, {});
        });
        return;
    }

    // Special handling for ICC FTP series/episodes or category listings
    if (href.contains(QLatin1String("player.php"))) {
        self->getHtml(origin, href, [self, origin, href, libraryName, group, cacheKey, done](bool ok, const QByteArray &html) {
            if (!self) return;
            if (!ok) {
                done(false, {});
                return;
            }
            QString htmlStr = QString::fromUtf8(html);
            std::vector<SearchResult> epItems;

            QString pagePosterUrl;
            static const QRegularExpression posterImgRx(
                QStringLiteral("<img[^>]+src=[\"'](files/[^\"']+)[\"']"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch pMatch = posterImgRx.match(htmlStr);
            if (pMatch.hasMatch()) {
                QString rel = pMatch.captured(1).trimmed();
                if (!rel.startsWith('/')) rel.prepend('/');
                pagePosterUrl = origin + rel;
            } else {
                static const QRegularExpression posterScriptRx(
                    QStringLiteral("poster(?:Image)?['\"]?\\s*:\\s*['\"](files/[^'\"]+)['\"]"),
                    QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch pScriptMatch = posterScriptRx.match(htmlStr);
                if (pScriptMatch.hasMatch()) {
                    QString rel = pScriptMatch.captured(1).trimmed();
                    if (!rel.startsWith('/')) rel.prepend('/');
                    pagePosterUrl = origin + rel;
                }
            }

            QString showTitle = group;
            if (showTitle.isEmpty()) {
                static const QRegularExpression titleRx(QStringLiteral("<h[234][^>]*>(.*?)</h[234]>"), QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch tMatch = titleRx.match(htmlStr);
                if (tMatch.hasMatch()) showTitle = tMatch.captured(1).trimmed();
            }

            static const QRegularExpression episodeLiRx(
                QStringLiteral("<li>\\s*<a\\s+[^>]*href=[\"']([^\"']+)[\"'][^>]*download[^>]*>\\s*([^<]*?)\\s*<span[^>]*class=[\"']pull-right[\"'][^>]*>\\s*([^<]*?)\\s*</span>"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator liIt = episodeLiRx.globalMatch(htmlStr);
            while (liIt.hasNext()) {
                auto m = liIt.next();
                QString epUrl = m.captured(1).trimmed();
                QString epTitle = m.captured(2).trimmed();
                QString epSize = m.captured(3).trimmed();

                QString filename = QUrl::fromPercentEncoding(QUrl(epUrl).fileName().toUtf8());
                QString displayName = epTitle;
                if (!filename.isEmpty() && !displayName.contains(filename)) {
                    displayName = epTitle + " - " + filename;
                }

                SearchResult r;
                r.displayName = displayName;
                r.parentPath = libraryName.isEmpty() ? QStringLiteral("TV Shows") : libraryName;
                r.isFolder = false;
                r.formattedSize = epSize;
                r.playUrl = epUrl;
                r.rawHref = epUrl;
                r.libraryName = libraryName;
                r.group = group;
                r.posterUrl = pagePosterUrl;
                r.extension = QFileInfo(filename).suffix();
                epItems.push_back(std::move(r));
            }

            if (epItems.empty()) {
                static const QRegularExpression sourceRx(
                    QStringLiteral("<source\\s+[^>]*src=['\"]([^'\"]+)['\"][^>]*title=['\"]([^'\"]*)['\"]"),
                    QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatchIterator srcIt = sourceRx.globalMatch(htmlStr);
                while (srcIt.hasNext()) {
                    auto m = srcIt.next();
                    QString epUrl = m.captured(1).trimmed();
                    QString epTitle = m.captured(2).trimmed();
                    QString filename = QUrl::fromPercentEncoding(QUrl(epUrl).fileName().toUtf8());

                    SearchResult r;
                    r.displayName = epTitle.isEmpty() ? filename : (epTitle + " - " + filename);
                    r.parentPath = libraryName.isEmpty() ? QStringLiteral("TV Shows") : libraryName;
                    r.isFolder = false;
                    r.formattedSize = QString();
                    r.playUrl = epUrl;
                    r.rawHref = epUrl;
                    r.libraryName = libraryName;
                    r.group = group;
                    r.posterUrl = pagePosterUrl;
                    r.extension = QFileInfo(filename).suffix();
                    epItems.push_back(std::move(r));
                }
            }

            if (!epItems.empty()) {
                const QString webFolderUrl = origin + href;
                if (!pagePosterUrl.isEmpty()) {
                    emit self->posterFound(webFolderUrl, pagePosterUrl, showTitle);
                }

                const QString firstEp = epItems.front().playUrl;
                QUrl u(firstEp);
                const QString epPath = u.path();
                const int ls = epPath.lastIndexOf('/');
                if (ls > 0) {
                    u.setPath(epPath.left(ls + 1));
                    const QString mediaFolderUrl = u.toString(QUrl::FullyEncoded);
                    self->m_mediaToWebFolder[mediaFolderUrl] = webFolderUrl;
                    emit self->seriesMappingFound(mediaFolderUrl, webFolderUrl, pagePosterUrl, showTitle);
                }

                for (const auto &item : epItems) {
                    if (!item.playUrl.isEmpty() && !pagePosterUrl.isEmpty()) {
                        emit self->posterFound(item.playUrl, pagePosterUrl, item.displayName);
                    }
                }

                epItems = postProcess(std::move(epItems));
                self->storeCache(cacheKey, epItems);
                done(true, std::move(epItems));
                return;
            }
            done(false, {});
        });
        return;
    }

    if (href.contains(QLatin1String("category=")) || href.contains(QLatin1String("dashboard.php"))) {
        self->getHtml(origin, href, [self, origin, href, libraryName, group, cacheKey, done](bool ok, const QByteArray &html) {
            if (!self) return;
            if (!ok) {
                done(false, {});
                return;
            }
            QString htmlStr = QString::fromUtf8(html);
            std::vector<SearchResult> catItems;

            static const QRegularExpression cardRx(
                QStringLiteral("<a[^>]*href=[\"']([^\"']*play=(\\d+)[^\"']*)[\"'][^>]*>\\s*<img[^>]*src=[\"']([^\"']+)[\"'][^>]*alt=[\"']([^\"']*)[\"']"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator cardIt = cardRx.globalMatch(htmlStr);

            static const QRegularExpression seriesRegex(
                QStringLiteral("(?:Season|Episode|Completed|S\\d{1,2}E\\d{1,2})"),
                QRegularExpression::CaseInsensitiveOption);

            while (cardIt.hasNext()) {
                auto m = cardIt.next();
                QString itemLink = m.captured(1).trimmed();
                QString itemId = m.captured(2).trimmed();
                QString posterRel = m.captured(3).trimmed();
                QString itemTitle = m.captured(4).trimmed();

                if (itemTitle.isEmpty() || itemId.isEmpty()) continue;

                bool isFolder = seriesRegex.match(itemTitle).hasMatch();

                QString fullPlayUrl = itemLink.startsWith("http://") || itemLink.startsWith("https://")
                                    ? itemLink
                                    : (origin + "/" + (itemLink.startsWith('/') ? itemLink.mid(1) : itemLink));

                QString fullPosterUrl = posterRel.startsWith("http://") || posterRel.startsWith("https://")
                                      ? posterRel
                                      : (origin + "/" + (posterRel.startsWith('/') ? posterRel.mid(1) : posterRel));

                SearchResult r;
                r.displayName = itemTitle;
                r.parentPath = libraryName.isEmpty() ? QStringLiteral("ICC FTP") : libraryName;
                r.isFolder = isFolder;
                r.formattedSize = isFolder ? QStringLiteral("Series") : QString();
                r.playUrl = fullPlayUrl;
                r.rawHref = fullPlayUrl;
                r.libraryName = libraryName;
                r.group = group;
                r.posterUrl = fullPosterUrl;
                r.extension = isFolder ? QString() : QStringLiteral("mp4");
                catItems.push_back(std::move(r));
            }

            if (!catItems.empty()) {
                self->storeCache(cacheKey, catItems);
                done(true, std::move(catItems));
                return;
            }
            done(false, {});
        });
        return;
    }

    // 1) Preferred: the server's JSON API (Dhaka Flix h5ai)
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
    if (!m_currentEpisodes.isEmpty() || m_currentEpisodeIndex != -1 || !m_currentSeasonName.isEmpty()) {
        m_currentEpisodes.clear();
        m_currentEpisodeIndex = -1;
        m_currentSeasonName.clear();
        emit currentEpisodesChanged();
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

    // If we already have items loaded (e.g. browsing the folder), pre-populate immediately
    if (!m_items.empty()) {
        std::vector<const SearchResult *> activeFiles;
        for (const SearchResult &r : m_items) {
            if (!r.isFolder) activeFiles.push_back(&r);
        }
        if (!activeFiles.empty()) {
            int preIdx = -1;
            const QString targetKey = keyHref(currentFileUrl).toLower();
            for (size_t i = 0; i < activeFiles.size(); ++i) {
                if (keyHref(activeFiles[i]->playUrl).toLower() == targetKey
                    || currentFileUrl.endsWith(activeFiles[i]->playUrl)
                    || activeFiles[i]->playUrl == currentFileUrl) {
                    preIdx = static_cast<int>(i);
                    break;
                }
            }
            if (preIdx >= 0) {
                QVariantList epList;
                for (size_t i = 0; i < activeFiles.size(); ++i) {
                    const SearchResult &r = *activeFiles[i];
                    QVariantMap ep;
                    ep[QStringLiteral("name")] = r.displayName;
                    ep[QStringLiteral("url")] = r.playUrl;
                    ep[QStringLiteral("size")] = r.formattedSize;
                    ep[QStringLiteral("isCurrent")] = (static_cast<int>(i) == preIdx);
                    ep[QStringLiteral("index")] = static_cast<int>(i);
                    epList.append(ep);
                }
                m_currentEpisodes = epList;
                m_currentEpisodeIndex = preIdx;
                m_currentSeasonName = folderName();
                emit currentEpisodesChanged();
            }
        }
    }

    QString origin, fileHref;
    if (!splitUrl(currentFileUrl, origin, fileHref)) return;

    QString lookupOrigin = origin;
    QString lookupHref;
    QString parentHref;
    QString fallbackOrigin;
    QString fallbackHref;

    if (fileHref.contains(QLatin1String("player.php"))) {
        lookupOrigin = origin;
        lookupHref = fileHref;
        parentHref = fileHref;
    } else {
        const int slash = fileHref.lastIndexOf('/');
        if (slash < 0) return;
        parentHref = fileHref.left(slash + 1);

        QString webFolder;
        const QString parentFullUrl = origin + parentHref;
        if (m_mediaToWebFolder.contains(parentFullUrl)) {
            webFolder = m_mediaToWebFolder.value(parentFullUrl);
        } else if (m_mediaToWebFolder.contains(currentFileUrl)) {
            webFolder = m_mediaToWebFolder.value(currentFileUrl);
        } else if (m_userStore) {
            webFolder = m_userStore->seriesFolderFor(parentFullUrl);
            if (webFolder.isEmpty()) webFolder = m_userStore->seriesFolderFor(currentFileUrl);
        }

        const QString parentClean = parentHref.trimmed();
        const bool isFlatFolder = (parentClean == QLatin1String("/") || parentClean == QLatin1String("/files/") || parentClean == QLatin1String("/data/"));

        if (isFlatFolder && !webFolder.isEmpty()) {
            QString o, h;
            if (splitUrl(webFolder, o, h)) {
                lookupOrigin = o;
                lookupHref = h;
            } else {
                lookupOrigin = origin;
                lookupHref = parentHref;
            }
        } else {
            // Primary: direct parent folder on the current media server
            lookupOrigin = origin;
            lookupHref = parentHref;

            // Fallback: mapped web series folder on web portal
            if (!webFolder.isEmpty()) {
                QString o, h;
                if (splitUrl(webFolder, o, h)) {
                    fallbackOrigin = o;
                    fallbackHref = h;
                }
            }
        }
    }

    const QString currentKey = keyHref(fileHref).toLower();

    QPointer<FolderBrowser> self(this);
    auto handleResults = [self, token, currentKey, currentFileUrl, parentHref](bool ok, std::vector<SearchResult> items) -> bool {
        if (!self || !ok || token != self->m_nextToken || items.empty()) return false;

        std::vector<const SearchResult *> files;
        QStringList names;
        for (const SearchResult &r : items) {
            if (!r.isFolder) {
                files.push_back(&r);
                names.append(r.displayName);
            }
        }
        if (files.empty()) return false;

        int idx = -1;
        for (size_t i = 0; i < files.size(); ++i) {
            const QString key = QUrl::fromPercentEncoding(files[i]->rawHref.toUtf8()).toLower();
            const QString playKey = keyHref(files[i]->playUrl).toLower();
            if (key == currentKey || playKey == currentKey || currentFileUrl.endsWith(files[i]->playUrl)) {
                idx = static_cast<int>(i);
                break;
            }
        }
        if (idx < 0) {
            const QString curFilename = QUrl(currentFileUrl).fileName().toLower();
            if (!curFilename.isEmpty()) {
                for (size_t i = 0; i < files.size(); ++i) {
                    if (files[i]->displayName.toLower().contains(curFilename)
                        || QUrl(files[i]->playUrl).fileName().toLower() == curFilename
                        || files[i]->displayName.toLower() == curFilename) {
                        idx = static_cast<int>(i);
                        break;
                    }
                }
            }
        }
        if (idx < 0 && !files.empty()) {
            const QString curEpKey = episodeKey(currentFileUrl);
            if (!curEpKey.isEmpty()) {
                for (size_t i = 0; i < files.size(); ++i) {
                    if (episodeKey(files[i]->displayName) == curEpKey
                        || episodeKey(files[i]->playUrl) == curEpKey) {
                        idx = static_cast<int>(i);
                        break;
                    }
                }
            }
            if (idx < 0) {
                idx = 0;
            }
        }

        QVariantList epList;
        for (size_t i = 0; i < files.size(); ++i) {
            const SearchResult &r = *files[i];
            QVariantMap ep;
            ep[QStringLiteral("name")] = r.displayName;
            ep[QStringLiteral("url")] = r.playUrl;
            ep[QStringLiteral("size")] = r.formattedSize;
            ep[QStringLiteral("isCurrent")] = (static_cast<int>(i) == idx);
            ep[QStringLiteral("index")] = static_cast<int>(i);
            epList.append(ep);
        }

        self->m_currentEpisodes = epList;
        self->m_currentEpisodeIndex = idx;

        QString season = parentHref;
        if (season.contains(QLatin1String("player.php")) && !items.empty() && !items.front().group.isEmpty()) {
            season = items.front().group;
        } else {
            if (season.endsWith('/')) season.chop(1);
            const int ls = season.lastIndexOf('/');
            if (ls >= 0) season = season.mid(ls + 1);
            season = QUrl::fromPercentEncoding(season.toUtf8());
            if (season.isEmpty() && !items.empty() && !items.front().group.isEmpty()) {
                season = items.front().group;
            }
        }
        self->m_currentSeasonName = season;
        emit self->currentEpisodesChanged();

        if (idx >= 0) {
            const int nextIdx = pickNextIndex(names, idx);
            if (nextIdx >= 0) {
                const SearchResult &next = *files[static_cast<size_t>(nextIdx)];
                self->m_nextUrl = next.playUrl;
                self->m_nextName = next.displayName;
                emit self->nextChanged();
                FLUX_LOG_INFO("Browser", QString("Next episode resolved: %1").arg(next.displayName));
            }
        }
        return true;
    };

    requestListing(lookupOrigin, lookupHref, QString(), QString(),
                   [self, token, handleResults, fallbackOrigin, fallbackHref](bool ok, std::vector<SearchResult> items) {
        if (!self || token != self->m_nextToken) return;
        if (handleResults(ok, std::move(items))) return;

        if (!fallbackHref.isEmpty() && !fallbackOrigin.isEmpty()) {
            self->requestListing(fallbackOrigin, fallbackHref, QString(), QString(),
                                 [self, token, handleResults](bool ok2, std::vector<SearchResult> items2) {
                if (!self || token != self->m_nextToken) return;
                handleResults(ok2, std::move(items2));
            });
        }
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
    QStringList names;
    QVariantList epList;
    for (int i = 0; i < files.size(); ++i) {
        const QFileInfo &f = files.at(i);
        names.append(f.fileName());
        const bool isCur = (idx < 0 && f.fileName().compare(cur.fileName(), Qt::CaseInsensitive) == 0);
        if (isCur) idx = i;

        QVariantMap ep;
        ep[QStringLiteral("name")] = f.fileName();
        ep[QStringLiteral("url")] = QString::fromLatin1(QUrl::fromLocalFile(f.absoluteFilePath()).toEncoded());
        ep[QStringLiteral("size")] = SearchResult::formatBytes(f.size());
        ep[QStringLiteral("isCurrent")] = isCur;
        ep[QStringLiteral("index")] = i;
        epList.append(ep);
    }

    if (idx >= 0 && idx < epList.size()) {
        QVariantMap curMap = epList[idx].toMap();
        curMap[QStringLiteral("isCurrent")] = true;
        epList[idx] = curMap;
    }

    m_currentEpisodes = epList;
    m_currentEpisodeIndex = idx;
    m_currentSeasonName = cur.dir().dirName();
    emit currentEpisodesChanged();

    if (idx < 0) return;

    const int nextIdx = pickNextIndex(names, idx);
    if (nextIdx < 0) {
        FLUX_LOG_INFO("Browser", QString("Next offline episode: none after '%1'").arg(cur.fileName()));
        return;
    }

    const QFileInfo &next = files.at(nextIdx);
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
