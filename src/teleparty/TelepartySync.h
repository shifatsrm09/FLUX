#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

namespace Flux {

class TelepartySession;
class VLCPlayer;

// Keeps every member's player in step.
//
// Local user actions (open a video, play / pause, seek) are sent to the room as events;
// events from other members are applied to the local player without being sent back.
//
// Events (all carried by TelepartySession::sendEvent):
//   open    {url, title}          somebody opened a video: everyone opens it from the start
//   play    {t}                   resume at position t (ms)
//   pause   {t}                   pause at position t
//   seek    {t}                   jump to position t
//   state   {url, title, t, playing}   "this is what I am watching": answers a newcomer
//   syncme  {}                    "tell me what you are watching" (member returning to the player)
//
// Only http(s) streams are shared; offline files exist on one machine only.
class TelepartySync : public QObject {
    Q_OBJECT

    // Set by the UI: true while the player page is on screen
    Q_PROPERTY(bool watching READ watching WRITE setWatching NOTIFY watchingChanged)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY currentMediaChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentMediaChanged)
    Q_PROPERTY(bool hasPartyMedia READ hasPartyMedia NOTIFY currentMediaChanged)

public:
    TelepartySync(TelepartySession *session, VLCPlayer *player, QObject *parent = nullptr);

    bool watching() const { return m_watching; }
    void setWatching(bool watching);

    QString currentUrl() const { return m_currentUrl; }
    QString currentTitle() const { return m_currentTitle; }
    bool hasPartyMedia() const { return !m_currentUrl.isEmpty(); }

    // True for http(s) URLs: the only kind of media that can be watched together
    Q_INVOKABLE bool isStreamUrl(const QString &url) const;

    // The local user opened a video: tell everyone to open it too
    Q_INVOKABLE void broadcastOpen(const QString &url, const QString &title);

    // The host was already watching something when the session started: make that the
    // shared video (nothing is sent; newcomers learn about it through "state")
    Q_INVOKABLE void adoptCurrent(const QString &url, const QString &title);

    // Open or catch up to the party's currently playing video at the party's live timestamp
    // (without broadcasting "open" or resetting other members)
    Q_INVOKABLE void joinPartyPlayback();

signals:
    void watchingChanged();
    void currentMediaChanged();

    // Another member opened (or you are catching up with) a video: open it in the UI
    void remoteOpenRequested(const QString &url, const QString &title, qint64 startMs);

    // Fired when we learn what the party is currently watching (for Continue Watching / Now Playing)
    void partyMediaAvailable(const QString &url, const QString &title, qint64 positionMs, qint64 durationMs);

    // Short description of what another member just did, for an on-screen notice
    void activity(const QString &text);

private:
    void onEvent(const QString &type, const QVariantMap &data, const QString &from);
    void onUserToggledPlay(bool playing, qint64 timeMs);
    void onUserSeeked(qint64 timeMs);
    void onPlayerStateChanged();
    void onSessionStateChanged();

    void handleOpen(const QVariantMap &data);
    void handleState(const QVariantMap &data);
    // Bring the local player to (timeMs, playing); a non-empty note is shown as an on-screen notice
    void align(qint64 timeMs, bool playing, const QString &note);
    void applySeek(qint64 timeMs);

    void sendState();
    void flushSeek();
    void armNeedState();
    void noteOpen(const QString &url, const QString &title);
    void resetParty();
    bool playerRunning() const;
    qint64 partyTimeMs() const;
    static QString formatTime(qint64 ms);

    TelepartySession *m_session = nullptr;
    VLCPlayer *m_player = nullptr;

    bool m_watching = false;

    // The video the whole party is on (empty = none yet)
    QString m_currentUrl;
    QString m_currentTitle;

    // Live party playback tracking (so a late joiner knows the party's current position)
    qint64 m_partyTimeMs = 0;
    qint64 m_partyDurationMs = 0;
    bool m_partyPlaying = true;
    QElapsedTimer m_partyClock;

    // Two members can open the same next episode at the same moment (autoplay); the second
    // "open" for the same URL within a few seconds is ignored
    QString m_lastOpenUrl;
    qint64 m_lastOpenAt = 0;

    // Waiting for somebody to tell us what the party is watching
    bool m_needState = false;
    QTimer m_stateTimer;

    // Position / pause state to apply once a video opened for catching up starts playing
    bool m_pendingSync = false;
    qint64 m_pendingTimeMs = 0;
    bool m_pendingPlaying = true;
    QElapsedTimer m_pendingClock;

    // Post-buffer alignment after a shared "open" so initial buffering differences cancel out
    bool m_syncOnPlay = false;
    bool m_announceOnPlay = false;

    // Local seeks are merged so a slider drag sends a few updates, not hundreds
    QTimer m_seekTimer;
    bool m_hasPendingSeek = false;
    qint64 m_pendingSeekMs = 0;

    QString m_prevSessionState;
};

} // namespace Flux
