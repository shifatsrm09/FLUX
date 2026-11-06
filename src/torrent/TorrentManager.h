#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QProcess>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QMap>

namespace Flux {

class TorrentManager : public QObject {
    Q_OBJECT

    // Engine status
    Q_PROPERTY(bool engineReady READ engineReady NOTIFY engineReadyChanged)
    Q_PROPERTY(bool isSettingUp READ isSettingUp NOTIFY setupChanged)
    Q_PROPERTY(double setupProgress READ setupProgress NOTIFY setupChanged)
    Q_PROPERTY(QString setupStatus READ setupStatus NOTIFY setupChanged)

    // Current torrent state: "idle" | "adding" | "connecting" | "preload" | "ready" | "streaming" | "error"
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusChanged)
    Q_PROPERTY(bool isReadyToPlay READ isReadyToPlay NOTIFY readyToPlayChanged)
    Q_PROPERTY(bool isBuffering READ isBuffering NOTIFY bufferingChanged)

    // Metadata & stats
    Q_PROPERTY(QString torrentName READ torrentName NOTIFY torrentChanged)
    Q_PROPERTY(QString torrentHash READ torrentHash NOTIFY torrentChanged)
    Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY torrentChanged)
    Q_PROPERTY(QString formattedSize READ formattedSize NOTIFY torrentChanged)

    Q_PROPERTY(qint64 downloadSpeed READ downloadSpeed NOTIFY statsChanged)
    Q_PROPERTY(QString formattedDownloadSpeed READ formattedDownloadSpeed NOTIFY statsChanged)
    Q_PROPERTY(qint64 uploadSpeed READ uploadSpeed NOTIFY statsChanged)
    Q_PROPERTY(QString formattedUploadSpeed READ formattedUploadSpeed NOTIFY statsChanged)

    Q_PROPERTY(int activePeers READ activePeers NOTIFY statsChanged)
    Q_PROPERTY(int totalPeers READ totalPeers NOTIFY statsChanged)

    Q_PROPERTY(double progress READ progress NOTIFY statsChanged)
    Q_PROPERTY(double bufferProgress READ bufferProgress NOTIFY statsChanged)

    // Playback
    Q_PROPERTY(QString streamUrl READ streamUrl NOTIFY streamUrlChanged)
    Q_PROPERTY(QVariantList files READ files NOTIFY filesChanged)
    Q_PROPERTY(int selectedFileIndex READ selectedFileIndex NOTIFY selectedFileIndexChanged)
    Q_PROPERTY(QString selectedFileName READ selectedFileName NOTIFY selectedFileIndexChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged)

    // 1337x Search & Stream
    Q_PROPERTY(bool isSearching1337x READ isSearching1337x NOTIFY searching1337xChanged)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchErrorChanged)
    Q_PROPERTY(bool isResolvingMagnet READ isResolvingMagnet NOTIFY resolvingMagnetChanged)
    Q_PROPERTY(QString resolvingTorrentName READ resolvingTorrentName NOTIFY resolvingMagnetChanged)
    Q_PROPERTY(int resolvingIndex READ resolvingIndex NOTIFY resolvingMagnetChanged)

    // Tork Multi-Engine Search & Stream
    Q_PROPERTY(bool isSearchingTork READ isSearchingTork NOTIFY searchingTorkChanged)
    Q_PROPERTY(QVariantList torkResults READ torkResults NOTIFY torkResultsChanged)
    Q_PROPERTY(QString torkError READ torkError NOTIFY torkErrorChanged)
    Q_PROPERTY(int torkResultCount READ torkResultCount NOTIFY torkResultsChanged)

public:
    explicit TorrentManager(QObject *parent = nullptr);
    ~TorrentManager() override;

    bool engineReady() const { return m_engineReady; }
    bool isSettingUp() const { return m_isSettingUp; }
    double setupProgress() const { return m_setupProgress; }
    QString setupStatus() const { return m_setupStatus; }

    QString status() const { return m_status; }
    QString statusMessage() const { return m_statusMessage; }
    bool isReadyToPlay() const { return m_isReadyToPlay; }
    bool isBuffering() const { return m_status == QLatin1String("preload"); }

    QString torrentName() const { return m_torrentName; }
    QString torrentHash() const { return m_torrentHash; }
    qint64 totalSize() const { return m_totalSize; }
    QString formattedSize() const;

    qint64 downloadSpeed() const { return m_downloadSpeed; }
    QString formattedDownloadSpeed() const;
    qint64 uploadSpeed() const { return m_uploadSpeed; }
    QString formattedUploadSpeed() const;

    int activePeers() const { return m_activePeers; }
    int totalPeers() const { return m_totalPeers; }

    double progress() const { return m_progress; }
    double bufferProgress() const { return m_bufferProgress; }

    QString streamUrl() const { return m_streamUrl; }
    QVariantList files() const { return m_files; }
    int selectedFileIndex() const { return m_selectedFileIndex; }
    QString selectedFileName() const { return m_selectedFileName; }
    QString errorMessage() const { return m_errorMessage; }

    // 1337x Search getters
    bool isSearching1337x() const { return m_isSearching1337x; }
    QVariantList searchResults() const { return m_searchResults; }
    QString searchError() const { return m_searchError; }
    bool isResolvingMagnet() const { return m_isResolvingMagnet; }
    QString resolvingTorrentName() const { return m_resolvingTorrentName; }
    int resolvingIndex() const { return m_resolvingIndex; }

    // Tork Search getters
    bool isSearchingTork() const { return m_isSearchingTork; }
    QVariantList torkResults() const { return m_torkResults; }
    QString torkError() const { return m_torkError; }
    int torkResultCount() const { return m_torkResults.size(); }

    Q_INVOKABLE void startTorrent(const QString &magnetOrPath);
    Q_INVOKABLE void selectFile(int fileIndex);
    Q_INVOKABLE void stopTorrent();
    Q_INVOKABLE void checkEngine();
    Q_INVOKABLE QString browseTorrentFile();

    // 1337x Search invokables
    Q_INVOKABLE void search1337x(const QString &query, const QString &category = QString());
    Q_INVOKABLE void cancelSearch1337x();
    Q_INVOKABLE void clearSearchResults();
    Q_INVOKABLE void streamSearchResult(int index);
    Q_INVOKABLE void downloadSearchResult(int index);
    Q_INVOKABLE void copyMagnet(int index);

    // Tork Multi-Engine Search invokables
    Q_INVOKABLE void searchTork(const QString &query, const QString &category = QString());
    Q_INVOKABLE void cancelSearchTork();
    Q_INVOKABLE void clearTorkResults();
    Q_INVOKABLE void streamTorkResult(int index);
    Q_INVOKABLE void downloadTorkResult(int index);
    Q_INVOKABLE void copyTorkMagnet(int index);

    // Direct download helper
    Q_INVOKABLE void downloadTorrent(const QString &magnet, const QString &title);

signals:
    void engineReadyChanged();
    void setupChanged();
    void statusChanged();
    void torrentChanged();
    void statsChanged();
    void streamUrlChanged();
    void filesChanged();
    void selectedFileIndexChanged();
    void errorChanged();
    void readyToPlayChanged();
    void bufferingChanged();
    void torrentReadyToPlay(const QString &url, const QString &title);

    // 1337x Signals
    void searching1337xChanged();
    void searchResultsChanged();
    void searchErrorChanged();
    void resolvingMagnetChanged();
    void magnetCopied(const QString &magnet);

    // Tork Signals
    void searchingTorkChanged();
    void torkResultsChanged();
    void torkErrorChanged();
    void downloadStarted(const QString &title);
    void downloadRequested(const QString &url, const QString &title);

private slots:
    void onPollTimer();
    void onTorkTimeout();

private:
    void startEngineProcess();
    void downloadEngine();
    void pingEngine();
    void configureEngineSettings();
    void startSequentialPreload();
    void addMagnet(const QString &magnet);
    void uploadTorrentFile(const QString &filePath);
    void parseStatusResponse(const QByteArray &data);
    void updateStreamUrl();
    QString findEngineExecutable() const;

    // 1337x Private helpers
    void parse1337xSearchHtml(const QString &html, const QString &baseUrl);
    static QString decodeHtmlEntities(const QString &raw);

    // Tork Private helpers
    void parseTpbResponse(const QByteArray &data, const QString &category);
    void parseYtsResponse(const QByteArray &data);
    void parseRarbgResponse(const QByteArray &data, const QString &category);
    void parseNyaaResponse(const QByteArray &data);
    void addOrMergeTorkResult(const QString &infoHash, const QString &title,
                             qint64 sizeBytes, const QString &formattedSize,
                             int seeders, int leechers,
                             const QString &category, const QString &provider);
    void finalizeTorkResults();
    static QString createMagnetLink(const QString &infoHash, const QString &title);

    static QString formatBytes(qint64 bytes);
    static bool isVideoFile(const QString &path);

    int m_port{8090};
    bool m_engineReady{false};
    bool m_isSettingUp{false};
    double m_setupProgress{0.0};
    QString m_setupStatus;

    QString m_status{"idle"};
    QString m_statusMessage;

    QString m_torrentName;
    QString m_torrentHash;
    qint64 m_totalSize{0};

    qint64 m_downloadSpeed{0};
    qint64 m_uploadSpeed{0};
    int m_activePeers{0};
    int m_totalPeers{0};
    double m_progress{0.0};
    double m_bufferProgress{0.0};

    QString m_streamUrl;
    QVariantList m_files;
    int m_selectedFileIndex{-1};
    QString m_selectedFileName;
    QString m_errorMessage;
    bool m_isReadyToPlay{false};

    // 1337x Search state
    bool m_isSearching1337x{false};
    QVariantList m_searchResults;
    QString m_searchError;
    bool m_isResolvingMagnet{false};
    QString m_resolvingTorrentName;
    int m_resolvingIndex{-1};
    bool m_pendingDownloadAfterResolve{false};
    QPointer<QNetworkReply> m_searchReply;
    QPointer<QNetworkReply> m_detailReply;
    QString m_currentBaseUrl{QStringLiteral("https://1337x.la")};

    // Tork Multi-Engine Search state
    bool m_isSearchingTork{false};
    QVariantList m_torkResults;
    QString m_torkError;
    int m_activeTorkPending{0};
    QMap<QString, QVariantMap> m_pendingTorkMap;
    QList<QPointer<QNetworkReply>> m_torkReplies;
    QTimer *m_torkTimeoutTimer{nullptr};
    bool m_pendingDownloadStream{false};
    QString m_pendingDownloadTitle;

    QProcess *m_process{nullptr};
    QNetworkAccessManager *m_nam{nullptr};
    QTimer *m_pollTimer{nullptr};
    QTimer *m_engineCheckTimer{nullptr};
    QPointer<QNetworkReply> m_preloadReply;
};

} // namespace Flux
