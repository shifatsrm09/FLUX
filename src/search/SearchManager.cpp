#include "SearchManager.h"
#include "../core/Logger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

namespace Flux {

SearchManager::SearchManager(QObject *parent)
    : QAbstractListModel(parent) {
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
    case TitleRole:     return item.displayName;
    case PathRole:      return item.parentPath;
    case IsFolderRole:  return item.isFolder;
    case SizeRole:      return item.formattedSize;
    case PlayUrlRole:   return item.playUrl;
    case ExtensionRole: return item.extension;
    case RawHrefRole:   return item.rawHref;
    default:            return QVariant();
    }
}

QHash<int, QByteArray> SearchManager::roleNames() const {
    QHash<int, QByteArray> roles;
    roles[TitleRole]     = "title";
    roles[PathRole]      = "parentPath";
    roles[IsFolderRole]  = "isFolder";
    roles[SizeRole]      = "formattedSize";
    roles[PlayUrlRole]   = "playUrl";
    roles[ExtensionRole] = "extension";
    roles[RawHrefRole]   = "rawHref";
    return roles;
}

void SearchManager::search(const QString &query) {
    QString trimmed = query.trimmed();

    if (trimmed.isEmpty()) {
        clear();
        return;
    }

    m_query = trimmed;
    emit queryChanged();

    // Abort any ongoing search request to prevent race conditions
    cancel();

    const quint64 searchId = ++m_activeSearchId;
    setSearching(true);
    setErrorMessage("");
    setStatusMessage(QString("Searching for \"%1\"...").arg(trimmed));

    const QString searchUrl = "http://172.16.50.14/DHAKA-FLIX-14/";
    FLUX_LOG_INFO("Search", QString("[#%1] Sending search request: \"%2\" -> %3").arg(searchId).arg(trimmed).arg(searchUrl));

    QNetworkRequest request{QUrl(searchUrl)};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=utf-8");

    // Build h5ai search payload
    QJsonObject searchObj;
    searchObj["href"] = "/DHAKA-FLIX-14/";
    searchObj["pattern"] = trimmed;
    searchObj["ignorecase"] = true;

    QJsonObject rootObj;
    rootObj["action"] = "get";
    rootObj["search"] = searchObj;

    QByteArray payload = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);

    m_activeReply = m_networkManager.post(request, payload);

    connect(m_activeReply, &QNetworkReply::finished, this, [this, searchId]() {
        QNetworkReply *reply = m_activeReply;
        m_activeReply = nullptr;
        onReplyFinished(reply, searchId);
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
    }
    return map;
}

void SearchManager::onReplyFinished(QNetworkReply *reply, quint64 searchId) {
    if (!reply) return;
    reply->deleteLater();

    // Guard against stale response overwriting a newer search (Requirement 14)
    if (searchId != m_activeSearchId) {
        FLUX_LOG_INFO("Search", QString("Ignoring stale response from search #%1 (active search is #%2)").arg(searchId).arg(m_activeSearchId));
        return;
    }

    setSearching(false);

    if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::OperationCanceledError) {
        QString err = QString("Network error (%1): %2").arg(reply->error()).arg(reply->errorString());
        setErrorMessage(err);
        setStatusMessage(err);
        FLUX_LOG_ERROR("Search", QString("[#%1] %2").arg(searchId).arg(err));

        beginResetModel();
        m_results.clear();
        endResetModel();
        emit resultCountChanged();
        return;
    }

    if (reply->error() == QNetworkReply::OperationCanceledError) {
        return; // Search was cancelled/superseded
    }

    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode < 200 || statusCode >= 300) {
        QString err = QString("HTTP error %1: %2").arg(statusCode).arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString());
        setErrorMessage(err);
        setStatusMessage(err);
        FLUX_LOG_ERROR("Search", QString("[#%1] %2").arg(searchId).arg(err));

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
        QString err = QString("Malformed JSON response from media server: %1").arg(parseError.errorString());
        setErrorMessage(err);
        setStatusMessage(err);
        FLUX_LOG_ERROR("Search", QString("[#%1] %2").arg(searchId).arg(err));

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

        newResults.push_back(SearchResult::fromJson(href, size, isSizeNull));
    }

    FLUX_LOG_INFO("Search", QString("[#%1] Successfully parsed %2 results for query \"%3\"").arg(searchId).arg(newResults.size()).arg(m_query));

    beginResetModel();
    m_results = std::move(newResults);
    endResetModel();

    if (m_results.empty()) {
        setStatusMessage(QString("No results found for \"%1\"").arg(m_query));
    } else {
        setStatusMessage(QString("Found %1 items for \"%2\"").arg(m_results.size()).arg(m_query));
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
