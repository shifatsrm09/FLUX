#include "SearchManager.h"
#include "../core/Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>
#include <QSet>
#include <algorithm>

namespace Flux {

SearchManager::SearchManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_libraryModel(std::make_unique<MediaLibraryModel>(this)) {
    // Default to All selected
    m_isAllSelected = true;
    m_selectedLibraryIndex = 0;
    int count = m_libraryModel->rowCount();
    m_selectedIndices.reserve(count);
    for (int i = 0; i < count; ++i) {
        m_selectedIndices.push_back(i);
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
    m_pendingReplies = static_cast<int>(targetRoots.size());
    m_accumulatedResults.clear();

    if (m_isAllSelected || m_selectedIndices.empty()) {
        setStatusMessage(QString("Searching all %1 categories for \"%2\"...").arg(targetRoots.size()).arg(trimmed));
    } else if (targetRoots.size() == 1) {
        setStatusMessage(QString("Searching %1 for \"%2\"...").arg(targetRoots[0].name).arg(trimmed));
    } else {
        setStatusMessage(QString("Searching %1 categories for \"%2\"...").arg(targetRoots.size()).arg(trimmed));
    }

    FLUX_LOG_INFO("Search", QString("[#%1] Search for \"%2\" across %3 roots")
                  .arg(searchId).arg(trimmed).arg(targetRoots.size()));

    for (const auto &root : targetRoots) {
        QNetworkRequest request{QUrl(root.url)};
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=utf-8");

        QJsonObject searchObj;
        searchObj["href"] = root.href;
        searchObj["pattern"] = trimmed;
        searchObj["ignorecase"] = true;

        QJsonObject rootObj;
        rootObj["action"] = "get";
        rootObj["search"] = searchObj;

        QByteArray payload = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
        QNetworkReply *reply = m_networkManager.post(request, payload);
        m_activeReplies.push_back(reply);

        MediaRoot rootCopy = root;
        connect(reply, &QNetworkReply::finished, this, [this, reply, searchId, rootCopy]() {
            onSingleReplyFinished(reply, searchId, rootCopy);
        });
    }
}

void SearchManager::cancel() {
    for (auto *reply : m_activeReplies) {
        if (reply) {
            reply->abort();
            reply->deleteLater();
        }
    }
    m_activeReplies.clear();
    m_pendingReplies = 0;
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
    }
    return map;
}

void SearchManager::onSingleReplyFinished(QNetworkReply *reply, quint64 searchId, MediaRoot activeRoot) {
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

                    m_accumulatedResults.push_back(
                        SearchResult::fromJson(href, size, isSizeNull, activeRoot.serverOrigin,
                                               activeRoot.name, activeRoot.group)
                    );
                }
            }
        }
    } else {
        FLUX_LOG_WARN("Search", QString("[#%1] Library \"%2\" error: %3")
                      .arg(searchId).arg(activeRoot.name).arg(reply->errorString()));
    }

    --m_pendingReplies;
    if (m_pendingReplies <= 0) {
        m_pendingReplies = 0;
        setSearching(false);

        // Sort results: folders first, then by title alphabetically
        std::sort(m_accumulatedResults.begin(), m_accumulatedResults.end(), [](const SearchResult &a, const SearchResult &b) {
            if (a.isFolder != b.isFolder) {
                return a.isFolder > b.isFolder;
            }
            return a.displayName.compare(b.displayName, Qt::CaseInsensitive) < 0;
        });

        // Deduplicate results by playUrl
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

        beginResetModel();
        m_results = std::move(m_accumulatedResults);
        endResetModel();

        if (m_results.empty()) {
            setStatusMessage(QString("No results found for \"%1\"").arg(m_query));
        } else {
            setStatusMessage(QString("%1 results found").arg(m_results.size()));
        }

        emit resultCountChanged();
        FLUX_LOG_INFO("Search", QString("[#%1] Multi-search finished with %2 results")
                      .arg(searchId).arg(m_results.size()));
    }
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
