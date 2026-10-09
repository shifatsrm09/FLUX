#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QNetworkAccessManager>
#include <QString>
#include <QVariantList>
#include <functional>
#include <vector>

#include "SearchResult.h"

namespace Flux {

// Browses the contents of one server directory (a "folder card" from search results),
// with breadcrumbs for navigation. Also resolves "what is the next episode?" for a file.
class FolderBrowser : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(bool active READ active NOTIFY activeChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(bool hasError READ hasError NOTIFY errorChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)
    Q_PROPERTY(QString folderName READ folderName NOTIFY locationChanged)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY locationChanged)
    Q_PROPERTY(QVariantList breadcrumbs READ breadcrumbs NOTIFY locationChanged)
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)

    // Next episode of the file most recently passed to findNext()
    Q_PROPERTY(QString nextUrl READ nextUrl NOTIFY nextChanged)
    Q_PROPERTY(QString nextName READ nextName NOTIFY nextChanged)

public:
    enum Roles {
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

    explicit FolderBrowser(QObject *parent = nullptr);
    ~FolderBrowser() override = default;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool active() const { return m_active; }
    bool isLoading() const { return m_loading; }
    bool hasError() const { return !m_error.isEmpty(); }
    QString errorMessage() const { return m_error; }
    QString folderName() const;
    QString currentUrl() const { return m_origin + m_href; }
    QVariantList breadcrumbs() const;
    int itemCount() const { return static_cast<int>(m_items.size()); }
    QString nextUrl() const { return m_nextUrl; }
    QString nextName() const { return m_nextName; }

    Q_INVOKABLE void open(const QString &folderUrl, const QString &libraryName = QString(), const QString &group = QString());
    Q_INVOKABLE void close();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void goTo(int crumbIndex);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void findNext(const QString &currentFileUrl);
    Q_INVOKABLE void clearNext();

    // One-shot listing of any server directory (folders + video files, naturally sorted).
    // Independent of the browsed location; used by the download manager to scan packs.
    void listFolder(const QString &folderUrl, std::function<void(bool, std::vector<SearchResult>)> done);

signals:
    void activeChanged();
    void isLoadingChanged();
    void errorChanged();
    void locationChanged();
    void itemCountChanged();
    void nextChanged();

private:
    using ListingCallback = std::function<void(bool ok, std::vector<SearchResult> items)>;

    static bool splitUrl(const QString &url, QString &origin, QString &href);
    static QString keyHref(const QString &href);

    void load();
    void applyResult(bool ok, std::vector<SearchResult> items);
    void findNextLocal(const QString &filePath);
    void requestListing(const QString &origin, const QString &href,
                        const QString &libraryName, const QString &group,
                        ListingCallback done);
    void postApi(const QString &origin, const QString &href,
                 std::function<void(bool, const QByteArray &)> done);
    void getHtml(const QString &origin, const QString &href,
                 std::function<void(bool, const QByteArray &)> done);
    void storeCache(const QString &key, const std::vector<SearchResult> &items);

    void setActive(bool active);
    void setLoading(bool loading);
    void setError(const QString &msg);

    QNetworkAccessManager m_network;
    std::vector<SearchResult> m_items;
    QHash<QString, std::vector<SearchResult>> m_cache;

    bool m_active = false;
    bool m_loading = false;
    QString m_error;

    QString m_origin;
    QString m_href;          // encoded path, always ends with '/'
    QString m_libraryName;
    QString m_group;

    quint64 m_loadToken = 0;
    quint64 m_nextToken = 0;
    QString m_nextUrl;
    QString m_nextName;
};

} // namespace Flux
