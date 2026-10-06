#include "SearchManager.h"
#include "../core/Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

namespace Flux {

SearchManager::SearchManager(QObject *parent)
    : QAbstractListModel(parent)
    , m_libraryModel(std::make_unique<MediaLibraryModel>(this)) {
    // Default to English Movies 1080p if available, else first item
    int defaultIdx = m_libraryModel->indexOfId("english_movies_1080p");
    if (defaultIdx >= 0) {
        m_selectedLibraryIndex = defaultIdx;
    } else if (m_libraryModel->count() > 0) {
        m_selectedLibraryIndex = 0;
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
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->id : "";
}

QString SearchManager::selectedLibraryName() const {
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->name : "";
}

QString SearchManager::selectedLibraryGroup() const {
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->group : "";
}

QString SearchManager::selectedLibraryUrl() const {
    const MediaRoot *root = m_libraryModel->rootAt(m_selectedLibraryIndex);
    return root ? root->url : "";
}

void SearchManager::selectLibrary(int index) {
    if (index >= 0 && index < m_libraryModel->count() && index != m_selectedLibraryIndex) {
        m_selectedLibraryIndex = index;
        emit selectedLibraryChanged();
        FLUX_LOG_INFO("Search", QString("Selected media library: [%1] %2 (%3)")
                      .arg(selectedLibraryGroup(), selectedLibraryName(), selectedLibraryUrl()));
    }
}

void SearchManager::selectLibraryById(const QString &id) {
    int idx = m_libraryModel->indexOfId(id);
    if (idx >= 0) {
        selectLibrary(idx);
    }
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

    const MediaRoot *activeRoot = m_libraryModel->rootAt(m_selectedLibraryIndex);
    if (!activeRoot) {
        setErrorMessage("No media library selected.");
        return;
    }

    m_query = trimmed;
    emit queryChanged();

    // Abort previous search to prevent race conditions
    cancel();

    const quint64 searchId = ++m_activeSearchId;
    setSearching(true);
    setErrorMessage("");
    setStatusMessage(QString("Searching %1...").arg(activeRoot->name));

    const QString searchUrl = activeRoot->url;
    FLUX_LOG_INFO("Search", QString("[#%1] Searching library \"%2\" for \"%3\" -> %4 (href: %5)")
                  .arg(searchId).arg(activeRoot->name).arg(trimmed).arg(searchUrl).arg(activeRoot->href));

    QNetworkRequest request{QUrl(searchUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=utf-8");

    // Build h5ai search payload for the specific root href
    QJsonObject searchObj;
    searchObj["href"] = activeRoot->href;
    searchObj["pattern"] = trimmed;
    searchObj["ignorecase"] = true;

    QJsonObject rootObj;
    rootObj["action"] = "get";
    rootObj["search"] = searchObj;

    QByteArray payload = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);

    m_activeReply = m_networkManager.post(request, payload);

    MediaRoot rootCopy = *activeRoot;
    connect(m_activeReply, &QNetworkReply::finished, this, [this, searchId, rootCopy]() {
        QNetworkReply *reply = m_activeReply;
        m_activeReply = nullptr;
        onReplyFinished(reply, searchId, rootCopy);
    });
}

void SearchManager::cancel() {
    if (m_activeReply) {
        m_activeReply->abort();
        m_activeReply->deleteLater();
        m_activeReply = nullptr;
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

void SearchManager::onReplyFinished(QNetworkReply *reply, quint64 searchId, MediaRoot activeRoot) {
    if (!reply) return;
    reply->deleteLater();

    // Guard against stale response overwriting newer searches
    if (searchId != m_activeSearchId) {
        FLUX_LOG_INFO("Search", QString("Ignoring stale response from search #%1 (active is #%2)").arg(searchId).arg(m_activeSearchId));
        return;
    }

    setSearching(false);

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return; // Normal cancellation
    }

    if (reply->error() != QNetworkReply::NoError) {
        QString err = QString("Unable to reach library \"%1\".").arg(activeRoot.name);
        setErrorMessage(err);
        setStatusMessage(err);
        FLUX_LOG_ERROR("Search", QString("[#%1] Network error contacting %2 (%3): %4")
                       .arg(searchId).arg(activeRoot.name).arg(reply->error()).arg(reply->errorString()));

        beginResetModel();
        m_results.clear();
        endResetModel();
        emit resultCountChanged();
        return;
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode >= 300) {
        QString err = QString("Library \"%1\" returned HTTP error %2.").arg(activeRoot.name).arg(statusCode);
        setErrorMessage(err);
        setStatusMessage(err);
        FLUX_LOG_ERROR("Search", QString("[#%1] HTTP error from %2: %3").arg(searchId).arg(activeRoot.name).arg(statusCode));

        beginResetModel();
        m_results.clear();
        endResetModel();
        emit resultCountChanged();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        QString err = QString("Invalid response format from library \"%1\".").arg(activeRoot.name);
        setErrorMessage(err);
        setStatusMessage(err);
        FLUX_LOG_ERROR("Search", QString("[#%1] Malformed JSON from %2: %3").arg(searchId).arg(activeRoot.name).arg(parseError.errorString()));

        beginResetModel();
        m_results.clear();
        endResetModel();
        emit resultCountChanged();
        return;
    }

    QJsonObject rootObj = doc.object();
    QJsonArray searchArray = rootObj["search"].toArray();

    std::vector<SearchResult> newResults;
    newResults.reserve(static_cast<size_t>(searchArray.size()));

    for (const auto &val : searchArray) {
        if (!val.isObject()) continue;
        QJsonObject obj = val.toObject();

        QString href = obj["href"].toString();
        if (href.isEmpty()) continue;

        QJsonValue sizeVal = obj["size"];
        bool isSizeNull = sizeVal.isNull() || sizeVal.isUndefined();
        qint64 size = isSizeNull ? -1 : sizeVal.toVariant().toLongLong();

        newResults.push_back(SearchResult::fromJson(href, size, isSizeNull, activeRoot.serverOrigin,
                                                    activeRoot.id, activeRoot.name, activeRoot.group));
    }

    FLUX_LOG_INFO("Search", QString("[#%1] Successfully parsed %2 results in \"%3\" for \"%4\"")
                  .arg(searchId).arg(newResults.size()).arg(activeRoot.name).arg(m_query));

    beginResetModel();
    m_results = std::move(newResults);
    endResetModel();

    if (m_results.empty()) {
        setStatusMessage(QString("No results found in %1 for \"%2\"").arg(activeRoot.name, m_query));
    } else {
        setStatusMessage(QString("%1 results found in %2").arg(m_results.size()).arg(activeRoot.name));
    }

    emit resultCountChanged();
}

void SearchManager::setSearching(bool searching) {
    if (m_isSearching == searching) return;
    m_isSearching = searching;
    emit isSearchingChanged();
}

void SearchManager::setErrorMessage(const QString &msg) {
    if (m_errorMessage == msg) return;
    m_errorMessage = msg;
    emit errorMessageChanged();
}

void SearchManager::setStatusMessage(const QString &msg) {
    if (m_statusMessage == msg) return;
    m_statusMessage = msg;
    emit statusMessageChanged();
}

} // namespace Flux
