#pragma once

#include <QAbstractListModel>
#include <QElapsedTimer>
#include <QFile>
#include <QNetworkAccessManager>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <memory>
#include <vector>

#include "../search/SearchResult.h"

class QNetworkReply;

namespace Flux {

class FolderBrowser;

// Downloads media from the server to disk.
//
//   <location>/Individuals/<file>                 single files (movies, episodes)
//   <location>/Series/<folder>/<sub>/<file>       whole folders ("packs"), structure preserved
//
// The default location is <user Downloads>/FLUX; it can be changed at runtime. Both sub
// folders are created up front. Downloads are streamed into "<name>.part" files and renamed
// when complete, so an interrupted download can resume (HTTP Range) from where it stopped.
class DownloadManager : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(QString downloadLocation READ downloadLocation WRITE setDownloadLocation NOTIFY locationChanged)
    Q_PROPERTY(QString defaultLocation READ defaultLocation CONSTANT)
    Q_PROPERTY(QString seriesDir READ seriesDir NOTIFY locationChanged)
    Q_PROPERTY(QString individualsDir READ individualsDir NOTIFY locationChanged)
    Q_PROPERTY(int activeCount READ activeCount NOTIFY summaryChanged)
    Q_PROPERTY(int count READ count NOTIFY summaryChanged)
    Q_PROPERTY(bool hasFinished READ hasFinished NOTIFY summaryChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TitleRole,
        KindRole,
        CategoryRole,
        StatusRole,
        ProgressRole,
        DetailRole,
        DestinationRole,
        ErrorRole,
        ActiveRole
    };

    explicit DownloadManager(FolderBrowser *browser, QObject *parent = nullptr);
    ~DownloadManager() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString downloadLocation() const;
    void setDownloadLocation(const QString &path);
    QString defaultLocation() const;
    QString seriesDir() const;
    QString individualsDir() const;
    int activeCount() const { return m_activeCount; }
    int count() const { return static_cast<int>(m_jobs.size()); }
    bool hasFinished() const { return m_hasFinished; }

    // ---- Location ----
    Q_INVOKABLE bool changeLocation(const QString &path);
    Q_INVOKABLE void resetLocation();
    Q_INVOKABLE void openRoot();

    // ---- Folder chooser helpers (used by the in-app folder picker) ----
    Q_INVOKABLE QVariantList listDirs(const QString &path) const;   // empty path = drives
    Q_INVOKABLE QString parentDir(const QString &path) const;       // "" = go to drives list
    Q_INVOKABLE QString makeDir(const QString &parent, const QString &name) const;

    // ---- Downloads ----
    // Both return the job id (0 if nothing was started). Re-requesting something that is
    // already queued/downloading returns the existing job instead of duplicating it.
    Q_INVOKABLE int downloadFile(const QString &url, const QString &title);
    Q_INVOKABLE int downloadPack(const QString &folderUrl, const QString &title);

    Q_INVOKABLE void cancel(int id);
    Q_INVOKABLE void retry(int id);
    Q_INVOKABLE void remove(int id);
    Q_INVOKABLE void clearFinished();
    Q_INVOKABLE void openFolder(int id);

signals:
    void locationChanged();
    void summaryChanged();
    void locationError(const QString &message);

private:
    enum class Status { Scanning, Queued, Running, Done, Failed, Cancelled };

    struct Item {
        QString url;
        QString relPath;       // path below the job's destination folder
        qint64 size = -1;      // bytes, -1 when unknown
        bool done = false;
    };

    struct Job {
        int id = 0;
        QString title;
        QString sourceUrl;
        QString category;      // "Series" | "Individuals"
        QString destDir;       // folder that receives the files (forward slashes)
        bool isPack = false;
        Status status = Status::Queued;
        QString error;
        QString note;          // transient hint ("Connection lost, retrying...")

        std::vector<Item> items;
        int nextIndex = 0;     // item currently being (or about to be) downloaded
        int doneCount = 0;
        int pendingLists = 0;  // folder listings still in flight while scanning
        int scanGen = 0;       // bumped on cancel/retry so stale listing callbacks are ignored
        bool scanned = false;  // pack scan finished (item list is complete)
        bool sizesKnown = false;
        qint64 totalBytes = 0;
        qint64 doneBytes = 0;  // bytes of completed items

        // Current transfer
        QNetworkReply *reply = nullptr;
        std::unique_ptr<QFile> file;
        QString finalPath;
        QString partPath;
        qint64 resumeFrom = 0;
        qint64 curBytes = 0;   // bytes of the current item on disk (incl. resumed part)
        qint64 curTotal = 0;   // full size of the current item as reported by the server
        int httpStatus = 0;
        bool headersChecked = false;
        bool writeFailed = false;
        int attempts = 0;

        // Speed / UI throttling
        double speed = 0.0;    // bytes per second, smoothed
        qint64 speedMarkMs = 0;
        qint64 speedMarkBytes = 0;
        QElapsedTimer clock;
        QElapsedTimer emitClock;
    };

    static QString statusName(Status s);
    static double progressOf(const Job &job);
    static QString detailOf(const Job &job);

    Job *findJob(int id, int *row = nullptr) const;
    Job *findActiveBySource(const QString &url) const;
    void insertJob(std::unique_ptr<Job> job);
    void touch(const Job &job);
    void updateSummary();
    void ensureFolders() const;

    void scanFolder(int id, const QString &folderUrl, const QString &rel, int depth, int gen);
    void onScanResult(int id, const QString &rel, int depth, int gen, bool ok, std::vector<SearchResult> items);
    void finalizeScan(Job &job);

    void pump();
    void startNext(Job &job);
    bool finalizeItem(Job &job);
    void failJob(Job &job, const QString &message);
    void finishJob(Job &job);
    void closePart(Job &job, bool removeFile);

    void onReadyRead(int id, QNetworkReply *reply);
    void onProgress(int id, QNetworkReply *reply, qint64 received, qint64 total);
    void onFinished(int id, QNetworkReply *reply);

    QPointer<FolderBrowser> m_browser;
    QNetworkAccessManager m_network;
    std::vector<std::unique_ptr<Job>> m_jobs;   // newest first

    QString m_root;            // forward slashes, no trailing slash
    int m_nextId = 0;
    int m_activeCount = 0;
    bool m_hasFinished = false;
    bool m_pumping = false;
    bool m_pumpAgain = false;
    bool m_shuttingDown = false;
};

} // namespace Flux
