#pragma once

#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QList>
#include <QHash>
#include <QJSValue>
#include <memory>
#include <vector>
#include "SearchResult.h"
#include "SearchQuery.h"
#include "MediaLibrary.h"

namespace Flux {

class SearchManager;

class FolderFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit FolderFilterProxyModel(bool showFolders, QObject *parent = nullptr)
        : QSortFilterProxyModel(parent), m_showFolders(showFolders) {}

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool m_showFolders = false;
};

class SearchManager : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(MediaLibraryModel* libraryModel READ libraryModel CONSTANT)
    Q_PROPERTY(int selectedLibraryIndex READ selectedLibraryIndex WRITE selectLibrary NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryId READ selectedLibraryId NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryName READ selectedLibraryName NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryGroup READ selectedLibraryGroup NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryUrl READ selectedLibraryUrl NOTIFY selectedLibraryChanged)

    // Multi-server properties (1 = Dhaka Flix, 2 = ICC FTP)
    Q_PROPERTY(int activeServer READ activeServer WRITE setActiveServer NOTIFY activeServerChanged)
    Q_PROPERTY(QString iccftpUrl READ iccftpUrl WRITE setIccftpUrl NOTIFY iccftpSettingsChanged)
    Q_PROPERTY(QString iccftpSession READ iccftpSession WRITE setIccftpSession NOTIFY iccftpSettingsChanged)

    // Multi-category / Filter properties
    Q_PROPERTY(bool isAllSelected READ isAllSelected WRITE setAllSelected NOTIFY selectedLibrariesChanged)
    Q_PROPERTY(QStringList selectedCategoryIds READ selectedCategoryIds NOTIFY selectedLibrariesChanged)
    Q_PROPERTY(QList<int> selectedIndices READ selectedIndices NOTIFY selectedLibrariesChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectedLibrariesChanged)
    Q_PROPERTY(QString selectedCategoriesSummary READ selectedCategoriesSummary NOTIFY selectedLibrariesChanged)

    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY isSearchingChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY resultCountChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY errorMessageChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(int resultsRevision READ resultsRevision NOTIFY resultsRevisionChanged)
    Q_PROPERTY(QAbstractItemModel* folderModel READ folderModel CONSTANT)
    Q_PROPERTY(QAbstractItemModel* mediaModel READ mediaModel CONSTANT)
    Q_PROPERTY(int folderCount READ folderCount NOTIFY resultCountChanged)
    Q_PROPERTY(int mediaCount READ mediaCount NOTIFY resultCountChanged)
    Q_PROPERTY(bool hasFolders READ hasFolders NOTIFY resultCountChanged)
    Q_PROPERTY(bool hasMedia READ hasMedia NOTIFY resultCountChanged)

    // Trending media from ICC FTP
    Q_PROPERTY(QVariantList trending READ trending NOTIFY trendingChanged)
    Q_PROPERTY(bool isTrendingLoading READ isTrendingLoading NOTIFY trendingChanged)

public:
    enum SearchRoles {
        TitleRole = Qt::UserRole + 1,
        PathRole,
        IsFolderRole,
        SizeRole,
        PlayUrlRole,
        ExtensionRole,
        RawHrefRole,
        LibraryNameRole,
        GroupRole,
        PosterUrlRole
    };

    explicit SearchManager(QObject *parent = nullptr);
    ~SearchManager() override;

    // QAbstractListModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Library Model & Selection
    MediaLibraryModel* libraryModel() const { return m_libraryModel.get(); }
    int selectedLibraryIndex() const { return m_selectedLibraryIndex; }
    QString selectedLibraryId() const;
    QString selectedLibraryName() const;
    QString selectedLibraryGroup() const;
    QString selectedLibraryUrl() const;

    // Multi-category / Filter methods
    bool isAllSelected() const { return m_isAllSelected; }
    QStringList selectedCategoryIds() const;
    QList<int> selectedIndices() const;
    int selectedCount() const;
    QString selectedCategoriesSummary() const;

    // Search Properties
    QString query() const { return m_query; }
    bool isSearching() const { return m_isSearching; }
    int resultCount() const { return static_cast<int>(m_results.size()); }
    bool hasResults() const { return !m_results.empty(); }
    QString errorMessage() const { return m_errorMessage; }
    bool hasError() const { return !m_errorMessage.isEmpty(); }
    QString statusMessage() const { return m_statusMessage; }

    // Sectioned results methods
    int resultsRevision() const { return m_resultsRevision; }
    QAbstractItemModel* folderModel() const { return reinterpret_cast<QAbstractItemModel*>(m_folderModel); }
    QAbstractItemModel* mediaModel() const { return reinterpret_cast<QAbstractItemModel*>(m_mediaModel); }
    int folderCount() const;
    int mediaCount() const;
    bool hasFolders() const;
    bool hasMedia() const;
    Q_INVOKABLE QVariantList getSectionedGridRows(int columns) const;

    // Server properties & methods
    int activeServer() const { return m_activeServer; }
    void setActiveServer(int server);
    QString iccftpUrl() const { return m_iccUrl; }
    void setIccftpUrl(const QString &url);
    QString iccftpSession() const { return m_iccSession; }
    void setIccftpSession(const QString &session);
    Q_INVOKABLE void resolvePlayUrl(const QString &url, const QJSValue &callback);

    // QML-invokable methods
    Q_INVOKABLE void selectLibrary(int index);
    Q_INVOKABLE void selectLibraryById(const QString &id);
    Q_INVOKABLE void setAllSelected(bool all);
    Q_INVOKABLE void toggleAll();
    Q_INVOKABLE void toggleLibrary(int index);
    Q_INVOKABLE void toggleCategory(const QString &id);
    Q_INVOKABLE bool isLibrarySelected(int index) const;
    Q_INVOKABLE bool isCategorySelected(const QString &id) const;
    Q_INVOKABLE void selectOnlyLibrary(int index);
    Q_INVOKABLE void clearCategorySelection();
    Q_INVOKABLE void selectAllCategories();
    Q_INVOKABLE void restoreSelection(const QStringList &ids);

    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void searchInLibrary(const QString &query, int libraryIndex);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QVariantMap getResult(int index) const;

    // Trending media
    QVariantList trending() const { return m_trending; }
    bool isTrendingLoading() const { return m_isTrendingLoading; }
    Q_INVOKABLE void fetchTrending();

signals:
    void activeServerChanged();
    void iccftpSettingsChanged();
    void selectedLibraryChanged();
    void selectedLibrariesChanged();
    void queryChanged();
    void isSearchingChanged();
    void resultCountChanged();
    void resultsRevisionChanged();
    void errorMessageChanged();
    void statusMessageChanged();
    void posterFound(const QString &url, const QString &posterUrl, const QString &title);
    void trendingChanged();

private:
    void setSearching(bool searching);
    void setErrorMessage(const QString &msg);
    void setStatusMessage(const QString &msg);
    void applyDefaultSelection();   // built-in default category for the active server

    // Two-stage search (see SearchQuery): stage 1 sends the literal queries; if that finds few
    // results, stage 2 sends short "anchor" chunks and keeps only candidates that score well.
    void startStage(quint64 searchId, const QStringList &queries);
    void startIccSearch(quint64 searchId, const QString &query);
    void resolveDirectUrl(const QString &playUrl, int resultIndex);
    void onReplyFinished(QNetworkReply *reply, quint64 searchId, const MediaRoot &root, int stage);
    void finishSearch(quint64 searchId);

    std::unique_ptr<MediaLibraryModel> m_libraryModel;
    int m_selectedLibraryIndex = 0;

    int m_activeServer = 2; // Default to Server 2 (ICC FTP)
    QString m_iccUrl = QStringLiteral("http://10.16.100.244/dashboard.php?session=b1bdfdae1f7e206b628274ef7eafadcd6094e07c46cd85b157c595174a369f96");
    QString m_iccSession = QStringLiteral("b1bdfdae1f7e206b628274ef7eafadcd6094e07c46cd85b157c595174a369f96");
    QHash<QString, QString> m_resolvedUrls;

    bool m_isAllSelected = true;
    std::vector<int> m_selectedIndices;

    QNetworkAccessManager m_networkManager;
    std::vector<QNetworkReply*> m_activeReplies;
    quint64 m_activeSearchId = 0;
    int m_pendingReplies = 0;
    std::vector<SearchResult> m_accumulatedResults;

    SearchQuery::Plan m_plan;
    std::vector<MediaRoot> m_targetRoots;
    int m_stage = 1;

    QString m_query;
    bool m_isSearching = false;
    QString m_errorMessage;
    QString m_statusMessage;

    std::vector<SearchResult> m_results;

    QVariantList m_trending;
    bool m_isTrendingLoading = false;

    FolderFilterProxyModel *m_folderModel = nullptr;
    FolderFilterProxyModel *m_mediaModel = nullptr;
    int m_resultsRevision = 0;
};

} // namespace Flux
