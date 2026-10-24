#include "SearchManager.h"
#include "../core/Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>

namespace Flux {

namespace {

bool isNonPlayableMediaOrGame(const QString &name, const QJsonObject &obj = QJsonObject()) {
    // 1. Check category/type fields in JSON metadata if present
    const QStringList catKeys = {
        QStringLiteral("category"),
        QStringLiteral("cat"),
        QStringLiteral("type"),
        QStringLiteral("group"),
        QStringLiteral("section"),
        QStringLiteral("path")
    };
    for (const auto &key : catKeys) {
        const QString val = obj.value(key).toString().trimmed();
        if (val.contains(QLatin1String("game"), Qt::CaseInsensitive) ||
            val.contains(QLatin1String("software"), Qt::CaseInsensitive) ||
            val.contains(QLatin1String("application"), Qt::CaseInsensitive)) {
            return true;
        }
    }

    // 2. Check release group / scene repack signatures and game-specific naming patterns
    static const QRegularExpression gameRegex(
        QStringLiteral(
            R"((?:(?:^|[\s._-])(?:CODEX|FitGirl|GOG|DODI|SKIDROW|CPY|EMPRESS|PLAZA|FLT|RELOADED|PROPHET|RUNE|TENOKE|Razor1911|ElAmigos|KaOs|TinyRepack)\b|\b(?:PC[\s._-]?Games?|Repacks?|CrackFix|Trainer|ISO[._-]Release)\b))"),
        QRegularExpression::CaseInsensitiveOption);

    if (gameRegex.match(name).hasMatch()) {
        return true;
    }

    // 3. Check for game/installer-specific file extensions
    static const QRegularExpression extRegex(
        QStringLiteral(R"(\.(?:iso|exe|rar|zip|7z|bin|cue|torrent)$)"),
        QRegularExpression::CaseInsensitiveOption);
    if (extRegex.match(name).hasMatch()) {
        return true;
    }

    return false;
}

} // namespace

bool FolderFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const {
    if (!sourceModel()) return false;
    const QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    bool isFolder = sourceModel()->data(idx, SearchManager::IsFolderRole).toBool();
    return isFolder == m_showFolders;
}

SearchManager::SearchManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_libraryModel(std::make_unique<MediaLibraryModel>(this)) {
    m_folderModel = new FolderFilterProxyModel(true, this);
    m_folderModel->setSourceModel(this);

    m_mediaModel = new FolderFilterProxyModel(false, this);
    m_mediaModel->setSourceModel(this);

    m_libraryModel->loadServer(m_activeServer, m_iccSession, m_iccUrl);
    applyDefaultSelection();
    fetchTrending();
}

int SearchManager::folderCount() const {
    int count = 0;
    for (const auto &r : m_results) {
        if (r.isFolder) ++count;
    }
    return count;
}

int SearchManager::mediaCount() const {
    return static_cast<int>(m_results.size()) - folderCount();
}

bool SearchManager::hasFolders() const {
    return folderCount() > 0;
}

bool SearchManager::hasMedia() const {
    return mediaCount() > 0;
}

// Default selection: English Movies only (falls back to All if the id is missing)
void SearchManager::applyDefaultSelection() {
    const int count = m_libraryModel ? m_libraryModel->rowCount() : 0;
    const int defaultIdx = m_libraryModel
        ? m_libraryModel->indexOfId(m_activeServer == 2 ? QStringLiteral("icc_eng_movies")
                                                        : QStringLiteral("english_movies"))
        : -1;

    m_selectedIndices.clear();
    if (defaultIdx >= 0) {
        m_isAllSelected = (count == 1);
        m_selectedLibraryIndex = defaultIdx;
        m_selectedIndices.push_back(defaultIdx);
    } else {
        m_isAllSelected = true;
        m_selectedLibraryIndex = 0;
        m_selectedIndices.reserve(count);
        for (int i = 0; i < count; ++i) {
            m_selectedIndices.push_back(i);
        }
    }
}

SearchManager::~SearchManager() {
    cancel();
}

int SearchManager::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) return 0;
    return static_cast<int>(m_results.size());
}

QVariant SearchManager::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_results.size())) {
        return QVariant();
    }

    const auto &item = m_results[static_cast<size_t>(index.row())];

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

QHash<int, QByteArray> SearchManager::roleNames() const {
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

QString SearchManager::selectedLibraryId() const {
    if (m_isAllSelected) return "all";
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->id : "";
}

QString SearchManager::selectedLibraryName() const {
    if (m_isAllSelected) return "All Categories";
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->name : "";
}

QString SearchManager::selectedLibraryGroup() const {
    if (m_isAllSelected) return "All";
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->group : "";
}

QString SearchManager::selectedLibraryUrl() const {
    if (m_isAllSelected) return "";
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->url : "";
}

void SearchManager::setActiveServer(int server) {
    server = (server == 2) ? 2 : 1;
    if (m_activeServer == server) return;

    const QString prevQuery = m_query;

    m_activeServer = server;
    FLUX_LOG_INFO("Search", QString("Switched to server %1 (%2)")
                  .arg(server).arg(server == 2 ? "ICC FTP" : "Dhaka Flix"));

    // Cancel in-flight requests and clear previous results without erasing m_query
    cancel();
    beginResetModel();
    m_results.clear();
    m_accumulatedResults.clear();
    m_errorMessage.clear();
    m_statusMessage.clear();
    endResetModel();
    ++m_resultsRevision;
    emit resultsRevisionChanged();
    emit resultCountChanged();
    emit errorMessageChanged();
    emit statusMessageChanged();

    m_resolvedUrls.clear();

    if (m_libraryModel) {
        m_libraryModel->loadServer(m_activeServer, m_iccSession, m_iccUrl);
        applyDefaultSelection();   // the app restores the user's saved choice for this server right after
    }

    emit activeServerChanged();
    emit selectedLibrariesChanged();
    emit selectedLibraryChanged();

    if (!prevQuery.trimmed().isEmpty()) {
        search(prevQuery.trimmed());
    }
}

void SearchManager::setIccftpUrl(const QString &url) {
    if (m_iccUrl != url) {
        m_iccUrl = url;
        emit iccftpSettingsChanged();
        // The categories live on the configured server: re-point them (indexes stay the same,
        // so the current selection is kept)
        if (m_activeServer == 2 && m_libraryModel) {
            m_libraryModel->loadServer(m_activeServer, m_iccSession, m_iccUrl);
        }
    }
}

void SearchManager::setIccftpSession(const QString &session) {
    if (m_iccSession != session) {
        m_iccSession = session;
        emit iccftpSettingsChanged();
        if (m_activeServer == 2 && m_libraryModel) {
            m_libraryModel->loadServer(m_activeServer, m_iccSession, m_iccUrl);
        }
    }
}

void SearchManager::resolvePlayUrl(const QString &url, const QJSValue &callback) {
    if (!url.contains(QLatin1String("player.php"))) {
        if (callback.isCallable()) {
            QJSValue cb = callback;
            cb.call(QJSValueList{url});
        }
        return;
    }

    if (m_resolvedUrls.contains(url)) {
        if (callback.isCallable()) {
            QJSValue cb = callback;
            cb.call(QJSValueList{m_resolvedUrls.value(url)});
        }
        return;
    }

    QNetworkRequest req{QUrl(url, QUrl::TolerantMode)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(10000);
    QNetworkReply *reply = m_networkManager.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url, callback]() mutable {
        reply->deleteLater();
        QString directUrl = url;
        if (reply->error() == QNetworkReply::NoError) {
            QString html = QString::fromUtf8(reply->readAll());
            static const QRegularExpression sourceRx(
                QStringLiteral("<source\\s+[^>]*src=['\"]([^'\"]+)['\"]"),
                QRegularExpression::CaseInsensitiveOption);
            auto it = sourceRx.globalMatch(html);
            QString chosenUrl;
            while (it.hasNext()) {
                QString candidate = it.next().captured(1);
                if (chosenUrl.isEmpty()) {
                    chosenUrl = candidate;
                }
                const QString lower = candidate.toLower();
                if ((lower.contains(QLatin1String("1080")) || lower.contains(QLatin1String("2160")) || lower.contains(QLatin1String("4k")))
                        && !lower.contains(QLatin1String("720"))) {
                    chosenUrl = candidate;
                    break;
                }
            }
            if (!chosenUrl.isEmpty()) {
                directUrl = chosenUrl;
                m_resolvedUrls[url] = directUrl;
            } else {
                static const QRegularExpression dlRx(
                    QStringLiteral("<a\\s+[^>]*href=['\"]([^'\"]+\\.(?:mp4|mkv|avi))['\"][^>]*download"),
                    QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch dlMatch = dlRx.match(html);
                if (dlMatch.hasMatch()) {
                    directUrl = dlMatch.captured(1);
                    m_resolvedUrls[url] = directUrl;
                }
            }
        }

        if (callback.isCallable()) {
            callback.call(QJSValueList{directUrl});
        }
    });
}

void SearchManager::fetchTrending() {
    if (m_isTrendingLoading) return;
    m_isTrendingLoading = true;
    emit trendingChanged();

    QUrl baseUrl(m_iccUrl);
    if (!baseUrl.isValid() || baseUrl.host().isEmpty()) {
        baseUrl = QUrl("http://10.16.100.244");
    }
    const QString origin = QString("%1://%2").arg(baseUrl.scheme(), baseUrl.authority());
    const QString sessionParam = m_iccSession.isEmpty()
        ? QStringLiteral("b1bdfdae1f7e206b628274ef7eafadcd6094e07c46cd85b157c595174a369f96")
        : m_iccSession;

    QString dashUrl = QString("%1/dashboard.php?session=%2").arg(origin, sessionParam);
    QNetworkRequest req{QUrl(dashUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
    req.setTransferTimeout(8000);

    auto items = std::make_shared<QVariantList>();
    auto seenIds = std::make_shared<QSet<QString>>();

    static const QRegularExpression cardRx(
        QStringLiteral("<a[^>]*href=[\"']([^\"']*play=(\\d+)[^\"']*)[\"'][^>]*>\\s*<img[^>]*src=[\"']([^\"']+)[\"'][^>]*alt=[\"']([^\"']*)[\"']"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression seriesRegex(
        QStringLiteral("(?:Season|Episode|Completed|S\\d{1,2}E\\d{1,2})"),
        QRegularExpression::CaseInsensitiveOption);

    auto parseCards = [this, origin, sessionParam, items, seenIds](const QString &htmlStr) {
        auto it = cardRx.globalMatch(htmlStr);
        while (it.hasNext()) {
            auto m = it.next();
            QString link = m.captured(1).trimmed();
            QString id = m.captured(2).trimmed();
            QString poster = m.captured(3).trimmed();
            QString title = m.captured(4).trimmed();

            if (id.isEmpty() || title.isEmpty() || seenIds->contains(id)) continue;
            if (isNonPlayableMediaOrGame(title)) continue;
            seenIds->insert(id);

            bool isFolder = seriesRegex.match(title).hasMatch();
            QString fullPlayUrl = link.startsWith("http://") || link.startsWith("https://")
                ? link
                : (origin + "/" + (link.startsWith('/') ? link.mid(1) : link));

            if (!fullPlayUrl.contains("session=") && !sessionParam.isEmpty()) {
                fullPlayUrl += (fullPlayUrl.contains('?') ? "&session=" : "?session=") + sessionParam;
            }

            QString fullPosterUrl = poster.startsWith("http://") || poster.startsWith("https://")
                ? poster
                : (origin + "/" + (poster.startsWith('/') ? poster.mid(1) : poster));

            QVariantMap item;
            item[QStringLiteral("title")] = title;
            item[QStringLiteral("url")] = fullPlayUrl;
            item[QStringLiteral("posterUrl")] = fullPosterUrl;
            item[QStringLiteral("isFolder")] = isFolder;
            item[QStringLiteral("libraryName")] = QStringLiteral("ICC FTP");
            item[QStringLiteral("extraInfo")] = isFolder ? QStringLiteral("Series") : QStringLiteral("Trending");

            items->append(item);
            if (!fullPosterUrl.isEmpty()) {
                emit posterFound(fullPlayUrl, fullPosterUrl, title);
            }
        }
    };

    QNetworkReply *reply = m_networkManager.get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, origin, sessionParam, parseCards, items]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QString html = QString::fromUtf8(reply->readAll());
            parseCards(html);
            m_trending = *items;
            emit trendingChanged();
        }

        // Fetch category 74 (4K Movies) as well for additional trending selections
        QString catUrl = QString("%1/dashboard.php?session=%2&category=74").arg(origin, sessionParam);
        QNetworkRequest catReq{QUrl(catUrl)};
        catReq.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
        catReq.setTransferTimeout(8000);

        QNetworkReply *catReply = m_networkManager.get(catReq);
        connect(catReply, &QNetworkReply::finished, this, [this, catReply, origin, sessionParam, parseCards, items]() {
            catReply->deleteLater();
            if (catReply->error() == QNetworkReply::NoError) {
                QString catHtml = QString::fromUtf8(catReply->readAll());
                parseCards(catHtml);
            }
            m_trending = *items;
            emit trendingChanged();

            // Fetch category 36 (TV & Web Series)
            QString serUrl = QString("%1/dashboard.php?session=%2&category=36").arg(origin, sessionParam);
            QNetworkRequest serReq{QUrl(serUrl)};
            serReq.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0");
            serReq.setTransferTimeout(8000);

            QNetworkReply *serReply = m_networkManager.get(serReq);
            connect(serReply, &QNetworkReply::finished, this, [this, serReply, parseCards, items]() {
                serReply->deleteLater();
                if (serReply->error() == QNetworkReply::NoError) {
                    QString serHtml = QString::fromUtf8(serReply->readAll());
                    parseCards(serHtml);
                }
                m_trending = *items;
                m_isTrendingLoading = false;
                emit trendingChanged();
            });
        });
    });
}

QStringList SearchManager::selectedCategoryIds() const {
    QStringList ids;
    if (!m_libraryModel) return ids;
    ids.reserve(static_cast<qsizetype>(m_selectedIndices.size()));
    for (int idx : m_selectedIndices) {
        const MediaRoot *r = m_libraryModel->rootAt(idx);
        if (r) {
            ids.append(r->id);
        }
    }
    return ids;
}

QList<int> SearchManager::selectedIndices() const {
    QList<int> list;
    list.reserve(static_cast<qsizetype>(m_selectedIndices.size()));
    for (int idx : m_selectedIndices) {
        list.append(idx);
    }
    return list;
}

int SearchManager::selectedCount() const {
    return static_cast<int>(m_selectedIndices.size());
}

QString SearchManager::selectedCategoriesSummary() const {
    if (m_isAllSelected || m_selectedIndices.empty()) {
        return "All Categories";
    }
    if (m_selectedIndices.size() == 1) {
        const MediaRoot *r = m_libraryModel->rootAt(m_selectedIndices.front());
        return r ? r->name : "1 Category";
    }
    return QString("%1 Categories Selected").arg(m_selectedIndices.size());
}

void SearchManager::selectLibrary(int index) {
    if (index >= 0 && index < m_libraryModel->count()) {
        m_selectedLibraryIndex = index;
        m_isAllSelected = false;
        m_selectedIndices = {index};
        emit selectedLibraryChanged();
        emit selectedLibrariesChanged();
        const MediaRoot *r = m_libraryModel->rootAt(index);
        if (r) {
            FLUX_LOG_INFO("Search", QString("Selected media library: [%1] %2 (%3)")
                          .arg(r->group, r->name, r->url));
        }
    }
}

void SearchManager::selectLibraryById(const QString &id) {
    if (id == "all") {
        setAllSelected(true);
        return;
    }
    int idx = m_libraryModel->indexOfId(id);
    if (idx >= 0) {
        selectLibrary(idx);
    }
}

void SearchManager::setAllSelected(bool all) {
    m_isAllSelected = all;
    m_selectedIndices.clear();
    if (all && m_libraryModel) {
        int count = m_libraryModel->rowCount();
        m_selectedIndices.reserve(count);
        for (int i = 0; i < count; ++i) {
            m_selectedIndices.push_back(i);
        }
    }
    emit selectedLibrariesChanged();
    emit selectedLibraryChanged();
    FLUX_LOG_INFO("Search", QString("All categories set to %1 (selected count: %2)")
                  .arg(all ? "true" : "false").arg(m_selectedIndices.size()));
}

void SearchManager::toggleAll() {
    setAllSelected(!m_isAllSelected);
}

void SearchManager::toggleLibrary(int index) {
    if (!m_libraryModel || index < 0 || index >= m_libraryModel->count()) return;

    auto it = std::find(m_selectedIndices.begin(), m_selectedIndices.end(), index);
    if (it != m_selectedIndices.end()) {
        m_selectedIndices.erase(it);
    } else {
        m_selectedIndices.push_back(index);
    }

    int totalCount = m_libraryModel->count();
    m_isAllSelected = (totalCount > 0 && static_cast<int>(m_selectedIndices.size()) == totalCount);

    if (!m_selectedIndices.empty()) {
        m_selectedLibraryIndex = m_selectedIndices.front();
    }

    emit selectedLibrariesChanged();
    emit selectedLibraryChanged();
    FLUX_LOG_INFO("Search", QString("Toggled library index %1 (selected count: %2/%3, isAll: %4)")
                  .arg(index).arg(m_selectedIndices.size()).arg(totalCount).arg(m_isAllSelected ? "true" : "false"));
}

void SearchManager::toggleCategory(const QString &id) {
    if (!m_libraryModel) return;
    int idx = m_libraryModel->indexOfId(id);
    if (idx >= 0) {
        toggleLibrary(idx);
    }
}

bool SearchManager::isLibrarySelected(int index) const {
    return std::find(m_selectedIndices.begin(), m_selectedIndices.end(), index) != m_selectedIndices.end();
}

bool SearchManager::isCategorySelected(const QString &id) const {
    if (!m_libraryModel) return false;
    int idx = m_libraryModel->indexOfId(id);
    if (idx >= 0) {
        return isLibrarySelected(idx);
    }
    return false;
}

void SearchManager::selectOnlyLibrary(int index) {
    selectLibrary(index);
}

void SearchManager::clearCategorySelection() {
    setAllSelected(false);
}

void SearchManager::selectAllCategories() {
    setAllSelected(true);
}

void SearchManager::restoreSelection(const QStringList &ids) {
    if (!m_libraryModel) return;

    std::vector<int> restored;
    for (const QString &id : ids) {
        int idx = m_libraryModel->indexOfId(id);
        if (idx >= 0 && std::find(restored.begin(), restored.end(), idx) == restored.end()) {
            restored.push_back(idx);
        }
    }
    if (restored.empty()) return;   // nothing valid saved: keep the built-in default

    m_selectedIndices = restored;
    m_selectedLibraryIndex = restored.front();
    m_isAllSelected = (static_cast<int>(restored.size()) == m_libraryModel->count());

    emit selectedLibrariesChanged();
    emit selectedLibraryChanged();
    FLUX_LOG_INFO("Search", QString("Restored %1 saved categories").arg(restored.size()));
}

void SearchManager::searchInLibrary(const QString &query, int libraryIndex) {
    selectLibrary(libraryIndex);
    search(query);
}

void SearchManager::search(const QString &query) {
    QString trimmed = query.trimmed();

    if (trimmed.isEmpty()) {
        clear();
        return;
    }

    if (m_activeServer == 2) {
        m_query = trimmed;
        emit queryChanged();

        cancel();

        const quint64 searchId = ++m_activeSearchId;
        setSearching(true);
        setErrorMessage("");
        m_accumulatedResults.clear();
        m_plan = SearchQuery::plan(trimmed, 6);

        setStatusMessage(QString("Searching ICC FTP for \"%1\"...").arg(trimmed));
        FLUX_LOG_INFO("Search", QString("[#%1] ICC FTP search for \"%2\"").arg(searchId).arg(trimmed));

        startIccSearch(searchId, trimmed);
        return;
    }

    std::vector<MediaRoot> targetRoots;
    if (m_isAllSelected) {
        int count = m_libraryModel ? m_libraryModel->rowCount() : 0;
        targetRoots.reserve(count);
        for (int i = 0; i < count; ++i) {
            const MediaRoot *r = m_libraryModel->rootAt(i);
            if (r) targetRoots.push_back(*r);
        }
    } else {
        targetRoots.reserve(m_selectedIndices.size());
        for (int idx : m_selectedIndices) {
            const MediaRoot *r = m_libraryModel->rootAt(idx);
            if (r) targetRoots.push_back(*r);
        }
        // Fallback: If nothing was selected, search all
        if (targetRoots.empty() && m_libraryModel) {
            int count = m_libraryModel->rowCount();
            for (int i = 0; i < count; ++i) {
                const MediaRoot *r = m_libraryModel->rootAt(i);
                if (r) targetRoots.push_back(*r);
            }
        }
    }

    if (targetRoots.empty()) {
        setErrorMessage("No media library categories available to search.");
        return;
    }

    m_query = trimmed;
    emit queryChanged();

    // Abort previous search to prevent race conditions
    cancel();

    const quint64 searchId = ++m_activeSearchId;
    setSearching(true);
    setErrorMessage("");
    m_accumulatedResults.clear();

    m_targetRoots = std::move(targetRoots);
    // Fewer query variants when many categories are searched at once, to go easy on the server
    m_plan = SearchQuery::plan(trimmed, m_targetRoots.size() > 8 ? 3 : 6);
    m_stage = 1;

    if (m_isAllSelected || m_selectedIndices.empty()) {
        setStatusMessage(QString("Searching all %1 categories for \"%2\"...").arg(m_targetRoots.size()).arg(trimmed));
    } else if (m_targetRoots.size() == 1) {
        setStatusMessage(QString("Searching %1 for \"%2\"...").arg(m_targetRoots[0].name).arg(trimmed));
    } else {
        setStatusMessage(QString("Searching %1 categories for \"%2\"...").arg(m_targetRoots.size()).arg(trimmed));
    }

    FLUX_LOG_INFO("Search", QString("[#%1] Search for \"%2\" across %3 roots; literal queries: [%4], anchors: [%5]")
                  .arg(searchId).arg(trimmed).arg(m_targetRoots.size())
                  .arg(m_plan.literalQueries.join(" | "), m_plan.anchorQueries.join(" | ")));

    startStage(searchId, m_plan.literalQueries);
}

void SearchManager::startStage(quint64 searchId, const QStringList &queries) {
    m_stage = (&queries == &m_plan.anchorQueries) ? 2 : 1;
    m_pendingReplies = static_cast<int>(m_targetRoots.size()) * static_cast<int>(queries.size());
    if (m_pendingReplies <= 0) {
        m_pendingReplies = 0;
        finishSearch(searchId);
        return;
    }

    const int stage = m_stage;
    for (const auto &root : m_targetRoots) {
        for (const QString &pattern : queries) {
            QNetworkRequest request{QUrl(root.url)};
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=utf-8");

            QJsonObject searchObj;
            searchObj["href"] = root.href;
            searchObj["pattern"] = pattern;
            searchObj["ignorecase"] = true;

            QJsonObject rootObj;
            rootObj["action"] = "get";
            rootObj["search"] = searchObj;

            QByteArray payload = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
            QNetworkReply *reply = m_networkManager.post(request, payload);
            m_activeReplies.push_back(reply);

            const MediaRoot rootCopy = root;
            connect(reply, &QNetworkReply::finished, this, [this, reply, searchId, rootCopy, stage]() {
                onReplyFinished(reply, searchId, rootCopy, stage);
            });
        }
    }
}

void SearchManager::startIccSearch(quint64 searchId, const QString &query) {
    QUrl baseUrl(m_iccUrl);
    if (!baseUrl.isValid() || baseUrl.host().isEmpty()) {
        baseUrl = QUrl("http://10.16.100.244");
    }
    QString commandUrl = QString("%1://%2/command.php").arg(baseUrl.scheme(), baseUrl.authority());

    QNetworkRequest request{QUrl(commandUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded; charset=UTF-8");

    QUrlQuery params;
    params.addQueryItem("cSearch", query);
    QByteArray payload = params.toString(QUrl::FullyEncoded).toUtf8();

    m_pendingReplies = 1;
    QNetworkReply *reply = m_networkManager.post(request, payload);
    m_activeReplies.push_back(reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, searchId, baseUrl]() {
        reply->deleteLater();
        auto it = std::find(m_activeReplies.begin(), m_activeReplies.end(), reply);
        if (it != m_activeReplies.end()) {
            m_activeReplies.erase(it);
        }

        if (searchId != m_activeSearchId) return;

        if (reply->error() != QNetworkReply::NoError) {
            setErrorMessage(QString("ICC search error: %1").arg(reply->errorString()));
            finishSearch(searchId);
            return;
        }

        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isArray()) {
            FLUX_LOG_WARN("Search", "ICC search response is not a JSON array");
            finishSearch(searchId);
            return;
        }

        QJsonArray arr = doc.array();
        static const QRegularExpression seriesRegex(
            QStringLiteral("(?:Season|Episode|Completed|S\\d{1,2}E\\d{1,2})"),
            QRegularExpression::CaseInsensitiveOption);

        for (const auto &val : arr) {
            if (!val.isObject()) continue;
            QJsonObject obj = val.toObject();
            QString id = obj["id"].toString();
            QString name = obj["name"].toString().trimmed();
            QString image = obj["image"].toString().trimmed();
            if (id.isEmpty() || name.isEmpty()) continue;
            if (isNonPlayableMediaOrGame(name, obj)) continue;

            bool isFolder = seriesRegex.match(name).hasMatch();

            QString playUrl = QString("%1://%2/player.php?play=%3").arg(baseUrl.scheme(), baseUrl.authority(), id);
            if (!m_iccSession.isEmpty()) {
                playUrl += "&session=" + m_iccSession;
            }

            QString posterUrl;
            if (!image.isEmpty()) {
                posterUrl = QString("%1://%2/files/%3").arg(baseUrl.scheme(), baseUrl.authority(), image);
            }

            SearchResult item;
            item.displayName = name;
            item.parentPath = isFolder ? QStringLiteral("TV Shows") : QStringLiteral("Movies");
            item.isFolder = isFolder;
            item.formattedSize = isFolder ? QStringLiteral("Series") : QString();
            item.playUrl = playUrl;
            item.extension = isFolder ? QString() : QStringLiteral("mp4");
            item.rawHref = playUrl;
            item.libraryName = QStringLiteral("ICC FTP");
            item.group = isFolder ? QStringLiteral("TV & Web Series") : QStringLiteral("English Movies");
            item.posterUrl = posterUrl;

            if (!posterUrl.isEmpty()) {
                emit posterFound(playUrl, posterUrl, name);
            }

            int score = SearchQuery::score(name, m_plan);
            item.matchScore = score > 0 ? score : 2;

            m_accumulatedResults.push_back(std::move(item));
        }

        finishSearch(searchId);

        // Pre-resolve top movie direct links in background
        for (size_t i = 0; i < std::min<size_t>(m_results.size(), 8); ++i) {
            if (!m_results[i].isFolder) {
                resolveDirectUrl(m_results[i].playUrl, static_cast<int>(i));
            }
        }
    });
}

void SearchManager::resolveDirectUrl(const QString &playUrl, int resultIndex) {
    if (playUrl.isEmpty() || !playUrl.contains(QLatin1String("player.php"))) return;
    if (m_resolvedUrls.contains(playUrl)) return;

    QNetworkRequest req{QUrl(playUrl, QUrl::TolerantMode)};
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(8000);
    QNetworkReply *reply = m_networkManager.get(req);

    connect(reply, &QNetworkReply::finished, this, [this, reply, playUrl, resultIndex]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QString html = QString::fromUtf8(reply->readAll());
            static const QRegularExpression sourceRx(
                QStringLiteral("<source\\s+[^>]*src=['\"]([^'\"]+)['\"]"),
                QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch match = sourceRx.match(html);
            if (match.hasMatch()) {
                QString directUrl = match.captured(1);
                m_resolvedUrls[playUrl] = directUrl;

                static const QRegularExpression sizeRx(
                    QStringLiteral("<strong>DOWNLOAD</strong>\\s*([0-9.]+\\s*[KMGTP]?B)"),
                    QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch sizeMatch = sizeRx.match(html);
                if (sizeMatch.hasMatch() && resultIndex >= 0 && resultIndex < static_cast<int>(m_results.size())) {
                    if (m_results[resultIndex].playUrl == playUrl) {
                        m_results[resultIndex].formattedSize = sizeMatch.captured(1).trimmed();
                        QModelIndex idx = index(resultIndex, 0);
                        emit dataChanged(idx, idx, {SizeRole});
                    }
                }
            }
        }
    });
}

void SearchManager::cancel() {
    // Take the list first: aborting a reply can emit finished() right away
    std::vector<QNetworkReply*> replies;
    replies.swap(m_activeReplies);
    m_pendingReplies = 0;

    for (auto *reply : replies) {
        if (reply) {
            reply->disconnect(this);   // no more callbacks from this reply
            reply->abort();
            reply->deleteLater();
        }
    }
}

void SearchManager::clear() {
    cancel();
    m_query.clear();
    setSearching(false);
    setErrorMessage("");
    setStatusMessage("");

    beginResetModel();
    m_results.clear();
    endResetModel();

    ++m_resultsRevision;
    emit resultsRevisionChanged();
    emit queryChanged();
    emit resultCountChanged();
}

QVariantMap SearchManager::getResult(int index) const {
    QVariantMap map;
    if (index >= 0 && index < static_cast<int>(m_results.size())) {
        const auto &item = m_results[static_cast<size_t>(index)];
        map["title"]         = item.displayName;
        map["parentPath"]    = item.parentPath;
        map["isFolder"]      = item.isFolder;
        map["formattedSize"] = item.formattedSize;
        map["playUrl"]       = item.playUrl;
        map["extension"]     = item.extension;
        map["rawHref"]       = item.rawHref;
        map["libraryName"]   = item.libraryName;
        map["group"]         = item.group;
        map["posterUrl"]     = item.posterUrl;
    }
    return map;
}

QVariantList SearchManager::getSectionedGridRows(int columns) const {
    QVariantList rows;
    if (m_results.empty()) return rows;
    columns = std::max(1, columns);

    std::vector<int> folderIndices;
    std::vector<int> mediaIndices;
    folderIndices.reserve(m_results.size());
    mediaIndices.reserve(m_results.size());

    for (int i = 0; i < static_cast<int>(m_results.size()); ++i) {
        if (m_results[static_cast<size_t>(i)].isFolder) {
            folderIndices.push_back(i);
        } else {
            mediaIndices.push_back(i);
        }
    }

    // 1. Folders section
    if (!folderIndices.empty()) {
        QVariantMap header;
        header["isHeader"] = true;
        header["isFolder"] = true;
        header["title"] = QStringLiteral("Folders & Series");
        header["icon"] = QStringLiteral("folder");
        const int fCount = static_cast<int>(folderIndices.size());
        header["countText"] = QString("%1 %2").arg(fCount).arg(fCount == 1 ? "folder" : "folders");
        rows.append(header);

        for (size_t i = 0; i < folderIndices.size(); i += static_cast<size_t>(columns)) {
            QVariantMap row;
            row["isHeader"] = false;
            row["isFolder"] = true;
            QVariantList items;
            for (size_t c = 0; c < static_cast<size_t>(columns) && (i + c) < folderIndices.size(); ++c) {
                items.append(getResult(folderIndices[i + c]));
            }
            row["items"] = items;
            rows.append(row);
        }
    }

    // 2. Media section
    if (!mediaIndices.empty()) {
        QVariantMap header;
        header["isHeader"] = true;
        header["isFolder"] = false;
        header["title"] = QStringLiteral("Movies & Episodes");
        header["icon"] = QStringLiteral("play");
        const int mCount = static_cast<int>(mediaIndices.size());
        header["countText"] = QString("%1 %2").arg(mCount).arg(mCount == 1 ? "item" : "items");
        rows.append(header);

        for (size_t i = 0; i < mediaIndices.size(); i += static_cast<size_t>(columns)) {
            QVariantMap row;
            row["isHeader"] = false;
            row["isFolder"] = false;
            QVariantList items;
            for (size_t c = 0; c < static_cast<size_t>(columns) && (i + c) < mediaIndices.size(); ++c) {
                items.append(getResult(mediaIndices[i + c]));
            }
            row["items"] = items;
            rows.append(row);
        }
    }

    return rows;
}

namespace {

// Text a result is scored on: the file/folder name plus the two folders above it, so a file
// inside "Spider-Man (2002)/" still counts as a hit for "spiderman"
QString scoringText(const SearchResult &r) {
    const QStringList parts = r.parentPath.split('/', Qt::SkipEmptyParts);
    QString text;
    for (qsizetype i = std::max<qsizetype>(0, parts.size() - 2); i < parts.size(); ++i) {
        text += parts.at(i) + QLatin1Char(' ');
    }
    return text + r.displayName;
}

// If the literal queries found fewer unique results than this, also try the looser "anchor" pass
constexpr int kBroadenBelow = 12;

} // namespace

void SearchManager::onReplyFinished(QNetworkReply *reply, quint64 searchId, const MediaRoot &activeRoot, int stage) {
    if (!reply) return;
    reply->deleteLater();

    // Remove from active replies
    auto it = std::find(m_activeReplies.begin(), m_activeReplies.end(), reply);
    if (it != m_activeReplies.end()) {
        m_activeReplies.erase(it);
    }

    if (searchId != m_activeSearchId) {
        // Stale response
        return;
    }

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return;
    }

    if (reply->error() == QNetworkReply::NoError) {
        int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (statusCode >= 200 && statusCode < 300) {
            QByteArray responseData = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(responseData);
            if (doc.isObject()) {
                QJsonObject rootObj = doc.object();
                QJsonArray searchArray = rootObj["search"].toArray();

                for (const auto &val : searchArray) {
                    if (!val.isObject()) continue;
                    QJsonObject obj = val.toObject();

                    QString href = obj["href"].toString();
                    if (href.isEmpty()) continue;

                    QJsonValue sizeVal = obj["size"];
                    bool isSizeNull = sizeVal.isNull() || sizeVal.isUndefined();
                    qint64 size = isSizeNull ? -1 : sizeVal.toVariant().toLongLong();

                    SearchResult item = SearchResult::fromJson(href, size, isSizeNull, activeRoot.serverOrigin,
                                                               activeRoot.id, activeRoot.name, activeRoot.group);

                    const int score = SearchQuery::score(scoringText(item), m_plan);
                    if (stage == 1) {
                        // The server matched this literally; keep it even if our scorer disagrees
                        item.matchScore = score > 0 ? score : 2;
                    } else {
                        // Loose candidates from the anchor pass: keep only the ones that really match
                        if (score == 0) continue;
                        item.matchScore = score;
                    }
                    m_accumulatedResults.push_back(std::move(item));
                }
            }
        }
    } else {
        FLUX_LOG_WARN("Search", QString("[#%1] Library \"%2\" error: %3")
                      .arg(searchId).arg(activeRoot.name).arg(reply->errorString()));
    }

    --m_pendingReplies;
    if (m_pendingReplies > 0) return;
    m_pendingReplies = 0;

    // Literal queries found little: look for close matches ("spiderman" -> "Spider-Man", typos)
    if (stage == 1 && !m_plan.anchorQueries.isEmpty()) {
        QSet<QString> unique;
        for (const auto &r : m_accumulatedResults) unique.insert(r.playUrl);

        if (unique.size() < kBroadenBelow) {
            setStatusMessage(QString("Looking for close matches to \"%1\"...").arg(m_query));
            FLUX_LOG_INFO("Search", QString("[#%1] Only %2 literal hits; trying anchors [%3]")
                          .arg(searchId).arg(unique.size()).arg(m_plan.anchorQueries.join(" | ")));
            startStage(searchId, m_plan.anchorQueries);
            return;
        }
    }

    finishSearch(searchId);
}

void SearchManager::finishSearch(quint64 searchId) {
    setSearching(false);

    // Folders first (a show's folder before its loose episodes), then best matches, then alphabetical
    std::sort(m_accumulatedResults.begin(), m_accumulatedResults.end(), [](const SearchResult &a, const SearchResult &b) {
        if (a.isFolder != b.isFolder) {
            return a.isFolder > b.isFolder;
        }
        if (a.matchScore != b.matchScore) {
            return a.matchScore > b.matchScore;
        }
        return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
    });

    // Deduplicate results by playUrl (the best-scoring copy comes first after the sort)
    QSet<QString> seenUrls;
    std::vector<SearchResult> uniqueResults;
    uniqueResults.reserve(m_accumulatedResults.size());
    for (auto &item : m_accumulatedResults) {
        if (item.playUrl.isEmpty() || !seenUrls.contains(item.playUrl)) {
            if (!item.playUrl.isEmpty()) {
                seenUrls.insert(item.playUrl);
            }
            uniqueResults.push_back(std::move(item));
        }
    }
    m_accumulatedResults = std::move(uniqueResults);

    bool hasCloseMatches = false;
    for (const auto &r : m_accumulatedResults) {
        if (r.matchScore < 3) { hasCloseMatches = true; break; }
    }

    beginResetModel();
    m_results = std::move(m_accumulatedResults);
    endResetModel();
    m_accumulatedResults.clear();

    if (m_results.empty()) {
        setStatusMessage(QString("No results found for \"%1\"").arg(m_query));
    } else if (hasCloseMatches) {
        setStatusMessage(QString("%1 results found (some are close matches)").arg(m_results.size()));
    } else {
        setStatusMessage(QString("%1 results found").arg(m_results.size()));
    }

    ++m_resultsRevision;
    emit resultsRevisionChanged();
    emit resultCountChanged();
    FLUX_LOG_INFO("Search", QString("[#%1] Multi-search finished with %2 results")
                  .arg(searchId).arg(m_results.size()));
}

void SearchManager::setSearching(bool searching) {
    if (m_isSearching != searching) {
        m_isSearching = searching;
        emit isSearchingChanged();
    }
}

void SearchManager::setErrorMessage(const QString &msg) {
    if (m_errorMessage != msg) {
        m_errorMessage = msg;
        emit errorMessageChanged();
    }
}

void SearchManager::setStatusMessage(const QString &msg) {
    if (m_statusMessage != msg) {
        m_statusMessage = msg;
        emit statusMessageChanged();
    }
}

} // namespace Flux
