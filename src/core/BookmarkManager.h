#pragma once

#include <QAbstractListModel>
#include <QSet>
#include <QString>
#include <QTimer>
#include <vector>

namespace Flux {

// Bookmarked files and folders.
//
// Exposes the SAME roles as search results (title, parentPath, isFolder, formattedSize,
// playUrl, extension, rawHref, libraryName, group) so the existing result cards and grid can
// display it unchanged. Newest bookmark first. Saved to bookmarks.json next to the settings.
class BookmarkManager : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Bumps on every change; QML bindings read it to refresh "is bookmarked" state
    Q_PROPERTY(int revision READ revision NOTIFY revisionChanged)

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

    explicit BookmarkManager(QObject *parent = nullptr);
    ~BookmarkManager() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_entries.size()); }
    int revision() const { return m_revision; }

    Q_INVOKABLE bool isBookmarked(const QString &url) const;

    // Adds the bookmark, or removes it if it already exists. Returns true if the item is
    // bookmarked afterwards.
    Q_INVOKABLE bool toggle(const QString &url, const QString &title, bool isFolder,
                            const QString &formattedSize, const QString &extension,
                            const QString &libraryName, const QString &parentPath);
    Q_INVOKABLE void remove(const QString &url);
    Q_INVOKABLE void clear();

    // Write pending changes to disk now (called on shutdown)
    Q_INVOKABLE void flush();

signals:
    void countChanged();
    void revisionChanged();

private:
    struct Entry {
        QString url;
        QString title;          // raw file / folder name
        QString parentPath;
        QString formattedSize;
        QString extension;
        QString libraryName;
        QString group;
        QString rawHref;
        bool isFolder = false;
        qint64 addedAt = 0;
    };

    static QString keyFor(const QString &url);

    void load();
    void save();
    void changed();

    std::vector<Entry> m_entries;   // newest first
    QSet<QString> m_keys;           // normalised URLs, for O(1) isBookmarked()
    QString m_path;
    QTimer m_saveTimer;
    int m_revision = 0;
    bool m_dirty = false;
};

} // namespace Flux
