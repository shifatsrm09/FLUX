#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVariantMap>

#include <atomic>
#include <functional>
#include <memory>

namespace Flux {

// ============================================================================
// 1. Monotonic Clock Abstraction
// ============================================================================
// Never use wall-clock timestamps (QDateTime) to measure elapsed playback time or
// network round-trip delay, as OS wall clocks are subject to NTP jumps and skew.
class MonotonicClock {
public:
    virtual ~MonotonicClock() = default;
    virtual qint64 nowMs() const = 0;
};

class SystemMonotonicClock final : public MonotonicClock {
public:
    SystemMonotonicClock() { m_timer.start(); }
    qint64 nowMs() const override { return m_timer.elapsed(); }

private:
    QElapsedTimer m_timer;
};

class SimulatedClock final : public MonotonicClock {
public:
    explicit SimulatedClock(qint64 initialMs = 10000)
        : m_nowMs(initialMs) {}

    qint64 nowMs() const override { return m_nowMs; }
    void setNowMs(qint64 nowMs) { m_nowMs = nowMs; }
    void advanceMs(qint64 deltaMs) {
        if (deltaMs > 0) {
            m_nowMs += deltaMs;
        }
    }

private:
    qint64 m_nowMs = 10000;
};

// ============================================================================
// 2. Origin Tracking & Participant Readiness
// ============================================================================
// Explicitly distinguishes local user intent from remote and automatic actions
// to guarantee zero feedback loops or rebroadcast storms.
enum class ActionOrigin {
    LocalUser,        // Direct local user action (UI button, shortcut, slider)
    ScheduledRemote,  // Action scheduled or executed from a remote peer event
    DriftCorrection,  // Automatic local rate adjustment or hard seek by drift controller
    BufferRecovery,   // Automatic local post-buffer / post-open alignment
    JoinSync          // Initial catch-up alignment upon joining or reconnecting
};

QString actionOriginToString(ActionOrigin origin);

enum class ParticipantReadiness {
    Idle,       // No shared media loaded
    Opening,    // Media opening / pre-rolling
    Buffering,  // Decoder cache < 100%
    Seeking,    // Seek in flight / seek guard active
    Ready,      // Playing or cleanly paused with ready buffer
    Error       // Playback error
};

QString readinessToCode(ParticipantReadiness readiness);
ParticipantReadiness readinessFromCode(const QString &code);
QString readinessToString(ParticipantReadiness readiness);

// ============================================================================
// 3. Authoritative Playback Timeline
// ============================================================================
// Synchronizes timelines rather than packet arrival times.
//
// For a playing timeline:
//   expectedPos(hostNow) = basePositionMs + round(max(0, hostNow - hostTimestampMs) * playbackRate)
// For a paused timeline:
//   expectedPos(hostNow) = basePositionMs
struct TimelineSnapshot {
    QString sessionId;
    qint64 revision = 0;               // Monotonically increasing state revision
    QString eventId;                   // Unique event ID ("<author>:<seq>")
    QString authorId;                  // Member who authored this revision
    QString mediaId;                   // Compact catalog movie ID
    QString mediaUrl;                  // Validated BDIX stream URL
    QString mediaTitle;                // Display title
    qint64 durationMs = 0;             // Media duration in ms (0 if unknown)
    qint64 basePositionMs = 0;         // Media position (ms) at hostTimestampMs
    qint64 hostTimestampMs = 0;        // Host monotonic timestamp (ms) when at basePositionMs
    bool playing = false;              // Intended playback state
    double playbackRate = 1.0;         // Intended timeline rate (1.0 = normal speed)
    bool valid = false;
};

class AuthoritativeTimeline {
public:
    static constexpr qint64 kMaxMediaPositionMs = 48LL * 3600 * 1000;
    static constexpr double kMinTimelineRate = 0.5;
    static constexpr double kMaxTimelineRate = 2.0;

    AuthoritativeTimeline() = default;

    void reset(const QString &sessionId = QString());

    bool isValid() const { return m_snapshot.valid; }
    const TimelineSnapshot &snapshot() const { return m_snapshot; }
    qint64 revision() const { return m_snapshot.revision; }
    qint64 nextRevision() const { return m_snapshot.revision + 1; }

    // Computes expected media position at the given host-clock timestamp
    qint64 expectedPositionMs(qint64 estimatedHostNowMs) const;

    // Deterministic revision & tie-break validation:
    // - Higher revision always wins.
    // - Equal revision from a different peer resolves deterministically in favor of
    //   the room host (hostPeerId), or lexicographically higher authorId if neither is host.
    // - Lower revision (or duplicate of current author+revision) is rejected.
    bool shouldAcceptRevision(qint64 incomingRevision,
                              const QString &incomingAuthor,
                              const QString &hostPeerId = QString()) const;

    // Applies validated state transitions
    bool update(const TimelineSnapshot &next, const QString &hostPeerId = QString());
    void forceUpdate(const TimelineSnapshot &next);

    // Re-anchors the timeline to a new host's monotonic clock during host migration
    // without causing any discontinuity in expected playback position.
    void migrateHostClock(const QString &newHostId,
                          qint64 oldEstimatedHostNowMs,
                          qint64 newHostLocalNowMs);

    static qint64 sanitizePositionMs(qint64 posMs, qint64 durationMs = 0);
    static double sanitizeRate(double rate);

private:
    TimelineSnapshot m_snapshot;
};

// ============================================================================
// 4. Four-Timestamp Clock-Offset and RTT Estimator
// ============================================================================
// Implements an NTP-style 4-timestamp clock exchange:
//   t1: Client local monotonic timestamp when request ("ts" ping) is sent
//   t2: Host monotonic timestamp when request is received
//   t3: Host monotonic timestamp when response ("ts" pong) is sent
//   t4: Client local monotonic timestamp when response is received
//
// Formulas & Sign Convention:
//   rttMs          = (t4 - t1) - (t3 - t2)
//   sampleOffsetMs = ((t2 - t1) + (t3 - t4)) / 2
//
//   estimatedHostTimeMs(localTimeMs) = localTimeMs + clockOffsetMs
//   localTimeForHostTimeMs(hostTimeMs) = hostTimeMs - clockOffsetMs
//
// Thus a positive clockOffsetMs means the Host's monotonic clock is ahead of the
// Client's monotonic clock (HostTime > LocalTime).
struct ClockSample {
    qint64 t1 = 0;
    qint64 t2 = 0;
    qint64 t3 = 0;
    qint64 t4 = 0;
    qint64 rttMs = 0;
    qint64 offsetMs = 0;
    qint64 receivedAtLocalMs = 0;
};

class ClockEstimator {
public:
    static constexpr int kWindowSize = 16;
    static constexpr qint64 kMaxPlausibleRttMs = 10000;
    static constexpr qint64 kMaxHostTurnaroundMs = 2000;
    static constexpr qint64 kMinLeadTimeMs = 80;
    static constexpr qint64 kMaxLeadTimeMs = 500;
    static constexpr qint64 kDefaultOneWayMs = 75;

    ClockEstimator() = default;

    void reset(bool isSelfAuthority = false, const QString &authorityPeerId = QString());
    // Preserves the last known smoothed offset during a brief socket reconnect to the same
    // authority while restarting the fast ping burst to refine it without a timeline jump.
    void softResetForReconnect();
    void setSelfAuthority(bool isSelfAuthority);
    bool isSelfAuthority() const { return m_isSelfAuthority; }

    QString authorityPeerId() const { return m_authorityPeerId; }
    void setAuthorityPeerId(const QString &peerId);

    // Records an outgoing ping at t1 so stale/unsolicited pongs can be validated
    void registerOutgoingPing(qint64 t1LocalMs);

    // Processes a 4-timestamp exchange sample. Returns true if valid and accepted.
    bool processSample(qint64 t1, qint64 t2, qint64 t3, qint64 t4);

    // Allows the host (whose own offset is 0) to track peer RTT reports for lead-time calculation
    void recordPeerRttSample(qint64 peerRttMs);

    // Time conversions using the smoothed clock offset
    qint64 estimatedHostTimeMs(qint64 localNowMs) const;
    qint64 localTimeForHostTimeMs(qint64 hostTimeMs) const;

    // Robust statistics
    bool hasEstimate() const { return m_isSelfAuthority || m_hasEstimate; }
    bool isLocked() const { return m_isSelfAuthority || m_samples.size() >= 4; }
    int sampleCount() const { return static_cast<int>(m_samples.size()); }
    int rejectedCount() const { return m_rejectedCount; }

    qint64 clockOffsetMs() const { return m_isSelfAuthority ? 0 : m_smoothedOffsetMs; }
    qint64 medianRttMs() const { return m_medianRttMs; }
    qint64 minRttMs() const { return m_minRttMs; }
    qint64 worstRttMs() const { return m_worstRttMs; }
    qint64 jitterMs() const { return m_jitterMs; }
    qint64 uncertaintyMs() const { return m_isSelfAuthority ? 0 : m_uncertaintyMs; }
    double confidence() const { return m_isSelfAuthority ? 1.0 : m_confidence; }

    // Adaptive ping interval: fast burst initially (1500 ms), slower when stable (10000 ms),
    // intermediate under high jitter (3500 ms)
    int recommendedPingIntervalMs() const;

    // Dynamic lead time for scheduling coordinated transitions
    qint64 recommendedLeadTimeMs(int activeMemberCount = 2) const;

private:
    void recomputeStatistics();

    bool m_isSelfAuthority = false;
    QString m_authorityPeerId;
    bool m_hasEstimate = false;

    QList<ClockSample> m_samples;
    QSet<qint64> m_pendingPings;
    QList<qint64> m_peerRttSamples;

    qint64 m_smoothedOffsetMs = 0;
    qint64 m_medianRttMs = 0;
    qint64 m_minRttMs = 0;
    qint64 m_worstRttMs = 0;
    qint64 m_jitterMs = 0;
    qint64 m_uncertaintyMs = 100;
    double m_confidence = 0.0;

    int m_rejectedCount = 0;
    int m_consecutiveOutliers = 0;
};

// ============================================================================
// 5. Future Action Scheduler & Late-Event Policy
// ============================================================================
enum class ScheduledActionType {
    Play,
    Pause,
    Seek,
    StateSync
};

struct ScheduledAction {
    qint64 revision = 0;
    QString eventId;
    QString authorId;
    ScheduledActionType type = ScheduledActionType::Play;
    qint64 targetPositionMs = 0;       // Authoritative media position at targetHostTimestampMs
    qint64 targetHostTimestampMs = 0;  // Target execution time on the Host's monotonic clock
    double playbackRate = 1.0;
    bool playing = false;
    ActionOrigin origin = ActionOrigin::ScheduledRemote;
    QString activityNote;
    qint64 enqueuedLocalMs = 0;
};

enum class ScheduleStatus {
    Queued,                // Scheduled for a future host timestamp
    ExecutedImmediately,   // Executed right away (targetHostTimestampMs ~= now)
    ExecutedLateCatchUp,   // Arrived after targetHostTimestampMs; late-event policy applied
    DuplicateIgnored,      // Duplicate eventId ignored
    StaleDropped           // Older revision dropped
};

class ActionScheduler {
public:
    static constexpr int kMaxPendingActions = 16;
    static constexpr int kMaxExecutedHistory = 64;
    static constexpr qint64 kMaxFutureWaitMs = 2000;
    static constexpr qint64 kLateThresholdMs = 15;

    ActionScheduler() = default;

    void reset();

    // Enqueues or immediately resolves a transition.
    // If the action is ready/late now, outImmediateAction is populated with the
    // late-adjusted target position and ready to apply.
    ScheduleStatus schedule(const ScheduledAction &action,
                            qint64 estimatedHostNowMs,
                            qint64 localNowMs,
                            ScheduledAction &outReadyAction,
                            const QString &hostPeerId = QString());

    // Extracts all actions whose targetHostTimestampMs <= estimatedHostNowMs
    QList<ScheduledAction> popDueActions(qint64 estimatedHostNowMs);

    // Returns milliseconds until the earliest pending action is due (-1 if queue is empty)
    qint64 nextWakeupDelayMs(qint64 estimatedHostNowMs) const;

    // Cancels all pending actions with revision <= upToRevision
    int cancelSuperseded(qint64 upToRevision);

    int pendingCount() const { return static_cast<int>(m_pending.size()); }
    qint64 highestRevision() const { return m_highestRevision; }
    QString highestRevisionAuthor() const { return m_highestAuthor; }

    int lateEventCount() const { return m_lateEventCount; }
    int supersededCount() const { return m_supersededCount; }
    int duplicateCount() const { return m_duplicateCount; }
    int staleDroppedCount() const { return m_staleDroppedCount; }

    // Computes late-adjusted media position when an action is executed after targetHostTimestampMs
    static ScheduledAction applyLatePolicy(const ScheduledAction &action, qint64 estimatedHostNowMs);

private:
    void recordExecutedEventId(const QString &eventId);
    bool isEventIdSeen(const QString &eventId) const;

    QList<ScheduledAction> m_pending;
    QList<QString> m_executedOrder;
    QSet<QString> m_executedSet;

    qint64 m_highestRevision = 0;
    QString m_highestAuthor;

    int m_lateEventCount = 0;
    int m_supersededCount = 0;
    int m_duplicateCount = 0;
    int m_staleDroppedCount = 0;
};

// ============================================================================
// 6. Three-Tier Adaptive Drift Controller
// ============================================================================
// Tier 1 (Deadband, |drift| <= deadbandMs):
//   No correction. Restores playback rate to 1.0x once drift converges inside
//   recoveryBandMs (hysteresis) or crosses zero.
// Tier 2 (Moderate drift, deadbandMs < |drift| <= softSeekThresholdMs):
//   Bounded proportional playback-rate adjustment (0.96x .. 1.04x) with hysteresis,
//   rate-change cooldown, and convergence watchdog. Automatically falls back to
//   soft seek if rate adjustment is unsupported or fails to reduce drift.
// Tier 3 (Large drift, |drift| > hardSeekThresholdMs):
//   Controlled hard seek when ready and seekable, respecting seek cooldown and
//   keyframe-granularity snapping.
struct PlayerObservation {
    qint64 actualPositionMs = 0;
    qint64 durationMs = 0;
    bool isPlaying = false;
    bool isPaused = false;
    bool isBuffering = false;
    bool isOpening = false;
    bool isSeekSettling = false;
    bool isSeekable = true;
    bool isEndedOrStopped = false;
    double currentRate = 1.0;
    bool rateControlSupported = true;
};

enum class DriftCorrectionMode {
    None,
    RateAdjust,
    RestoreRate,
    HardSeek,
    BufferRecoverySeek,
    SuppressedBuffering,
    SuppressedSeeking
};

QString driftCorrectionModeToString(DriftCorrectionMode mode);

struct DriftCorrectionDecision {
    DriftCorrectionMode mode = DriftCorrectionMode::None;
    ActionOrigin origin = ActionOrigin::DriftCorrection;
    qint64 expectedPositionMs = 0;
    qint64 actualPositionMs = 0;
    qint64 signedDriftMs = 0;      // actualPositionMs - expectedPositionMs (+ = local ahead, - = local behind)
    qint64 smoothedDriftMs = 0;
    double targetRate = 1.0;
    qint64 targetSeekMs = -1;
    bool shouldPause = false;
    bool shouldResume = false;
    bool shouldRestart = false;
};

class AdaptiveDriftController {
public:
    struct Config {
        qint64 deadbandMs = 80;              // Tier 1 deadband: ignore |drift| <= 80 ms
        qint64 recoveryBandMs = 35;          // Hysteresis inner band to restore 1.0x rate
        qint64 softSeekThresholdMs = 650;    // Tier 2 upper bound: rate adjust for 80..650 ms
        qint64 hardSeekThresholdMs = 650;    // Tier 3 threshold while playing: seek if |drift| > 650 ms
        qint64 pausedSeekThresholdMs = 250;  // Tier 3 threshold while paused: seek if |drift| > 250 ms
        qint64 fallbackSeekThresholdMs = 320;// Seek threshold when rate control is unsupported/ineffective
        double minRate = 0.96;               // Minimum rate during Tier 2 correction (-4%)
        double maxRate = 1.04;               // Maximum rate during Tier 2 correction (+4%)
        double rateTimeConstantMs = 6000.0;  // Proportional controller time constant
        double minRateStep = 0.004;          // Minimum rate delta before calling libVLC
        qint64 rateAdjustCooldownMs = 450;   // Cooldown between libVLC rate updates
        qint64 hardSeekCooldownMs = 2500;    // Cooldown between hard seeks to prevent thrashing
        qint64 postSeekSettleMs = 800;       // Settle grace window after a seek
        qint64 postBufferSettleMs = 300;     // Settle grace window after buffering completes
        qint64 rateFallbackWindowMs = 6500;  // Watchdog window to detect ineffective rate control
    };

    AdaptiveDriftController();
    explicit AdaptiveDriftController(const Config &config);

    void reset();
    void notifyExternalSeek(qint64 localNowMs, qint64 targetMs);
    void notifyRateControlFailed();

    const Config &config() const { return m_config; }
    void setConfig(const Config &config) { m_config = config; }

    DriftCorrectionDecision evaluate(const PlayerObservation &obs,
                                     qint64 expectedPositionMs,
                                     bool timelinePlaying,
                                     double timelineRate,
                                     qint64 clockUncertaintyMs,
                                     qint64 localNowMs);

    qint64 lastSignedDriftMs() const { return m_lastSignedDriftMs; }
    qint64 smoothedDriftMs() const { return std::llround(m_smoothedDriftMs); }
    DriftCorrectionMode lastMode() const { return m_lastMode; }
    bool isRateAdjustmentActive() const { return m_rateAdjustActive; }
    bool isRateFallbackActive() const { return m_rateFallbackTriggered; }

    int softCorrectionCount() const { return m_softCorrectionCount; }
    int hardSeekCount() const { return m_hardSeekCount; }
    int bufferRecoveryCount() const { return m_bufferRecoveryCount; }

private:
    Config m_config;

    bool m_hasDriftSample = false;
    qint64 m_lastSignedDriftMs = 0;
    double m_smoothedDriftMs = 0.0;
    DriftCorrectionMode m_lastMode = DriftCorrectionMode::None;

    bool m_wasBuffering = false;
    bool m_pendingBufferRecovery = false;
    qint64 m_bufferRecoveredAtMs = 0;

    bool m_rateAdjustActive = false;
    qint64 m_rateAdjustStartedAtMs = 0;
    qint64 m_rateAdjustStartDriftMs = 0;
    qint64 m_lastRateChangeAtMs = 0;
    int m_outsideDeadbandStreak = 0;
    bool m_rateFallbackTriggered = false;

    qint64 m_lastSeekAtMs = 0;
    qint64 m_lastSeekTargetMs = -1;
    qint64 m_lastPreSeekAbsDriftMs = 0;

    int m_softCorrectionCount = 0;
    int m_hardSeekCount = 0;
    int m_bufferRecoveryCount = 0;
};

// ============================================================================
// 7. Observability & Diagnostics Snapshot
// ============================================================================
struct SyncDiagnostics {
    qint64 smoothedRttMs = 0;
    qint64 minRttMs = 0;
    qint64 worstRttMs = 0;
    qint64 jitterMs = 0;
    qint64 clockOffsetMs = 0;
    qint64 clockUncertaintyMs = 0;
    double clockConfidence = 0.0;
    bool clockLocked = false;
    int clockSampleCount = 0;
    int rejectedSampleCount = 0;

    qint64 scheduledLeadTimeMs = 0;
    int pendingScheduledCount = 0;

    qint64 currentRevision = 0;
    QString authorityPeerId;
    QString localReadiness;
    qint64 authoritativePositionMs = 0;
    qint64 actualPositionMs = 0;
    qint64 signedDriftMs = 0;
    qint64 smoothedDriftMs = 0;
    double currentPlaybackRate = 1.0;
    QString activeCorrectionMode;

    int softCorrectionCount = 0;
    int hardSeekCorrectionCount = 0;
    int bufferRecoveryCount = 0;
    int lateEventCount = 0;
    int supersededActionCount = 0;
    int duplicateEventCount = 0;
    int staleRevisionDropCount = 0;

    QVariantMap toVariantMap() const;
};

// ============================================================================
// 8. Local Video Teleparty Verification & Media Matching
// ============================================================================
enum class TelepartyMediaMode {
    OnlineCatalog = 0, // Streamed from shared BDIX catalog URL
    LocalFile = 1      // Each participant plays their own verified local copy
};

QString telepartyMediaModeToString(TelepartyMediaMode mode);

enum class LocalVerificationState {
    None = 0,                 // Not in a Local Teleparty session
    WaitingForLocalSelection, // Host declared local media; guest has not selected a local file yet
    InspectingLocalMedia,     // Extracting local media metadata / content fingerprint
    VerifiedCompatible,       // Local file verified compatible with host's descriptor
    IncompatibleMedia,        // Verification failed (duration, title, or fingerprint mismatch)
    UnresolvedMetadata,       // Could not extract sufficient metadata/duration to verify safely
    OpeningMedia,             // Verified file is opening in libVLC
    BufferingMedia,           // Verified file is buffering / seeking to initial position
    ReadyToSync,              // File open, playable, seekable, verified, and ready for timeline sync
    PlaybackError             // Local file missing, unreadable, unsupported, or failed to decode
};

QString localVerificationStateToCode(LocalVerificationState state);
LocalVerificationState localVerificationStateFromCode(const QString &code);
QString localVerificationStateToString(LocalVerificationState state);

enum class LocalCompatibilityReason {
    None = 0,
    MatchedExact,
    MatchedRemuxOrReencode,
    MissingFile,
    UnreadableOrEmptyFile,
    UnsupportedContainer,
    UnplayableMedia,
    InsufficientMetadata,
    TitleMismatch,
    EpisodeMismatch,
    EditionMismatch,
    DurationMismatch,
    FingerprintMismatch,
    StrictHashMismatch,
    InvalidSessionDescriptor
};

QString localCompatibilityReasonToCode(LocalCompatibilityReason reason);
LocalCompatibilityReason localCompatibilityReasonFromCode(const QString &code);
QString localCompatibilityReasonToMessage(LocalCompatibilityReason reason);

struct LocalVerificationPolicy {
    // Default 2000 ms (2.0 s) duration tolerance accounts for container muxing padding,
    // audio pre-roll / trailing silence differences, and MKV/MP4 timestamp rounding while
    // rejecting different cuts (Theatrical vs Extended), wrong episodes, or truncated files.
    qint64 durationToleranceMs = 2000;
    bool allowDifferentContainers = true;
    bool allowDifferentFileSizes = true;
    bool requireContentFingerprint = true;
    bool allowSameTitleAndDurationWhenRemuxed = false;
    bool requireExactFullFileHash = false;
};

struct LocalMediaDescriptor {
    QString sessionId;           // Session-scoped identifier
    QString mediaId;             // Compact session-scoped shared media ID ("LM_<16hex>")
    qint64 revision = 0;         // Session media revision
    QString displayTitle;        // Human-readable title (never a filesystem path)
    QString normalizedTitle;     // Case-folded, separator-normalized core title
    QString episodeTag;          // Normalized season/episode token if episodic (e.g. "s01e02")
    QString yearTag;             // 4-digit release year if present in filename/title (e.g. "2014")
    QString editionTag;          // Normalized cut/edition tag if present ("extended", "directors cut", "imax")
    qint64 durationMs = 0;       // Media duration in milliseconds (> 0 required for verification)
    qint64 fileSizeBytes = 0;    // Local file size in bytes
    QString containerFormat;     // Normalized uppercase extension/container ("MKV", "MP4", "AVI", "MOV", "WEBM", "TS")
    QString videoCodec;          // Optional video codec identifier (e.g. "H264", "HEVC", "AV1")
    QString audioCodec;          // Optional audio codec identifier (e.g. "AAC", "EAC3", "OPUS")
    int width = 0;               // Optional video width
    int height = 0;              // Optional video height
    double frameRate = 0.0;      // Optional video frame rate
    qint64 streamStartOffsetMs = 0; // Container start offset if non-zero
    // Versioned multi-window content fingerprint:
    //   "v1:<windowCount>:<windowBytes>:<byteDigest16>:<payloadDigest16>"
    QString contentFingerprint;
    QString fullFileSha256;      // Optional full-file SHA-256 hex digest (strict mode)
    qint64 lastModifiedMs = 0;   // Local-only mtime for cache invalidation (never transmitted)

    bool isValidForSession() const;
    QJsonObject toNetworkJson() const;
    static LocalMediaDescriptor fromNetworkJson(const QJsonObject &obj);
    QVariantMap toVariantMap() const;

    // Generates a privacy-safe, session-scoped media identity from the descriptor
    static QString generateSessionMediaId(const QString &sessionId,
                                          qint64 revision,
                                          const LocalMediaDescriptor &desc);

    // Privacy guard: returns true if a string or JSON object contains local filesystem paths,
    // drive letters ("C:"), UNC paths, or file:// URLs that must never be transmitted.
    static bool containsPrivatePathLeak(const QString &text);
    static bool containsPrivatePathLeakInJson(const QJsonObject &obj);
};

struct LocalCompatibilityResult {
    LocalVerificationState state = LocalVerificationState::None;
    LocalCompatibilityReason reason = LocalCompatibilityReason::None;
    qint64 durationDeltaMs = 0;
    bool exactContentMatch = false;
    bool payloadEssenceMatch = false;
    bool titleMatched = false;
    bool durationWithinTolerance = false;
    QString userMessage;

    bool isCompatible() const {
        return state == LocalVerificationState::VerifiedCompatible ||
               state == LocalVerificationState::OpeningMedia ||
               state == LocalVerificationState::BufferingMedia ||
               state == LocalVerificationState::ReadyToSync;
    }
};

struct ParticipantLocalStatus {
    QString peerId;
    QString mediaId;
    qint64 revision = 0;
    LocalVerificationState state = LocalVerificationState::WaitingForLocalSelection;
    LocalCompatibilityReason reason = LocalCompatibilityReason::None;
    qint64 durationDeltaMs = 0;
    qint64 updatedAtLocalMs = 0;
};

class LocalMediaInspector {
public:
    static constexpr int kDefaultWindowCount = 5;
    static constexpr qint64 kDefaultWindowBytes = 65536; // 64 KB per sample window
    using CancelToken = std::shared_ptr<std::atomic<bool>>;
    using InspectCallback = std::function<void(LocalMediaDescriptor, LocalCompatibilityReason)>;

    // Normalizes a filename or title into (normalizedTitle, displayTitle, episodeTag, yearTag, editionTag)
    static void parseTitleAndTags(const QString &rawNameOrTitle,
                                  QString &outNormalizedTitle,
                                  QString &outDisplayTitle,
                                  QString &outEpisodeTag,
                                  QString &outYearTag,
                                  QString &outEditionTag);

    static QString normalizeContainerFormat(const QString &filePathOrExt);
    static bool isSupportedContainer(const QString &containerFormat);

    // Computes the versioned multi-window content fingerprint ("v1:5:65536:<byteDigest>:<payloadDigest>")
    // without blocking on full-file reads for multi-gigabyte videos.
    static QString computeMultiWindowFingerprint(const QString &filePath,
                                                 int windowCount = kDefaultWindowCount,
                                                 qint64 windowBytes = kDefaultWindowBytes,
                                                 CancelToken cancelToken = nullptr);

    static QString computeFullFileSha256(const QString &filePath,
                                         CancelToken cancelToken = nullptr);

    // Extracts container duration (MP4/MOV mvhd, MKV/WebM EBML Segment Info, AVI avih)
    // and stream metadata directly from the local file header.
    static LocalMediaDescriptor inspectFile(const QString &filePath,
                                            const QString &titleHint = QString(),
                                            qint64 knownDurationMs = 0,
                                            bool computeFullHash = false,
                                            LocalCompatibilityReason *outErrorReason = nullptr,
                                            CancelToken cancelToken = nullptr,
                                            bool useCache = true,
                                            bool computeFingerprint = true);

    // Asynchronous inspection on a worker thread with cancellation support
    static CancelToken inspectFileAsync(QObject *context,
                                        const QString &filePath,
                                        const QString &titleHint,
                                        qint64 knownDurationMs,
                                        bool computeFullHash,
                                        InspectCallback callback,
                                        bool computeFingerprint = true);

    // Returns true if the file on disk has been deleted, replaced, or modified since descriptor extraction
    static bool hasFileChangedSince(const QString &filePath, const LocalMediaDescriptor &desc);
    static void clearCache();
};

class LocalMediaVerifier {
public:
    static LocalCompatibilityResult verify(const LocalMediaDescriptor &hostDesc,
                                           const LocalMediaDescriptor &guestDesc,
                                           const LocalVerificationPolicy &policy = LocalVerificationPolicy{});
};

} // namespace Flux

