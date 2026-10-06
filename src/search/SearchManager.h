#pragma once

#include <QAbstractListModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <vector>
#include "SearchResult.h"

namespace Flux {

class SearchManager : public QAbstractListModel {
    Q_OBJECT

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
        RawHrefRole
    };

    explicit SearchManager(QObject *parent = nullptr);
    ~SearchManager() override;

    // QAbstractListModel overrides
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Properties
    QString query() const { return m_query; }
    bool isSearching() const { return m_isSearching; }
    int resultCount() const { return static_cast<int>(m_results.size()); }
    bool hasResults() const { return !m_results.empty(); }
    QString errorMessage() const { return m_errorMessage; }
    bool hasError() const { return !m_errorMessage.isEmpty(); }
    QString statusMessage() const { return m_statusMessage; }

    // QML-invokable methods
    Q_INVOKABLE void search(const QString &query);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void cancel();
    Q_INVOKABLE QVariantMap getResult(int index) const;

signals:
    void queryChanged();
    void isSearchingChanged();
    void resultCountChanged();
    void errorMessageChanged();
    void statusMessageChanged();

private slots:
    void onReplyFinished(QNetworkReply *reply, quint64 searchId);

private:
    void setSearching(bool searching);
    void setErrorMessage(const QString &msg);
    void setStatusMessage(const QString &msg);

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
