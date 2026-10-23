#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QProcess>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>

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

    Q_INVOKABLE void startTorrent(const QString &magnetOrPath);
    Q_INVOKABLE void selectFile(int fileIndex);
    Q_INVOKABLE void stopTorrent();
    Q_INVOKABLE void checkEngine();
    Q_INVOKABLE QString browseTorrentFile();

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

private slots:
    void onPollTimer();

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

    QProcess *m_process{nullptr};
    QNetworkAccessManager *m_nam{nullptr};
    QTimer *m_pollTimer{nullptr};
    QTimer *m_engineCheckTimer{nullptr};
    QPointer<QNetworkReply> m_preloadReply;
};

} // namespace Flux
