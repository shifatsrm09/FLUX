#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

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

public:
    explicit UserStore(QObject *parent = nullptr);
    ~UserStore() override;

    // Records the player's progress automatically
    void attachPlayer(VLCPlayer *player);

    int revision() const { return m_revision; }
    QVariantList continueWatching() const;
    bool autoplayNext() const;
    void setAutoplayNext(bool enabled);

    // ---- Settings ----
    Q_INVOKABLE QVariant value(const QString &key, const QVariant &defaultValue = QVariant()) const;
    Q_INVOKABLE int intValue(const QString &key, int defaultValue) const;
    Q_INVOKABLE bool boolValue(const QString &key, bool defaultValue) const;
    Q_INVOKABLE void setValue(const QString &key, const QVariant &value);

    // ---- Watch history ----
    Q_INVOKABLE void noteStart(const QString &url, const QString &title);
    Q_INVOKABLE void notePartyMedia(const QString &url, const QString &title, qint64 positionMs, qint64 durationMs);
    Q_INVOKABLE void clearPartyMedia();
    Q_INVOKABLE QVariantMap progressFor(const QString &url) const;
    Q_INVOKABLE qint64 resumePositionFor(const QString &url) const;
    Q_INVOKABLE void removeFromHistory(const QString &url);
    Q_INVOKABLE void markWatched(const QString &url, bool watched);
    Q_INVOKABLE void clearHistory();

    // Called on application exit
    Q_INVOKABLE void shutdown();

signals:
    void historyChanged();
    void autoplayNextChanged();

private:
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

    void loadHistory();
    void scheduleSave();
    void writeHistory();
    void checkpoint(bool notify);
    void onPlayerStateChanged();

    QSettings m_settings;
    QString m_historyPath;
    QHash<QString, HistoryEntry> m_history;
    QString m_currentKey;
    QString m_partyKey;
    int m_revision = 0;

    QPointer<VLCPlayer> m_player;
    QTimer m_tickTimer;      // periodic progress capture while playing
    QTimer m_saveTimer;      // debounced disk writes
    QTimer m_settingsTimer;  // debounced QSettings sync
    bool m_dirty = false;
};

} // namespace Flux
