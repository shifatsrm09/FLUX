#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <memory>
#include <vector>
#include "SearchResult.h"
#include "MediaLibrary.h"

namespace Flux {

class SearchManager : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(MediaLibraryModel* libraryModel READ libraryModel CONSTANT)
    Q_PROPERTY(int selectedLibraryIndex READ selectedLibraryIndex WRITE selectLibrary NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryId READ selectedLibraryId NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryName READ selectedLibraryName NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryGroup READ selectedLibraryGroup NOTIFY selectedLibraryChanged)
    Q_PROPERTY(QString selectedLibraryUrl READ selectedLibraryUrl NOTIFY selectedLibraryChanged)

    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(bool isSearching READ isSearching NOTIFY isSearchingChanged)
    Q_PROPERTY(int resultCount READ resultCount NOTIFY resultCountChanged)
    Q_PROPERTY(bool hasResults READ hasResults NOTIFY resultCountChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorMessageChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY errorMessageChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

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
        GroupRole
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

    // Search Properties
    QString query() const { return m_query; }
    bool isSearching() const { return m_isSearching; }
    int resultCount() const { return static_cast<int>(m_results.size()); }
    bool hasResults() const { return !m_results.empty(); }
    QString errorMessage() const { return m_errorMessage; }
    bool hasError() const { return !m_errorMessage.isEmpty(); }
    QString statusMessage() const { return m_statusMessage; }

    // QML-invokable methods
    Q_INVOKABLE void selectLibrary(int index);
    Q_INVOKABLE void selectLibraryById(const QString &id);
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void searchInLibrary(const QString &query, int libraryIndex);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QVariantMap getResult(int index) const;

signals:
    void selectedLibraryChanged();
    void queryChanged();
    void isSearchingChanged();
    void resultCountChanged();
    void errorMessageChanged();
    void statusMessageChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply, quint64 searchId, MediaRoot activeRoot);

private:
    void setSearching(bool searching);
    void setErrorMessage(const QString &msg);
    void setStatusMessage(const QString &msg);

    std::unique_ptr<MediaLibraryModel> m_libraryModel;
    int m_selectedLibraryIndex = 0;

    QNetworkAccessManager m_networkManager;
    QNetworkReply *m_activeReply = nullptr;
    quint64 m_activeSearchId = 0;

    QString m_query;
    bool m_isSearching = false;
    QString m_errorMessage;
    QString m_statusMessage;

    std::vector<SearchResult> m_results;
};

} // namespace Flux
