#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>
#include <memory>

#include "../search/MediaLibrary.h"
#include "TelepartyEngine.h"

namespace Flux {

class TelepartySession;
class VLCPlayer;
class UserStore;

// Keeps every member's player in step using an authoritative timeline, 4-timestamp
// monotonic clock synchronization, future action scheduling, and a three-tier
// adaptive drift controller (deadband -> playback-rate nudge -> controlled hard seek).
//
// Events (all carried by TelepartySession::sendEvent within the 150-char Syncplay limit):
//   open    {id, n?, rev, ht}            opened a catalog video; resets timeline to 0
//   play    {t, rev, ht, r?}             resume at position t (ms) anchored at host time ht
//   pause   {t, rev, ht}                 pause at position t (ms) anchored at host time ht
//   seek    {t, rev, ht}                 jump to position t (ms) anchored at host time ht
//   state   {id, n?, t, d, p, rev, ht}   authoritative timeline snapshot for newcomers/resync
//   syncme  {}                           request current authoritative timeline snapshot
//   ts      {q, l?} / {p, q, r, s}       4-timestamp monotonic clock-offset & RTT exchange
//   rd      {s, t, rev}                  participant decoder/buffer readiness report
//
// Catalog streams are resolved locally. Local files are shared by descriptor only after each
// participant verifies a matching copy; filesystem paths never leave the device.
class TelepartySync : public QObject {
    Q_OBJECT

    // Set by the UI: true while the player page is on screen
    Q_PROPERTY(bool watching READ watching WRITE setWatching NOTIFY watchingChanged)
    Q_PROPERTY(bool partyPlaying READ partyPlaying NOTIFY partyPlayingChanged)
    Q_PROPERTY(QString currentUrl READ currentUrl NOTIFY currentMediaChanged)
    Q_PROPERTY(QString currentTitle READ currentTitle NOTIFY currentMediaChanged)
    Q_PROPERTY(bool hasPartyMedia READ hasPartyMedia NOTIFY currentMediaChanged)

    // Read-only observability & synchronization diagnostics
    Q_PROPERTY(qint64 smoothedRttMs READ smoothedRttMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 jitterMs READ jitterMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 clockOffsetMs READ clockOffsetMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 clockUncertaintyMs READ clockUncertaintyMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 scheduledLeadTimeMs READ scheduledLeadTimeMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 currentRevision READ currentRevision NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 authoritativePositionMs READ authoritativePositionMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 actualPositionMs READ actualPositionMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(qint64 signedDriftMs READ signedDriftMs NOTIFY diagnosticsChanged)
    Q_PROPERTY(double currentPlaybackRate READ currentPlaybackRate NOTIFY diagnosticsChanged)
    Q_PROPERTY(QString activeCorrectionMode READ activeCorrectionMode NOTIFY diagnosticsChanged)

    // Local Video Teleparty properties
    Q_PROPERTY(bool localMode READ isLocalSession NOTIFY localVerificationChanged)
    Q_PROPERTY(QString mediaMode READ mediaModeString NOTIFY localVerificationChanged)
    Q_PROPERTY(QString localVerificationState READ localVerificationStateString NOTIFY localVerificationChanged)
    Q_PROPERTY(QString localEpisodeTag READ localEpisodeTagString NOTIFY localVerificationChanged)
    Q_PROPERTY(QString localStatusText READ localStatusText NOTIFY localVerificationChanged)
    Q_PROPERTY(QString localErrorText READ localErrorText NOTIFY localVerificationChanged)
    Q_PROPERTY(bool allParticipantsReady READ areAllParticipantsReadyToSync NOTIFY localVerificationChanged)
    Q_PROPERTY(bool localPlaybackCommitted READ isLocalPlaybackCommitted NOTIFY localVerificationChanged)
    Q_PROPERTY(bool localReadinessTimedOut READ isLocalReadinessTimedOut NOTIFY localVerificationChanged)
    Q_PROPERTY(int readyParticipantCount READ readyParticipantCount NOTIFY localVerificationChanged)

public:
    static constexpr qint64 kDefaultLocalReadinessTimeoutMs = 45000;

    TelepartySync(TelepartySession *session, VLCPlayer *player, QObject *parent = nullptr);
    ~TelepartySync() override;

    void setUserStore(UserStore *store);
    UserStore* userStore() const { return m_userStore; }

    bool watching() const { return m_watching; }
    void setWatching(bool watching);
    bool partyPlaying() const { return m_timeline.isValid() ? m_timeline.snapshot().playing : m_partyPlaying; }

    QString currentUrl() const { return m_currentUrl; }
    QString currentTitle() const { return m_currentTitle; }
    bool hasPartyMedia() const {
        return !m_currentUrl.isEmpty() ||
               (m_mediaMode == TelepartyMediaMode::LocalFile && m_hostLocalDescriptor.isValidForSession());
    }

    // True for validated catalog http(s) URLs
    Q_INVOKABLE bool isStreamUrl(const QString &url) const;

    // Encodes a catalog stream URL into a compact movie/catalog ID (empty if not in local catalog)
    QString encodeCatalogMovieId(const QString &url) const;

    // Resolves and validates a remote event payload ({id, n} or {url, title}) against the local catalog
    bool resolveCatalogMovie(const QVariantMap &data, QString &outUrl, QString &outTitle) const;

    // Broadcast a catalog stream selected by the local user. Offline files are not shared.
    Q_INVOKABLE void broadcastOpen(const QString &url, const QString &title, qint64 startMs = 0);

    // Host declares or replaces the shared local video for a Local Teleparty session
    Q_INVOKABLE bool broadcastLocalOpen(const QString &fileUrlOrPath,
                                        const QString &title = QString(),
                                        qint64 knownDurationMs = 0);

    // Guest selects a local video from their library to verify against the host's LocalMediaDescriptor
    Q_INVOKABLE bool selectGuestLocalMedia(const QString &fileUrlOrPath,
                                           const QString &title = QString(),
                                           qint64 knownDurationMs = 0,
                                           bool isQuietAutoMatch = false);

    // Retries waiting for participant readiness (or re-requests state) after a readiness timeout
    Q_INVOKABLE void retryLocalReadinessWait();

    // Cancels the current Local Teleparty media wait and resets local media state
    Q_INVOKABLE void cancelLocalMediaSession();

    // The host was already watching something when the session started: make that the
    // shared video (nothing is sent; newcomers learn about it through "state" / "lopen")
    Q_INVOKABLE void adoptCurrent(const QString &url, const QString &title);

    // Open or catch up to the party's currently playing video at the party's live timestamp
    // (without broadcasting "open" or resetting other members)
    Q_INVOKABLE void joinPartyPlayback();

    // Server mismatch reporting & local playback override
    Q_INVOKABLE void reportServerMismatch(const QString &serverName, const QString &mediaTitle = QString());
    Q_INVOKABLE void setLocalPlaybackOverride(const QString &fileUrlOrPath);
    Q_INVOKABLE void clearLocalPlaybackOverride();

    // Local Teleparty state & diagnostics getters
    TelepartyMediaMode mediaMode() const { return m_mediaMode; }
    bool isLocalSession() const { return m_mediaMode == TelepartyMediaMode::LocalFile; }
    QString mediaModeString() const { return telepartyMediaModeToString(m_mediaMode); }
    LocalVerificationState localVerificationState() const { return m_localVerificationState; }
    QString localVerificationStateString() const { return localVerificationStateToString(m_localVerificationState); }
    QString localEpisodeTagString() const { return m_hostLocalDescriptor.episodeTag; }
    LocalCompatibilityReason localCompatibilityReason() const { return m_localCompatibilityReason; }
    QString localStatusText() const;
    QString localErrorText() const;

    const LocalMediaDescriptor &hostLocalDescriptor() const { return m_hostLocalDescriptor; }
    const LocalMediaDescriptor &guestLocalDescriptor() const { return m_guestLocalDescriptor; }
    ParticipantLocalStatus peerLocalStatus(const QString &peerId) const;
    bool areAllParticipantsReadyToSync() const;
    bool isLocalParticipantSynchronized() const;
    bool isLocalPlaybackCommitted() const { return m_localPlaybackCommitted; }
    bool isLocalReadinessTimedOut() const { return m_localReadinessWaitTimedOut; }
    int readyParticipantCount() const;

    void setLocalVerificationPolicy(const LocalVerificationPolicy &policy) { m_verificationPolicy = policy; }
    const LocalVerificationPolicy &localVerificationPolicy() const { return m_verificationPolicy; }

    void setLocalReadinessTimeoutMs(qint64 timeoutMs);
    qint64 localReadinessTimeoutMs() const { return m_localReadinessTimeoutMs; }

    void setForceAsyncInspection(bool async) { m_forceAsyncInspection = async; }
    bool forceAsyncInspection() const { return m_forceAsyncInspection; }

    // Observability & diagnostic getters
    qint64 smoothedRttMs() const { return m_clockEstimator.medianRttMs(); }
    qint64 jitterMs() const { return m_clockEstimator.jitterMs(); }
    qint64 clockOffsetMs() const { return m_clockEstimator.clockOffsetMs(); }
    qint64 clockUncertaintyMs() const { return m_clockEstimator.uncertaintyMs(); }
    qint64 scheduledLeadTimeMs() const;
    qint64 currentRevision() const { return m_timeline.revision(); }
    qint64 authoritativePositionMs() const;
    qint64 actualPositionMs() const;
    qint64 signedDriftMs() const { return m_driftController.lastSignedDriftMs(); }
    double currentPlaybackRate() const;
    QString activeCorrectionMode() const { return driftCorrectionModeToString(m_driftController.lastMode()); }

    SyncDiagnostics diagnostics() const;
    Q_INVOKABLE QVariantMap diagnosticsMap() const { return diagnostics().toVariantMap(); }

    // Engine & testability hooks
    void setMonotonicClock(std::shared_ptr<MonotonicClock> clock);
    std::shared_ptr<MonotonicClock> monotonicClock() const { return m_clock; }

    void setUseScheduledLeadTime(bool enabled) { m_useScheduledLeadTime = enabled; }
    bool useScheduledLeadTime() const { return m_useScheduledLeadTime; }

    const AuthoritativeTimeline &timeline() const { return m_timeline; }
    const ClockEstimator &clockEstimator() const { return m_clockEstimator; }
    ClockEstimator &clockEstimator() { return m_clockEstimator; }
    const ActionScheduler &scheduler() const { return m_scheduler; }
    const AdaptiveDriftController &driftController() const { return m_driftController; }
    AdaptiveDriftController &driftController() { return m_driftController; }

    ParticipantReadiness localReadiness() const { return m_localReadiness; }
    ParticipantReadiness peerReadiness(const QString &peerId) const {
        return m_peerReadiness.value(peerId, ParticipantReadiness::Idle);
    }

    // Explicit tick drivers (called by timers in production, or deterministically in tests)
    void sendClockPing();
    void processScheduledActions();
    void evaluateDriftTick();
    void stepSimulation();

signals:
    void watchingChanged();
    void partyPlayingChanged();
    void currentMediaChanged();
    void diagnosticsChanged();
    void localVerificationChanged();

    // Emitted when a guest needs to select the corresponding local video from their library
    void localMediaPromptRequested(const QString &expectedTitle, qint64 expectedDurationMs, const QString &expectedContainer);

    // Emitted quietly during candidate iteration if a trial candidate fails verification
    void localCandidateCheckFailed(const QString &reason);

    // Another member opened (or you are catching up with) a video: open it in the UI
    void remoteOpenRequested(const QString &url, const QString &title, qint64 startMs);

    // Fired when we learn what the party is currently watching (for Continue Watching / Now Playing)
    void partyMediaAvailable(const QString &url, const QString &title, qint64 positionMs, qint64 durationMs);

    // Short description of what another member just did, for an on-screen notice
    void activity(const QString &text);

private:
    class ScopedInternalAction {
    public:
        explicit ScopedInternalAction(int &depth) : m_depth(depth) { ++m_depth; }
        ~ScopedInternalAction() { --m_depth; }
    private:
        int &m_depth;
    };

    void onEvent(const QString &type, const QVariantMap &data, const QString &from);
    void onUserToggledPlay(bool playing, qint64 timeMs);
    void onUserSeeked(qint64 timeMs);
    void onPlayerStateChanged();
    void onPlayerBufferingChanged();
    void onSessionStateChanged();
    void onAuthorityChanged(const QString &authorityMemberId, bool localIsAuthority);

    void handleOpen(const QVariantMap &data, const QString &from);
    void handleState(const QVariantMap &data, const QString &from);
    void handlePlaybackTransition(const QString &type, const QVariantMap &data, const QString &from);
    void handleTimeSync(const QVariantMap &data, const QString &from);
    void handleReadiness(const QVariantMap &data, const QString &from);

    // Local Teleparty protocol handlers & helpers
    void handleLocalOpen(const QVariantMap &data, const QString &from);
    void handleLocalVerifyReport(const QVariantMap &data, const QString &from);
    void handleLocalStartCommit(const QVariantMap &data, const QString &from);
    void finishHostLocalInspection(const QString &resolvedUrl,
                                   const QString &resolvedLocalPath,
                                   LocalMediaDescriptor desc,
                                   LocalCompatibilityReason errReason);
    void finishGuestLocalInspection(const QString &resolvedUrl,
                                    const QString &resolvedLocalPath,
                                    LocalMediaDescriptor guestDesc,
                                    LocalCompatibilityReason errReason,
                                    bool isQuietAutoMatch = false);
    void scheduleLocalPlaybackResume();
    void updateLocalVerificationReadiness(bool allowBroadcast = true);
    void sendLocalVerificationReport();
    void tryCommitLocalPlaybackStart();
    void onLocalReadinessTimeout();
    void cancelActiveInspection();
    void clearLocalSessionState();

    void executeScheduledAction(const ScheduledAction &action);
    void updateScheduleTimer();
    void updateLocalReadiness(bool allowBroadcast = true);

    // Bring the local player to (timeMs, playing); a non-empty note is shown as an on-screen notice
    void align(qint64 timeMs, bool playing, const QString &note,
               ActionOrigin origin = ActionOrigin::ScheduledRemote);
    void applySeek(qint64 timeMs, const QString &note = QString(),
                   ActionOrigin origin = ActionOrigin::ScheduledRemote);

    void sendState();
    void flushSeek();
    void armNeedState();
    void noteOpen(const QString &url, const QString &title, qint64 rev, qint64 hostTimestampMs,
                  const QString &authorId, qint64 startMs = 0);
    void resetParty();
    bool playerRunning() const;
    qint64 partyTimeMs() const;
    qint64 localNowMs() const;
    qint64 estimatedHostNowMs() const;
    static QString formatTime(qint64 ms);

    QString iccftpBaseOrigin() const;
    QString iccftpSessionToken() const;
    QString iccftpHost() const;
    bool isSameOrMatchingMedia(const QString &playerUrl, const QString &currentUrl) const;

    TelepartySession *m_session = nullptr;
    VLCPlayer *m_player = nullptr;
    UserStore *m_userStore = nullptr;
    MediaLibraryModel m_catalog;

    std::shared_ptr<MonotonicClock> m_clock;
    AuthoritativeTimeline m_timeline;
    ClockEstimator m_clockEstimator;
    ActionScheduler m_scheduler;
    AdaptiveDriftController m_driftController;

    bool m_watching = false;
    int m_internalActionDepth = 0;
    bool m_useScheduledLeadTime = false;

    // The video the whole party is on (empty = none yet)
    QString m_currentUrl;
    QString m_currentTitle;

    // Fallback tracking when timeline is not yet anchored
    qint64 m_partyTimeMs = 0;
    qint64 m_partyDurationMs = 0;
    bool m_partyPlaying = true;
    qint64 m_partyUpdatedLocalMs = 0;

    // Re-anchoring state if "state" arrived before the first clock-sync sample locked
    bool m_reanchorStateOnFirstClockLock = false;
    qint64 m_rawStateHostTimestampMs = 0;
    qint64 m_rawStateBasePositionMs = 0;

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
    qint64 m_pendingStartedLocalMs = 0;

    // Post-buffer alignment after a shared "open" so initial buffering differences cancel out
    bool m_syncOnPlay = false;
    bool m_announceOnPlay = false;

    // Local seeks are merged so a slider drag sends a few updates, not hundreds
    QTimer m_seekTimer;
    bool m_hasPendingSeek = false;
    qint64 m_pendingSeekMs = 0;

    // Periodic timers for adaptive drift evaluation, clock sync, and future action execution
    QTimer m_driftTimer;
    QTimer m_clockSyncTimer;
    QTimer m_scheduleTimer;

    qint64 m_lastDriftTickLocalMs = 0;
    qint64 m_lastClockPingLocalMs = 0;
    qint64 m_lastSoftLogLocalMs = 0;
    qint64 m_lastReadinessSentLocalMs = 0;

    ParticipantReadiness m_localReadiness = ParticipantReadiness::Idle;
    QHash<QString, ParticipantReadiness> m_peerReadiness;

    // Local Video Teleparty state
    TelepartyMediaMode m_mediaMode = TelepartyMediaMode::OnlineCatalog;
    LocalVerificationPolicy m_verificationPolicy;
    LocalVerificationState m_localVerificationState = LocalVerificationState::None;
    LocalCompatibilityReason m_localCompatibilityReason = LocalCompatibilityReason::None;
    LocalMediaDescriptor m_hostLocalDescriptor;
    LocalMediaDescriptor m_guestLocalDescriptor;
    QString m_localMediaAuthorityPeerId;
    QString m_localFilePath;
    QHash<QString, ParticipantLocalStatus> m_peerLocalStatus;
    LocalMediaInspector::CancelToken m_activeInspectCancel;
    bool m_forceAsyncInspection = false;
    bool m_localPlaybackCommitted = false;
    bool m_localPlaybackAligned = false;
    bool m_hostSessionAlreadyStarted = false;
    bool m_localResumeCheckPending = false;
    bool m_localReadinessWaitTimedOut = false;
    qint64 m_localReadinessWaitStartedMs = 0;
    qint64 m_localReadinessTimeoutMs = kDefaultLocalReadinessTimeoutMs;
    QTimer m_localReadinessTimeoutTimer;

    QString m_prevSessionState;
    QString m_localPlaybackOverrideUrl;
};

} // namespace Flux

