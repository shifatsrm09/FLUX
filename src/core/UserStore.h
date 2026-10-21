#pragma once

#include <QObject>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QSet>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QJSValue>
class QNetworkAccessManager;

namespace Flux {

class VLCPlayer;

// Persistent user data:
//  - settings (QSettings INI in the user's app-data folder)
//  - watch history: per-URL resume position, watched flag, "Continue Watching"
class UserStore : public QObject {
    Q_OBJECT

    // Bumps whenever history changes; QML bindings read it to refresh progress badges
    Q_PROPERTY(int revision READ revision NOTIFY historyChanged)
    Q_PROPERTY(QVariantList continueWatching READ continueWatching NOTIFY historyChanged)
    Q_PROPERTY(bool autoplayNext READ autoplayNext WRITE setAutoplayNext NOTIFY autoplayNextChanged)
    Q_PROPERTY(bool topPicksFirst READ topPicksFirst WRITE setTopPicksFirst NOTIFY topPicksFirstChanged)
    Q_PROPERTY(int activeServer READ activeServer WRITE setActiveServer NOTIFY serverChanged)
    Q_PROPERTY(bool server1Available READ isServer1Available NOTIFY serverAvailabilityChanged)
    Q_PROPERTY(bool server2Available READ isServer2Available NOTIFY serverAvailabilityChanged)
    Q_PROPERTY(bool serverCheckComplete READ isServerCheckComplete NOTIFY serverAvailabilityChanged)
    Q_PROPERTY(bool bothServersDown READ bothServersDown NOTIFY serverAvailabilityChanged)
    Q_PROPERTY(QString iccftpUrl READ iccftpUrl WRITE setIccftpUrl NOTIFY serverChanged)
    Q_PROPERTY(QString iccftpSession READ iccftpSession WRITE setIccftpSession NOTIFY serverChanged)
    // Bumps when a poster is remembered; QML bindings read it to pick up new posters
    Q_PROPERTY(int posterRevision READ posterRevision NOTIFY postersChanged)
    Q_PROPERTY(bool autoFetchPosters READ autoFetchPosters WRITE setAutoFetchPosters NOTIFY autoFetchPostersChanged)
    Q_PROPERTY(QString tmdbApiKey READ tmdbApiKey WRITE setTmdbApiKey NOTIFY tmdbApiKeyChanged)
    Q_PROPERTY(bool devModeUnlocked READ devModeUnlocked WRITE setDevModeUnlocked NOTIFY devModeUnlockedChanged)
    Q_PROPERTY(QString devPin READ devPin WRITE setDevPin NOTIFY devPinChanged)
    Q_PROPERTY(bool useCustomTelepartyCode READ useCustomTelepartyCode WRITE setUseCustomTelepartyCode NOTIFY customTelepartyCodeChanged)
    Q_PROPERTY(QString customTelepartyCode READ customTelepartyCode WRITE setCustomTelepartyCode NOTIFY customTelepartyCodeChanged)

public:
    explicit UserStore(QObject *parent = nullptr);
    ~UserStore() override;

    // Records the player's progress automatically
    void attachPlayer(VLCPlayer *player);

    int revision() const { return m_revision; }
    QVariantList continueWatching() const;
    bool autoplayNext() const;
    void setAutoplayNext(bool enabled);
    bool topPicksFirst() const;
    Q_INVOKABLE void setTopPicksFirst(bool first);

    // ---- Media Server (1 = Dhaka Flix, 2 = ICC FTP) ----
    // The setters are Q_INVOKABLE so QML can call them as methods as well as assign the properties
    int activeServer() const;
    Q_INVOKABLE void setActiveServer(int server);
    bool isServer1Available() const { return m_server1Available; }
    bool isServer2Available() const { return m_server2Available; }
    bool isServerCheckComplete() const { return m_serverCheckComplete; }
    bool bothServersDown() const { return m_serverCheckComplete && !m_server1Available && !m_server2Available; }
    Q_INVOKABLE void autoDetectServer();
    QString iccftpUrl() const;
    Q_INVOKABLE void setIccftpUrl(const QString &url);
    QString iccftpSession() const;
    Q_INVOKABLE void setIccftpSession(const QString &session);
    Q_INVOKABLE void testServerConnection(int server, const QString &customUrl, const QJSValue &callback);

    // ---- Posters (banners from ICC FTP) ----
    // Remembered per item so Bookmarks and Continue Watching can show the same artwork as search
    // results. Items are matched by their play id / address, not by the session token in the URL.
    int posterRevision() const { return m_posterRevision; }
    Q_INVOKABLE void rememberPoster(const QString &url, const QString &posterUrl, const QString &title = QString());
    Q_INVOKABLE QString posterFor(const QString &url, const QString &title = QString()) const;
    Q_INVOKABLE void mapSeriesFolder(const QString &mediaFolderUrl, const QString &webFolderUrl);
    Q_INVOKABLE QString seriesFolderFor(const QString &mediaFolderUrl) const;

    // ---- Server reachability & URL detection ----
    Q_INVOKABLE int serverForUrl(const QString &url) const;
    Q_INVOKABLE QString serverNameForUrl(const QString &url) const;
    Q_INVOKABLE bool isServerAvailableForUrl(const QString &url) const;
    // ---- Settings ----
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE int intValue(const QString &key, int defaultValue) const;
    Q_INVOKABLE bool boolValue(const QString &key, bool defaultValue) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);

    // ---- Watch history ----
    Q_INVOKABLE void noteStart(const QString &url, const QString &title,
                               const QString &playbackUrl = QString());
    Q_INVOKABLE void notePartyMedia(const QString &url, const QString &title, qint64 positionMs, qint64 durationMs);
    Q_INVOKABLE void clearPartyMedia();
    Q_INVOKABLE QVariantMap progressFor(const QString &url) const;
    Q_INVOKABLE qint64 resumePositionFor(const QString &url) const;
    Q_INVOKABLE void removeFromHistory(const QString &url);
    Q_INVOKABLE void markWatched(const QString &url, bool watched);
    Q_INVOKABLE void clearHistory();

    bool autoFetchPosters() const;
    Q_INVOKABLE void setAutoFetchPosters(bool enabled);
    QString tmdbApiKey() const;
    Q_INVOKABLE void setTmdbApiKey(const QString &key);

    Q_INVOKABLE void fetchPoster(const QString &url, const QString &title = QString(), bool isFolder = false);
    Q_INVOKABLE void clearPosters();

    bool devModeUnlocked() const;
    void setDevModeUnlocked(bool unlocked);
    QString devPin() const;
    void setDevPin(const QString &pin);
    bool useCustomTelepartyCode() const;
    void setUseCustomTelepartyCode(bool use);
    QString customTelepartyCode() const;
    void setCustomTelepartyCode(const QString &code);
    Q_INVOKABLE bool verifyDevPin(const QString &pin);

    // Called on application exit
    Q_INVOKABLE void shutdown();

signals:
    void historyChanged();
    void autoplayNextChanged();
    void topPicksFirstChanged();
    void serverChanged();
    void serverAvailabilityChanged();
    void postersChanged();
    void autoFetchPostersChanged();
    void tmdbApiKeyChanged();
    void devModeUnlockedChanged();
    void devPinChanged();
    void customTelepartyCodeChanged();

private:
    struct PosterFetchTask {
        QString url;
        QString originalTitle;
        QString cleanTitle;
        QString year;
        QString type; // "movie" or "series"
        bool retriedOtherType = false;
    };

    struct HistoryEntry {
        QString url;
        QString title;          // raw file name (decoded)
        qint64 positionMs = 0;
        qint64 durationMs = 0;
        qint64 lastPlayed = 0;  // ms since epoch
        bool watched = false;
    };

    static QString keyFor(const QString &url);
    static QString nameFromUrl(const QString &url);
    static QString posterKey(const QString &url);
    static QString titleKey(const QString &title);
    static QString folderKey(const QString &url);

    void loadPosters();
    void savePosters();
    void processPosterQueue();
    void executePosterFetch(const PosterFetchTask &task);
    void handleCinemetaResponse(const PosterFetchTask &task, const QByteArray &data);
    void handleTmdbResponse(const PosterFetchTask &task, const QByteArray &data);

    void loadHistory();
    void scheduleSave();
    void writeHistory();
    void checkpoint(bool notify, bool allowZeroPosition = false);
    void onPlayerStateChanged();

    QSettings m_settings;
    QString m_historyPath;

    struct PosterEntry {
        QString url;
        qint64 usedAt = 0;
    };
    QHash<QString, PosterEntry> m_posters;   // posterKey(item url) or titleKey(title) -> poster image url
    QHash<QString, QString> m_seriesFolders; // folderKey(mediaFolderUrl) -> webFolderUrl
    QString m_posterPath;
    QTimer m_posterSaveTimer;
    int m_posterRevision = 0;
    bool m_postersDirty = false;

    QNetworkAccessManager *m_posterNam = nullptr;
    QList<PosterFetchTask> m_posterQueue;
    QSet<QString> m_pendingPosterFetches;
    QSet<QString> m_failedPosterFetches;
    int m_activePosterRequests = 0;
    bool m_autoFetchPosters = true;
    QString m_tmdbApiKey;

    QHash<QString, HistoryEntry> m_history;
    QString m_currentKey;
    QString m_currentPlaybackKey;
    QString m_partyKey;
    HistoryEntry m_partyEntry;
    bool m_hasPartyMedia = false;
    int m_revision = 0;

    QPointer<VLCPlayer> m_player;
    QTimer m_tickTimer;      // periodic progress capture while playing
    QTimer m_saveTimer;      // debounced disk writes
    QTimer m_settingsTimer;  // debounced QSettings sync
    bool m_dirty = false;
    bool m_server1Available = true;
    bool m_server2Available = true;
    bool m_serverCheckComplete = false;
    bool m_devUnlocked = false;
};

} // namespace Flux
