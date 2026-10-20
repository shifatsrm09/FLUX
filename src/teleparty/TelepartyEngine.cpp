#include "TelepartyEngine.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonValue>
#include <QMetaObject>
#include <QMutexLocker>
#include <QPointer>
#include <QRegularExpression>
#include <QThreadPool>
#include <QUrl>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace Flux {

// ============================================================================
// Origin & Readiness Helpers
// ============================================================================

QString actionOriginToString(ActionOrigin origin) {
    switch (origin) {
    case ActionOrigin::LocalUser:       return QStringLiteral("LocalUser");
    case ActionOrigin::ScheduledRemote: return QStringLiteral("ScheduledRemote");
    case ActionOrigin::DriftCorrection: return QStringLiteral("DriftCorrection");
    case ActionOrigin::BufferRecovery:  return QStringLiteral("BufferRecovery");
    case ActionOrigin::JoinSync:        return QStringLiteral("JoinSync");
    }
    return QStringLiteral("Unknown");
}

QString readinessToCode(ParticipantReadiness readiness) {
    switch (readiness) {
    case ParticipantReadiness::Idle:      return QStringLiteral("i");
    case ParticipantReadiness::Opening:   return QStringLiteral("o");
    case ParticipantReadiness::Buffering: return QStringLiteral("b");
    case ParticipantReadiness::Seeking:   return QStringLiteral("s");
    case ParticipantReadiness::Ready:     return QStringLiteral("r");
    case ParticipantReadiness::Error:     return QStringLiteral("e");
    }
    return QStringLiteral("i");
}

ParticipantReadiness readinessFromCode(const QString &code) {
    if (code == QLatin1String("o")) return ParticipantReadiness::Opening;
    if (code == QLatin1String("b")) return ParticipantReadiness::Buffering;
    if (code == QLatin1String("s")) return ParticipantReadiness::Seeking;
    if (code == QLatin1String("r")) return ParticipantReadiness::Ready;
    if (code == QLatin1String("e")) return ParticipantReadiness::Error;
    return ParticipantReadiness::Idle;
}

QString readinessToString(ParticipantReadiness readiness) {
    switch (readiness) {
    case ParticipantReadiness::Idle:      return QStringLiteral("Idle");
    case ParticipantReadiness::Opening:   return QStringLiteral("Opening");
    case ParticipantReadiness::Buffering: return QStringLiteral("Buffering");
    case ParticipantReadiness::Seeking:   return QStringLiteral("Seeking");
    case ParticipantReadiness::Ready:     return QStringLiteral("Ready");
    case ParticipantReadiness::Error:     return QStringLiteral("Error");
    }
    return QStringLiteral("Idle");
}

QString driftCorrectionModeToString(DriftCorrectionMode mode) {
    switch (mode) {
    case DriftCorrectionMode::None:                return QStringLiteral("None");
    case DriftCorrectionMode::RateAdjust:          return QStringLiteral("RateAdjust");
    case DriftCorrectionMode::RestoreRate:         return QStringLiteral("RestoreRate");
    case DriftCorrectionMode::HardSeek:            return QStringLiteral("HardSeek");
    case DriftCorrectionMode::BufferRecoverySeek:  return QStringLiteral("BufferRecoverySeek");
    case DriftCorrectionMode::SuppressedBuffering: return QStringLiteral("SuppressedBuffering");
    case DriftCorrectionMode::SuppressedSeeking:   return QStringLiteral("SuppressedSeeking");
    }
    return QStringLiteral("None");
}

// ============================================================================
// AuthoritativeTimeline
// ============================================================================

void AuthoritativeTimeline::reset(const QString &sessionId) {
    m_snapshot = TimelineSnapshot{};
    m_snapshot.sessionId = sessionId;
}

qint64 AuthoritativeTimeline::sanitizePositionMs(qint64 posMs, qint64 durationMs) {
    const qint64 maxAllowed = (durationMs > 0 && durationMs <= kMaxMediaPositionMs)
                                  ? durationMs
                                  : kMaxMediaPositionMs;
    return std::clamp<qint64>(posMs, 0, maxAllowed);
}

double AuthoritativeTimeline::sanitizeRate(double rate) {
    if (!std::isfinite(rate) || rate <= 0.0) {
        return 1.0;
    }
    return std::clamp(rate, kMinTimelineRate, kMaxTimelineRate);
}

qint64 AuthoritativeTimeline::expectedPositionMs(qint64 estimatedHostNowMs) const {
    if (!m_snapshot.valid) {
        return 0;
    }

    const qint64 basePos = sanitizePositionMs(m_snapshot.basePositionMs, m_snapshot.durationMs);
    if (!m_snapshot.playing) {
        return basePos;
    }

    const qint64 elapsedHostMs = std::max<qint64>(0, estimatedHostNowMs - m_snapshot.hostTimestampMs);
    const double rate = sanitizeRate(m_snapshot.playbackRate);
    const qint64 advancedMs = std::llround(static_cast<double>(elapsedHostMs) * rate);
    return sanitizePositionMs(basePos + advancedMs, m_snapshot.durationMs);
}

bool AuthoritativeTimeline::shouldAcceptRevision(qint64 incomingRevision,
                                                 const QString &incomingAuthor,
                                                 const QString &hostPeerId) const {
    if (incomingRevision <= 0) {
        return false;
    }
    if (!m_snapshot.valid) {
        return true;
    }
    if (incomingRevision > m_snapshot.revision) {
        return true;
    }
    if (incomingRevision < m_snapshot.revision) {
        return false;
    }

    // Equal revision: reject if from the same author (duplicate)
    if (incomingAuthor.isEmpty() || incomingAuthor == m_snapshot.authorId) {
        return false;
    }

    // Deterministic tie-breaker for concurrent equal revisions:
    // 1. The room host always wins over a guest.
    if (!hostPeerId.isEmpty()) {
        if (incomingAuthor == hostPeerId && m_snapshot.authorId != hostPeerId) {
            return true;
        }
        if (m_snapshot.authorId == hostPeerId && incomingAuthor != hostPeerId) {
            return false;
        }
    }

    // 2. Otherwise lexicographically higher member ID wins deterministically on all nodes.
    return incomingAuthor > m_snapshot.authorId;
}

bool AuthoritativeTimeline::update(const TimelineSnapshot &next, const QString &hostPeerId) {
    if (!shouldAcceptRevision(next.revision, next.authorId, hostPeerId)) {
        return false;
    }
    forceUpdate(next);
    return true;
}

void AuthoritativeTimeline::forceUpdate(const TimelineSnapshot &next) {
    m_snapshot = next;
    m_snapshot.durationMs = std::clamp<qint64>(m_snapshot.durationMs, 0, kMaxMediaPositionMs);
    m_snapshot.basePositionMs = sanitizePositionMs(m_snapshot.basePositionMs, m_snapshot.durationMs);
    m_snapshot.playbackRate = sanitizeRate(m_snapshot.playbackRate);
    m_snapshot.valid = !m_snapshot.mediaUrl.isEmpty() || !m_snapshot.mediaId.isEmpty();
}

void AuthoritativeTimeline::migrateHostClock(const QString &newHostId,
                                             qint64 oldEstimatedHostNowMs,
                                             qint64 newHostLocalNowMs) {
    if (!m_snapshot.valid) {
        m_snapshot.authorId = newHostId;
        return;
    }

    const qint64 seamlessPos = expectedPositionMs(oldEstimatedHostNowMs);
    m_snapshot.basePositionMs = seamlessPos;
    m_snapshot.hostTimestampMs = newHostLocalNowMs;
    m_snapshot.authorId = newHostId;
    m_snapshot.revision += 1;
}

// ============================================================================
// ClockEstimator
// ============================================================================

void ClockEstimator::reset(bool isSelfAuthority, const QString &authorityPeerId) {
    m_isSelfAuthority = isSelfAuthority;
    m_authorityPeerId = authorityPeerId;
    m_hasEstimate = false;
    m_samples.clear();
    m_pendingPings.clear();
    m_peerRttSamples.clear();
    m_smoothedOffsetMs = 0;
    m_medianRttMs = 0;
    m_minRttMs = 0;
    m_worstRttMs = 0;
    m_jitterMs = 0;
    m_uncertaintyMs = isSelfAuthority ? 0 : 100;
    m_confidence = isSelfAuthority ? 1.0 : 0.0;
    m_rejectedCount = 0;
    m_consecutiveOutliers = 0;
}

void ClockEstimator::softResetForReconnect() {
    m_samples.clear();
    m_pendingPings.clear();
    m_consecutiveOutliers = 0;
    if (!m_isSelfAuthority) {
        m_confidence = m_hasEstimate ? 0.5 : 0.0;
    }
}

void ClockEstimator::setSelfAuthority(bool isSelfAuthority) {
    if (m_isSelfAuthority == isSelfAuthority) {
        return;
    }
    m_isSelfAuthority = isSelfAuthority;
    if (m_isSelfAuthority) {
        m_smoothedOffsetMs = 0;
        m_uncertaintyMs = 0;
        m_confidence = 1.0;
    }
}

void ClockEstimator::setAuthorityPeerId(const QString &peerId) {
    if (m_authorityPeerId == peerId) {
        return;
    }
    const bool wasAuthority = m_isSelfAuthority;
    reset(wasAuthority, peerId);
}

void ClockEstimator::registerOutgoingPing(qint64 t1LocalMs) {
    if (t1LocalMs <= 0) return;
    if (m_pendingPings.size() >= 32) {
        // Evict oldest ping entries if unanswered
        qint64 minPing = *std::min_element(m_pendingPings.begin(), m_pendingPings.end());
        m_pendingPings.remove(minPing);
    }
    m_pendingPings.insert(t1LocalMs);
}

bool ClockEstimator::processSample(qint64 t1, qint64 t2, qint64 t3, qint64 t4) {
    // 1. Basic structural & monotonicity validation
    if (t1 <= 0 || t4 <= 0 || t2 <= 0 || t3 <= 0) {
        ++m_rejectedCount;
        return false;
    }
    if (t4 < t1 || t3 < t2) {
        ++m_rejectedCount;
        return false;
    }

    const qint64 hostTurnaroundMs = t3 - t2;
    if (hostTurnaroundMs > kMaxHostTurnaroundMs) {
        ++m_rejectedCount;
        return false;
    }

    const qint64 rttMs = (t4 - t1) - hostTurnaroundMs;
    if (rttMs < 0 || rttMs > kMaxPlausibleRttMs) {
        ++m_rejectedCount;
        return false;
    }

    // If outgoing pings are being tracked, verify t1 was not already consumed or forged
    if (!m_pendingPings.isEmpty()) {
        if (!m_pendingPings.contains(t1)) {
            ++m_rejectedCount;
            return false;
        }
        m_pendingPings.remove(t1);
    }

    // 2. Outlier spike rejection when a stable window already exists:
    //    Single high-RTT spikes caused by TCP head-of-line blocking should not skew the clock.
    //    However, if 3 consecutive samples show higher RTT, adapt to the new network route.
    if (m_samples.size() >= 4 && m_medianRttMs > 0) {
        const qint64 outlierLimit = std::max<qint64>(600, m_medianRttMs * 4 + 250);
        if (rttMs > outlierLimit) {
            ++m_consecutiveOutliers;
            if (m_consecutiveOutliers < 3) {
                ++m_rejectedCount;
                return false;
            }
        } else {
            m_consecutiveOutliers = 0;
        }
    }

    // 3. Compute 4-timestamp NTP offset:
    //    offset = ((t2 - t1) + (t3 - t4)) / 2
    //    where estimatedHostTime = localTime + offset
    const qint64 offsetMs = std::llround(static_cast<double>((t2 - t1) + (t3 - t4)) / 2.0);

    ClockSample sample;
    sample.t1 = t1;
    sample.t2 = t2;
    sample.t3 = t3;
    sample.t4 = t4;
    sample.rttMs = rttMs;
    sample.offsetMs = offsetMs;
    sample.receivedAtLocalMs = t4;

    m_samples.append(sample);
    while (m_samples.size() > kWindowSize) {
        m_samples.removeFirst();
    }

    recomputeStatistics();
    return true;
}

void ClockEstimator::recordPeerRttSample(qint64 peerRttMs) {
    if (peerRttMs <= 0 || peerRttMs > kMaxPlausibleRttMs) {
        return;
    }
    m_peerRttSamples.append(peerRttMs);
    while (m_peerRttSamples.size() > kWindowSize) {
        m_peerRttSamples.removeFirst();
    }

    if (m_isSelfAuthority && m_samples.isEmpty()) {
        QList<qint64> sorted = m_peerRttSamples;
        std::sort(sorted.begin(), sorted.end());
        const int n = static_cast<int>(sorted.size());
        m_minRttMs = sorted.front();
        m_worstRttMs = sorted.back();
        m_medianRttMs = (n % 2 == 1)
                            ? sorted[n / 2]
                            : (sorted[n / 2 - 1] + sorted[n / 2]) / 2;

        qint64 diffSum = 0;
        for (int i = 1; i < n; ++i) {
            diffSum += std::llabs( m_peerRttSamples[i] - m_peerRttSamples[i - 1] );
        }
        m_jitterMs = (n > 1) ? (diffSum / (n - 1)) : 0;
    }
}

void ClockEstimator::recomputeStatistics() {
    if (m_samples.isEmpty()) {
        return;
    }

    const int n = static_cast<int>(m_samples.size());
    QList<ClockSample> byRtt = m_samples;
    std::sort(byRtt.begin(), byRtt.end(), [](const ClockSample &a, const ClockSample &b) {
        return a.rttMs < b.rttMs;
    });

    m_minRttMs = byRtt.front().rttMs;
    m_worstRttMs = byRtt.back().rttMs;
    m_medianRttMs = (n % 2 == 1)
                        ? byRtt[n / 2].rttMs
                        : (byRtt[n / 2 - 1].rttMs + byRtt[n / 2].rttMs) / 2;

    // Compute inter-sample RTT jitter (mean absolute consecutive RTT delta)
    if (n > 1) {
        qint64 totalDelta = 0;
        for (int i = 1; i < n; ++i) {
            totalDelta += std::llabs(m_samples[i].rttMs - m_samples[i - 1].rttMs);
        }
        m_jitterMs = totalDelta / (n - 1);
    } else {
        m_jitterMs = 0;
    }

    // Prefer lowest-RTT samples for clock-offset estimation:
    // Queueing delay only adds positive delay, so the lowest-RTT half of samples
    // experienced the least queuing asymmetry.
    const int bestCount = std::max(1, (n + 1) / 2);
    QList<qint64> bestOffsets;
    bestOffsets.reserve(bestCount);
    for (int i = 0; i < bestCount; ++i) {
        bestOffsets.append(byRtt[i].offsetMs);
    }
    std::sort(bestOffsets.begin(), bestOffsets.end());

    const qint64 targetOffsetMs = (bestCount % 2 == 1)
                                      ? bestOffsets[bestCount / 2]
                                      : (bestOffsets[bestCount / 2 - 1] + bestOffsets[bestCount / 2]) / 2;

    // Median Absolute Deviation (MAD) of offsets in the low-RTT subset
    qint64 madSum = 0;
    for (qint64 off : bestOffsets) {
        madSum += std::llabs(off - targetOffsetMs);
    }
    const qint64 offsetMadMs = madSum / bestCount;

    // Smooth clock-offset updates so single noisy samples never jump the timeline
    if (!m_hasEstimate) {
        m_smoothedOffsetMs = targetOffsetMs;
        m_hasEstimate = true;
    } else {
        const qint64 delta = targetOffsetMs - m_smoothedOffsetMs;
        if (n <= 2 && std::llabs(delta) > 500) {
            // During initial lock, snap quickly to the low-RTT median
            m_smoothedOffsetMs = targetOffsetMs;
        } else {
            const double alpha = (n < 4) ? 0.50 : 0.30;
            qint64 step = std::llround(static_cast<double>(delta) * alpha);
            // Slew-rate clamp in steady state to prevent sudden jumps
            if (n >= 4) {
                step = std::clamp<qint64>(step, -150, 150);
            }
            if (step == 0 && delta != 0) {
                step = (delta > 0) ? 1 : -1;
            }
            m_smoothedOffsetMs += step;
        }
    }

    // Uncertainty combines minimum one-way delay dispersion, RTT jitter, and offset MAD
    m_uncertaintyMs = std::max<qint64>(5, (m_minRttMs / 4) + m_jitterMs + offsetMadMs);

    // Confidence scales from 0.25 (1 sample) up to 1.0 (>= 4 samples with low jitter)
    const double sampleFactor = std::min(1.0, static_cast<double>(n) / 4.0);
    const double jitterPenalty = std::clamp(1.0 - (static_cast<double>(m_jitterMs) / 300.0), 0.25, 1.0);
    m_confidence = sampleFactor * jitterPenalty;
}

qint64 ClockEstimator::estimatedHostTimeMs(qint64 localNowMs) const {
    if (m_isSelfAuthority || !m_hasEstimate) {
        return localNowMs;
    }
    return localNowMs + m_smoothedOffsetMs;
}

qint64 ClockEstimator::localTimeForHostTimeMs(qint64 hostTimeMs) const {
    if (m_isSelfAuthority || !m_hasEstimate) {
        return hostTimeMs;
    }
    return hostTimeMs - m_smoothedOffsetMs;
}

int ClockEstimator::recommendedPingIntervalMs() const {
    if (m_samples.size() < 4) {
        return 1500; // Initial burst until locked
    }
    if (m_jitterMs > 60 || m_confidence < 0.70) {
        return 3500; // Elevated sampling under high jitter
    }
    return 10000;    // Low steady-state overhead within 150-char chat relay
}

qint64 ClockEstimator::recommendedLeadTimeMs(int activeMemberCount) const {
    if (activeMemberCount <= 1) {
        return 0;
    }

    const qint64 oneWayMs = (m_medianRttMs > 0) ? (m_medianRttMs / 2) : kDefaultOneWayMs;
    const qint64 rawLeadMs = oneWayMs + m_jitterMs + (uncertaintyMs() / 4) + 30;
    return std::clamp<qint64>(rawLeadMs, kMinLeadTimeMs, kMaxLeadTimeMs);
}

// ============================================================================
// ActionScheduler
// ============================================================================

void ActionScheduler::reset() {
    m_pending.clear();
    m_executedOrder.clear();
    m_executedSet.clear();
    m_highestRevision = 0;
    m_highestAuthor.clear();
    m_lateEventCount = 0;
    m_supersededCount = 0;
    m_duplicateCount = 0;
    m_staleDroppedCount = 0;
}

bool ActionScheduler::isEventIdSeen(const QString &eventId) const {
    if (eventId.isEmpty()) return false;
    if (m_executedSet.contains(eventId)) return true;
    for (const ScheduledAction &item : m_pending) {
        if (item.eventId == eventId) return true;
    }
    return false;
}

void ActionScheduler::recordExecutedEventId(const QString &eventId) {
    if (eventId.isEmpty() || m_executedSet.contains(eventId)) return;
    m_executedSet.insert(eventId);
    m_executedOrder.append(eventId);
    while (m_executedOrder.size() > kMaxExecutedHistory) {
        const QString oldest = m_executedOrder.takeFirst();
        m_executedSet.remove(oldest);
    }
}

int ActionScheduler::cancelSuperseded(qint64 upToRevision) {
    int removed = 0;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it->revision <= upToRevision) {
            it = m_pending.erase(it);
            ++removed;
            ++m_supersededCount;
        } else {
            ++it;
        }
    }
    return removed;
}

ScheduledAction ActionScheduler::applyLatePolicy(const ScheduledAction &action, qint64 estimatedHostNowMs) {
    ScheduledAction adjusted = action;
    if (action.targetHostTimestampMs <= 0) {
        return adjusted;
    }

    const qint64 lateMs = estimatedHostNowMs - action.targetHostTimestampMs;
    if (lateMs <= 0) {
        return adjusted;
    }

    // For a playing timeline (Play, or Seek/StateSync while playing), project the
    // authoritative media position forward by lateMs * playbackRate so we align to
    // where the timeline actually is right now rather than a stale past position.
    if (action.playing || action.type == ScheduledActionType::Play) {
        const double rate = AuthoritativeTimeline::sanitizeRate(action.playbackRate);
        const qint64 catchUpDelta = std::llround(static_cast<double>(lateMs) * rate);
        adjusted.targetPositionMs = AuthoritativeTimeline::sanitizePositionMs(
            action.targetPositionMs + catchUpDelta);
    }
    return adjusted;
}

ScheduleStatus ActionScheduler::schedule(const ScheduledAction &action,
                                         qint64 estimatedHostNowMs,
                                         qint64 localNowMs,
                                         ScheduledAction &outReadyAction,
                                         const QString &hostPeerId) {
    // 1. Deduplicate by eventId
    if (!action.eventId.isEmpty() && isEventIdSeen(action.eventId)) {
        ++m_duplicateCount;
        return ScheduleStatus::DuplicateIgnored;
    }

    // 2. Reject stale or losing tie-break revisions
    if (action.revision > 0 && m_highestRevision > 0) {
        if (action.revision < m_highestRevision) {
            ++m_staleDroppedCount;
            return ScheduleStatus::StaleDropped;
        }
        if (action.revision == m_highestRevision) {
            if (action.authorId == m_highestAuthor) {
                ++m_duplicateCount;
                return ScheduleStatus::DuplicateIgnored;
            }
            bool incomingWins = false;
            if (!hostPeerId.isEmpty()) {
                if (action.authorId == hostPeerId && m_highestAuthor != hostPeerId) {
                    incomingWins = true;
                } else if (m_highestAuthor == hostPeerId && action.authorId != hostPeerId) {
                    incomingWins = false;
                } else {
                    incomingWins = (action.authorId > m_highestAuthor);
                }
            } else {
                incomingWins = (action.authorId > m_highestAuthor);
            }
            if (!incomingWins) {
                ++m_staleDroppedCount;
                return ScheduleStatus::StaleDropped;
            }
        }
    }

    // 3. Cancel any pending scheduled actions superseded by this revision
    if (action.revision > 0) {
        cancelSuperseded(action.revision);
        m_highestRevision = action.revision;
        m_highestAuthor = action.authorId;
    }

    // 4. Determine whether to execute now (immediate / late catch-up) or queue for the future
    const qint64 waitMs = (action.targetHostTimestampMs > 0)
                              ? (action.targetHostTimestampMs - estimatedHostNowMs)
                              : 0;

    if (waitMs <= 0) {
        recordExecutedEventId(action.eventId);
        outReadyAction = applyLatePolicy(action, estimatedHostNowMs);
        if (action.targetHostTimestampMs > 0 && waitMs < -kLateThresholdMs) {
            ++m_lateEventCount;
            return ScheduleStatus::ExecutedLateCatchUp;
        }
        return ScheduleStatus::ExecutedImmediately;
    }

    // 5. Queue for future execution, clamped to conservative maximum wait
    ScheduledAction queued = action;
    queued.enqueuedLocalMs = localNowMs;
    if (waitMs > kMaxFutureWaitMs) {
        queued.targetHostTimestampMs = estimatedHostNowMs + kMaxFutureWaitMs;
    }

    while (m_pending.size() >= kMaxPendingActions) {
        m_pending.removeFirst();
        ++m_supersededCount;
    }

    m_pending.append(queued);
    std::sort(m_pending.begin(), m_pending.end(), [](const ScheduledAction &a, const ScheduledAction &b) {
        return a.targetHostTimestampMs < b.targetHostTimestampMs;
    });

    return ScheduleStatus::Queued;
}

QList<ScheduledAction> ActionScheduler::popDueActions(qint64 estimatedHostNowMs) {
    QList<ScheduledAction> due;
    for (auto it = m_pending.begin(); it != m_pending.end();) {
        if (it->targetHostTimestampMs <= estimatedHostNowMs) {
            ScheduledAction ready = applyLatePolicy(*it, estimatedHostNowMs);
            recordExecutedEventId(ready.eventId);
            due.append(ready);
            it = m_pending.erase(it);
        } else {
            ++it;
        }
    }
    return due;
}

qint64 ActionScheduler::nextWakeupDelayMs(qint64 estimatedHostNowMs) const {
    if (m_pending.isEmpty()) {
        return -1;
    }
    const qint64 waitMs = m_pending.front().targetHostTimestampMs - estimatedHostNowMs;
    return std::clamp<qint64>(waitMs, 0, kMaxFutureWaitMs);
}

// ============================================================================
// AdaptiveDriftController
// ============================================================================

AdaptiveDriftController::AdaptiveDriftController()
    : m_config(Config{}) {}

AdaptiveDriftController::AdaptiveDriftController(const Config &config)
    : m_config(config) {}

void AdaptiveDriftController::reset() {
    m_hasDriftSample = false;
    m_lastSignedDriftMs = 0;
    m_smoothedDriftMs = 0.0;
    m_lastMode = DriftCorrectionMode::None;
    m_wasBuffering = false;
    m_pendingBufferRecovery = false;
    m_bufferRecoveredAtMs = 0;
    m_rateAdjustActive = false;
    m_rateAdjustStartedAtMs = 0;
    m_rateAdjustStartDriftMs = 0;
    m_lastRateChangeAtMs = 0;
    m_outsideDeadbandStreak = 0;
    m_rateFallbackTriggered = false;
    m_lastSeekAtMs = 0;
    m_lastSeekTargetMs = -1;
    m_lastPreSeekAbsDriftMs = 0;
    m_softCorrectionCount = 0;
    m_hardSeekCount = 0;
    m_bufferRecoveryCount = 0;
}

void AdaptiveDriftController::notifyExternalSeek(qint64 localNowMs, qint64 targetMs) {
    m_lastSeekAtMs = localNowMs;
    m_lastSeekTargetMs = targetMs;
    m_rateAdjustActive = false;
    m_outsideDeadbandStreak = 0;
    m_hasDriftSample = false;
}

void AdaptiveDriftController::notifyRateControlFailed() {
    m_rateFallbackTriggered = true;
    m_rateAdjustActive = false;
}

DriftCorrectionDecision AdaptiveDriftController::evaluate(const PlayerObservation &obs,
                                                          qint64 expectedPositionMs,
                                                          bool timelinePlaying,
                                                          double timelineRate,
                                                          qint64 clockUncertaintyMs,
                                                          qint64 localNowMs) {
    DriftCorrectionDecision decision;
    decision.expectedPositionMs = expectedPositionMs;
    decision.actualPositionMs = obs.actualPositionMs;
    decision.signedDriftMs = obs.actualPositionMs - expectedPositionMs;
    decision.targetRate = AuthoritativeTimeline::sanitizeRate(timelineRate);

    m_lastSignedDriftMs = decision.signedDriftMs;

    // 1. Suppress drift corrections while media is opening or buffering
    if (obs.isOpening || obs.isBuffering) {
        m_wasBuffering = true;
        m_rateAdjustActive = false;
        m_outsideDeadbandStreak = 0;
        decision.mode = DriftCorrectionMode::SuppressedBuffering;
        // If playback rate was previously altered, restore 1.0x so buffering isn't starved
        if (std::abs(obs.currentRate - decision.targetRate) > 0.001) {
            decision.mode = DriftCorrectionMode::RestoreRate;
        }
        m_lastMode = decision.mode;
        return decision;
    }

    // 2. Suppress drift corrections while a local/remote seek is settling
    const bool inPostSeekGrace = (m_lastSeekAtMs > 0 && (localNowMs - m_lastSeekAtMs) < m_config.postSeekSettleMs);
    if (obs.isSeekSettling || inPostSeekGrace) {
        decision.mode = DriftCorrectionMode::SuppressedSeeking;
        m_lastMode = decision.mode;
        return decision;
    }

    // 3. Detect transition out of buffering and enforce a brief decoder-settle grace window
    if (m_wasBuffering) {
        m_wasBuffering = false;
        m_pendingBufferRecovery = true;
        m_bufferRecoveredAtMs = localNowMs;
        m_hasDriftSample = false;
    }

    if (m_pendingBufferRecovery) {
        if ((localNowMs - m_bufferRecoveredAtMs) < m_config.postBufferSettleMs) {
            decision.mode = DriftCorrectionMode::SuppressedBuffering;
            m_lastMode = decision.mode;
            return decision;
        }

        // Perform a single deliberate post-buffer recovery alignment
        m_pendingBufferRecovery = false;
        decision.origin = ActionOrigin::BufferRecovery;

        if (!timelinePlaying) {
            if (!obs.isPaused) {
                decision.shouldPause = true;
            }
            if (std::llabs(decision.signedDriftMs) > m_config.pausedSeekThresholdMs && obs.isSeekable) {
                decision.mode = DriftCorrectionMode::BufferRecoverySeek;
                decision.targetSeekMs = expectedPositionMs;
                m_lastSeekAtMs = localNowMs;
                m_lastSeekTargetMs = expectedPositionMs;
                m_lastPreSeekAbsDriftMs = std::llabs(decision.signedDriftMs);
                ++m_bufferRecoveryCount;
                ++m_hardSeekCount;
                m_lastMode = decision.mode;
                return decision;
            }
        } else {
            if (obs.isPaused) {
                decision.shouldResume = true;
            }
            if (std::llabs(decision.signedDriftMs) > m_config.hardSeekThresholdMs && obs.isSeekable) {
                decision.mode = DriftCorrectionMode::BufferRecoverySeek;
                decision.targetSeekMs = expectedPositionMs;
                m_lastSeekAtMs = localNowMs;
                m_lastSeekTargetMs = expectedPositionMs;
                m_lastPreSeekAbsDriftMs = std::llabs(decision.signedDriftMs);
                ++m_bufferRecoveryCount;
                ++m_hardSeekCount;
                m_lastMode = decision.mode;
                return decision;
            }
            // If post-buffer drift is moderate (deadband < |drift| <= hardSeekThresholdMs),
            // fall through to Tier 2 rate adjustment so we don't trigger a second re-buffer!
            if (std::llabs(decision.signedDriftMs) > m_config.deadbandMs) {
                ++m_bufferRecoveryCount;
            }
        }
    }

    // 4. Update smoothed drift EMA
    if (!m_hasDriftSample) {
        m_smoothedDriftMs = static_cast<double>(decision.signedDriftMs);
        m_hasDriftSample = true;
    } else {
        m_smoothedDriftMs = 0.65 * static_cast<double>(decision.signedDriftMs) + 0.35 * m_smoothedDriftMs;
    }
    decision.smoothedDriftMs = std::llround(m_smoothedDriftMs);

    // 5. Paused timeline handling
    if (!timelinePlaying) {
        m_rateAdjustActive = false;
        m_outsideDeadbandStreak = 0;

        if (!obs.isPaused && !obs.isEndedOrStopped) {
            decision.shouldPause = true;
        }
        const bool seekCooldownOk = (m_lastSeekAtMs == 0 || (localNowMs - m_lastSeekAtMs) >= m_config.hardSeekCooldownMs);
        if (std::llabs(decision.signedDriftMs) > m_config.pausedSeekThresholdMs && obs.isSeekable && seekCooldownOk) {
            decision.mode = DriftCorrectionMode::HardSeek;
            decision.targetSeekMs = expectedPositionMs;
            m_lastSeekAtMs = localNowMs;
            m_lastSeekTargetMs = expectedPositionMs;
            m_lastPreSeekAbsDriftMs = std::llabs(decision.signedDriftMs);
            ++m_hardSeekCount;
            m_lastMode = decision.mode;
            return decision;
        }
        if (std::abs(obs.currentRate - decision.targetRate) > 0.001) {
            decision.mode = DriftCorrectionMode::RestoreRate;
        } else {
            decision.mode = DriftCorrectionMode::None;
        }
        m_lastMode = decision.mode;
        return decision;
    }

    // 6. Playing timeline: ensure player is running
    if (obs.isEndedOrStopped) {
        decision.shouldRestart = true;
        decision.targetSeekMs = expectedPositionMs;
        decision.mode = DriftCorrectionMode::HardSeek;
        m_lastSeekAtMs = localNowMs;
        ++m_hardSeekCount;
        m_lastMode = decision.mode;
        return decision;
    }
    if (obs.isPaused) {
        decision.shouldResume = true;
    }

    const qint64 absDrift = std::llabs(decision.signedDriftMs);
    const qint64 effectiveDeadbandMs = m_config.deadbandMs + std::clamp<qint64>(clockUncertaintyMs / 4, 0, 40);
    const bool seekCooldownOk = (m_lastSeekAtMs == 0 || (localNowMs - m_lastSeekAtMs) >= m_config.hardSeekCooldownMs);
    const bool canUseRateControl = obs.rateControlSupported && !m_rateFallbackTriggered;

    // 7. Tier 3: Large drift -> Controlled Hard Seek
    if (absDrift > m_config.hardSeekThresholdMs) {
        if (obs.isSeekable && seekCooldownOk) {
            m_rateAdjustActive = false;
            m_outsideDeadbandStreak = 0;
            decision.mode = DriftCorrectionMode::HardSeek;
            decision.targetSeekMs = expectedPositionMs;
            decision.targetRate = AuthoritativeTimeline::sanitizeRate(timelineRate);
            m_lastSeekAtMs = localNowMs;
            m_lastSeekTargetMs = expectedPositionMs;
            m_lastPreSeekAbsDriftMs = absDrift;
            ++m_hardSeekCount;
            m_lastMode = decision.mode;
            return decision;
        }
        // If on hard-seek cooldown (e.g. seek just snapped to a nearby keyframe),
        // allow Tier 2 rate control below to smoothly close the residual offset.
    }

    // Fallback seek when rate control is unsupported or failed convergence watchdog
    if (!canUseRateControl && absDrift > m_config.fallbackSeekThresholdMs) {
        if (obs.isSeekable && seekCooldownOk) {
            m_rateAdjustActive = false;
            decision.mode = DriftCorrectionMode::HardSeek;
            decision.targetSeekMs = expectedPositionMs;
            m_lastSeekAtMs = localNowMs;
            m_lastSeekTargetMs = expectedPositionMs;
            m_lastPreSeekAbsDriftMs = absDrift;
            ++m_hardSeekCount;
            m_lastMode = decision.mode;
            return decision;
        }
    }

    // 8. Tier 1: Deadband & Hysteresis Recovery
    const bool crossedZero = m_rateAdjustActive &&
                             ((decision.signedDriftMs >= 0 && m_rateAdjustStartDriftMs < 0) ||
                              (decision.signedDriftMs <= 0 && m_rateAdjustStartDriftMs > 0));

    if (m_rateAdjustActive && (absDrift <= m_config.recoveryBandMs || crossedZero)) {
        m_rateAdjustActive = false;
        m_outsideDeadbandStreak = 0;
        decision.mode = DriftCorrectionMode::RestoreRate;
        decision.targetRate = AuthoritativeTimeline::sanitizeRate(timelineRate);
        m_lastRateChangeAtMs = localNowMs;
        m_lastMode = decision.mode;
        return decision;
    }

    if (!m_rateAdjustActive && absDrift <= effectiveDeadbandMs) {
        m_outsideDeadbandStreak = 0;
        if (std::abs(obs.currentRate - decision.targetRate) > 0.001) {
            decision.mode = DriftCorrectionMode::RestoreRate;
        } else {
            decision.mode = DriftCorrectionMode::None;
        }
        m_lastMode = decision.mode;
        return decision;
    }

    // 9. Tier 2: Moderate drift -> Gradual Proportional Playback-Rate Adjustment
    if (!canUseRateControl) {
        if (std::abs(obs.currentRate - decision.targetRate) > 0.001) {
            decision.mode = DriftCorrectionMode::RestoreRate;
        } else {
            decision.mode = DriftCorrectionMode::None;
        }
        m_lastMode = decision.mode;
        return decision;
    }

    if (!m_rateAdjustActive) {
        ++m_outsideDeadbandStreak;
        // Require either 2 consecutive ticks outside deadband or a clear moderate drift (> 120 ms)
        if (m_outsideDeadbandStreak < 2 && absDrift < 120) {
            decision.mode = DriftCorrectionMode::None;
            m_lastMode = decision.mode;
            return decision;
        }
        m_rateAdjustActive = true;
        m_rateAdjustStartedAtMs = localNowMs;
        m_rateAdjustStartDriftMs = decision.signedDriftMs;
    } else {
        // Watchdog: if rate adjustment has been active for rateFallbackWindowMs without
        // reducing drift by at least 25 ms, mark rate control ineffective and fall back.
        if ((localNowMs - m_rateAdjustStartedAtMs) >= m_config.rateFallbackWindowMs) {
            if (absDrift >= std::llabs(m_rateAdjustStartDriftMs) - 25) {
                m_rateFallbackTriggered = true;
                m_rateAdjustActive = false;
                if (absDrift >= m_config.fallbackSeekThresholdMs && obs.isSeekable && seekCooldownOk) {
                    decision.mode = DriftCorrectionMode::HardSeek;
                    decision.targetSeekMs = expectedPositionMs;
                    m_lastSeekAtMs = localNowMs;
                    ++m_hardSeekCount;
                    m_lastMode = decision.mode;
                    return decision;
                }
                decision.mode = DriftCorrectionMode::RestoreRate;
                m_lastMode = decision.mode;
                return decision;
            }
            // Progress is being made; slide the watchdog window forward
            m_rateAdjustStartedAtMs = localNowMs;
            m_rateAdjustStartDriftMs = decision.signedDriftMs;
        }
    }

    // Proportional rate law:
    //   signedDriftMs > 0 (local is ahead)  -> deltaRate < 0 (slow down, e.g. 0.97x)
    //   signedDriftMs < 0 (local is behind) -> deltaRate > 0 (speed up, e.g. 1.03x)
    const double baseRate = AuthoritativeTimeline::sanitizeRate(timelineRate);
    const double rawDelta = -static_cast<double>(decision.signedDriftMs) / m_config.rateTimeConstantMs;
    const double computedRate = std::clamp(baseRate + rawDelta, m_config.minRate, m_config.maxRate);

    const bool rateCooldownOk = (m_lastRateChangeAtMs == 0 ||
                                 (localNowMs - m_lastRateChangeAtMs) >= m_config.rateAdjustCooldownMs);

    if (std::abs(computedRate - obs.currentRate) >= m_config.minRateStep && rateCooldownOk) {
        decision.mode = DriftCorrectionMode::RateAdjust;
        decision.targetRate = computedRate;
        m_lastRateChangeAtMs = localNowMs;
        ++m_softCorrectionCount;
    } else {
        // Keep holding current adjusted rate
        decision.mode = DriftCorrectionMode::None;
        decision.targetRate = obs.currentRate;
    }

    m_lastMode = decision.mode;
    return decision;
}

// ============================================================================
// SyncDiagnostics
// ============================================================================

QVariantMap SyncDiagnostics::toVariantMap() const {
    QVariantMap m;
    m.insert(QStringLiteral("smoothedRttMs"), smoothedRttMs);
    m.insert(QStringLiteral("minRttMs"), minRttMs);
    m.insert(QStringLiteral("worstRttMs"), worstRttMs);
    m.insert(QStringLiteral("jitterMs"), jitterMs);
    m.insert(QStringLiteral("clockOffsetMs"), clockOffsetMs);
    m.insert(QStringLiteral("clockUncertaintyMs"), clockUncertaintyMs);
    m.insert(QStringLiteral("clockConfidence"), clockConfidence);
    m.insert(QStringLiteral("clockLocked"), clockLocked);
    m.insert(QStringLiteral("clockSampleCount"), clockSampleCount);
    m.insert(QStringLiteral("rejectedSampleCount"), rejectedSampleCount);
    m.insert(QStringLiteral("scheduledLeadTimeMs"), scheduledLeadTimeMs);
    m.insert(QStringLiteral("pendingScheduledCount"), pendingScheduledCount);
    m.insert(QStringLiteral("currentRevision"), currentRevision);
    m.insert(QStringLiteral("authorityPeerId"), authorityPeerId);
    m.insert(QStringLiteral("localReadiness"), localReadiness);
    m.insert(QStringLiteral("authoritativePositionMs"), authoritativePositionMs);
    m.insert(QStringLiteral("actualPositionMs"), actualPositionMs);
    m.insert(QStringLiteral("signedDriftMs"), signedDriftMs);
    m.insert(QStringLiteral("smoothedDriftMs"), smoothedDriftMs);
    m.insert(QStringLiteral("currentPlaybackRate"), currentPlaybackRate);
    m.insert(QStringLiteral("activeCorrectionMode"), activeCorrectionMode);
    m.insert(QStringLiteral("softCorrectionCount"), softCorrectionCount);
    m.insert(QStringLiteral("hardSeekCorrectionCount"), hardSeekCorrectionCount);
    m.insert(QStringLiteral("bufferRecoveryCount"), bufferRecoveryCount);
    m.insert(QStringLiteral("lateEventCount"), lateEventCount);
    m.insert(QStringLiteral("supersededActionCount"), supersededActionCount);
    m.insert(QStringLiteral("duplicateEventCount"), duplicateEventCount);
    m.insert(QStringLiteral("staleRevisionDropCount"), staleRevisionDropCount);
    return m;
}

// ============================================================================
// 8. Local Video Teleparty Verification & Media Matching
// ============================================================================

QString telepartyMediaModeToString(TelepartyMediaMode mode) {
    switch (mode) {
    case TelepartyMediaMode::OnlineCatalog: return QStringLiteral("Online");
    case TelepartyMediaMode::LocalFile:     return QStringLiteral("Local");
    }
    return QStringLiteral("Online");
}

QString localVerificationStateToCode(LocalVerificationState state) {
    switch (state) {
    case LocalVerificationState::None:                     return QStringLiteral("n");
    case LocalVerificationState::WaitingForLocalSelection: return QStringLiteral("w");
    case LocalVerificationState::InspectingLocalMedia:     return QStringLiteral("i");
    case LocalVerificationState::VerifiedCompatible:       return QStringLiteral("v");
    case LocalVerificationState::IncompatibleMedia:        return QStringLiteral("x");
    case LocalVerificationState::UnresolvedMetadata:       return QStringLiteral("u");
    case LocalVerificationState::OpeningMedia:             return QStringLiteral("o");
    case LocalVerificationState::BufferingMedia:           return QStringLiteral("b");
    case LocalVerificationState::ReadyToSync:              return QStringLiteral("r");
    case LocalVerificationState::PlaybackError:            return QStringLiteral("e");
    }
    return QStringLiteral("n");
}

LocalVerificationState localVerificationStateFromCode(const QString &code) {
    if (code == QLatin1String("w")) return LocalVerificationState::WaitingForLocalSelection;
    if (code == QLatin1String("i")) return LocalVerificationState::InspectingLocalMedia;
    if (code == QLatin1String("v")) return LocalVerificationState::VerifiedCompatible;
    if (code == QLatin1String("x")) return LocalVerificationState::IncompatibleMedia;
    if (code == QLatin1String("u")) return LocalVerificationState::UnresolvedMetadata;
    if (code == QLatin1String("o")) return LocalVerificationState::OpeningMedia;
    if (code == QLatin1String("b")) return LocalVerificationState::BufferingMedia;
    if (code == QLatin1String("r")) return LocalVerificationState::ReadyToSync;
    if (code == QLatin1String("e")) return LocalVerificationState::PlaybackError;
    return LocalVerificationState::None;
}

QString localVerificationStateToString(LocalVerificationState state) {
    switch (state) {
    case LocalVerificationState::None:                     return QStringLiteral("None");
    case LocalVerificationState::WaitingForLocalSelection: return QStringLiteral("WaitingForLocalSelection");
    case LocalVerificationState::InspectingLocalMedia:     return QStringLiteral("InspectingLocalMedia");
    case LocalVerificationState::VerifiedCompatible:       return QStringLiteral("VerifiedCompatible");
    case LocalVerificationState::IncompatibleMedia:        return QStringLiteral("IncompatibleMedia");
    case LocalVerificationState::UnresolvedMetadata:       return QStringLiteral("UnresolvedMetadata");
    case LocalVerificationState::OpeningMedia:             return QStringLiteral("OpeningMedia");
    case LocalVerificationState::BufferingMedia:           return QStringLiteral("BufferingMedia");
    case LocalVerificationState::ReadyToSync:              return QStringLiteral("ReadyToSync");
    case LocalVerificationState::PlaybackError:            return QStringLiteral("PlaybackError");
    }
    return QStringLiteral("None");
}

QString localCompatibilityReasonToCode(LocalCompatibilityReason reason) {
    switch (reason) {
    case LocalCompatibilityReason::None:                     return QStringLiteral("none");
    case LocalCompatibilityReason::MatchedExact:             return QStringLiteral("ok_exact");
    case LocalCompatibilityReason::MatchedRemuxOrReencode:   return QStringLiteral("ok_remux");
    case LocalCompatibilityReason::MissingFile:              return QStringLiteral("err_missing");
    case LocalCompatibilityReason::UnreadableOrEmptyFile:    return QStringLiteral("err_empty");
    case LocalCompatibilityReason::UnsupportedContainer:     return QStringLiteral("err_container");
    case LocalCompatibilityReason::UnplayableMedia:          return QStringLiteral("err_unplayable");
    case LocalCompatibilityReason::InsufficientMetadata:     return QStringLiteral("err_meta");
    case LocalCompatibilityReason::TitleMismatch:            return QStringLiteral("err_title");
    case LocalCompatibilityReason::EpisodeMismatch:          return QStringLiteral("err_episode");
    case LocalCompatibilityReason::EditionMismatch:          return QStringLiteral("err_edition");
    case LocalCompatibilityReason::DurationMismatch:         return QStringLiteral("err_dur");
    case LocalCompatibilityReason::FingerprintMismatch:      return QStringLiteral("err_fp");
    case LocalCompatibilityReason::StrictHashMismatch:       return QStringLiteral("err_hash");
    case LocalCompatibilityReason::InvalidSessionDescriptor: return QStringLiteral("err_desc");
    }
    return QStringLiteral("none");
}

LocalCompatibilityReason localCompatibilityReasonFromCode(const QString &code) {
    if (code == QLatin1String("ok_exact"))      return LocalCompatibilityReason::MatchedExact;
    if (code == QLatin1String("ok_remux"))      return LocalCompatibilityReason::MatchedRemuxOrReencode;
    if (code == QLatin1String("err_missing"))   return LocalCompatibilityReason::MissingFile;
    if (code == QLatin1String("err_empty"))     return LocalCompatibilityReason::UnreadableOrEmptyFile;
    if (code == QLatin1String("err_container")) return LocalCompatibilityReason::UnsupportedContainer;
    if (code == QLatin1String("err_unplayable"))return LocalCompatibilityReason::UnplayableMedia;
    if (code == QLatin1String("err_meta"))      return LocalCompatibilityReason::InsufficientMetadata;
    if (code == QLatin1String("err_title"))     return LocalCompatibilityReason::TitleMismatch;
    if (code == QLatin1String("err_episode"))   return LocalCompatibilityReason::EpisodeMismatch;
    if (code == QLatin1String("err_edition"))   return LocalCompatibilityReason::EditionMismatch;
    if (code == QLatin1String("err_dur"))       return LocalCompatibilityReason::DurationMismatch;
    if (code == QLatin1String("err_fp"))        return LocalCompatibilityReason::FingerprintMismatch;
    if (code == QLatin1String("err_hash"))      return LocalCompatibilityReason::StrictHashMismatch;
    if (code == QLatin1String("err_desc"))      return LocalCompatibilityReason::InvalidSessionDescriptor;
    return LocalCompatibilityReason::None;
}

QString localCompatibilityReasonToMessage(LocalCompatibilityReason reason) {
    switch (reason) {
    case LocalCompatibilityReason::None:
        return QString();
    case LocalCompatibilityReason::MatchedExact:
        return QStringLiteral("Verified compatible (exact local media match).");
    case LocalCompatibilityReason::MatchedRemuxOrReencode:
        return QStringLiteral("Verified compatible (matching movie timeline & essence).");
    case LocalCompatibilityReason::MissingFile:
        return QStringLiteral("Selected local video file could not be found on disk.");
    case LocalCompatibilityReason::UnreadableOrEmptyFile:
        return QStringLiteral("Selected local video file is empty or unreadable.");
    case LocalCompatibilityReason::UnsupportedContainer:
        return QStringLiteral("Unsupported video container format.");
    case LocalCompatibilityReason::UnplayableMedia:
        return QStringLiteral("Selected local file could not be decoded or played by libVLC.");
    case LocalCompatibilityReason::InsufficientMetadata:
        return QStringLiteral("Could not identify this media from its name.");
    case LocalCompatibilityReason::TitleMismatch:
        return QStringLiteral("Incompatible video: title does not match the host's selected movie.");
    case LocalCompatibilityReason::EpisodeMismatch:
        return QStringLiteral("Incompatible video: season/episode number does not match the host's selection.");
    case LocalCompatibilityReason::EditionMismatch:
        return QStringLiteral("Incompatible video: edition/cut (e.g. Extended vs Theatrical) does not match.");
    case LocalCompatibilityReason::DurationMismatch:
        return QStringLiteral("Incompatible video: runtime differs beyond the allowed synchronization tolerance.");
    case LocalCompatibilityReason::FingerprintMismatch:
        return QStringLiteral("Incompatible video: media content fingerprint does not match the host's video.");
    case LocalCompatibilityReason::StrictHashMismatch:
        return QStringLiteral("Incompatible video: full-file SHA-256 hash does not match the host's video.");
    case LocalCompatibilityReason::InvalidSessionDescriptor:
        return QStringLiteral("Invalid or incomplete local media descriptor from session host.");
    }
    return QString();
}

// ============================================================================
// LocalMediaDescriptor
// ============================================================================

bool LocalMediaDescriptor::isValidForSession() const {
    if (mediaId.isEmpty() || !mediaId.startsWith(QLatin1String("LM_"))) {
        return false;
    }
    if (normalizedTitle.isEmpty() || displayTitle.isEmpty()) {
        return false;
    }
    if (durationMs < 0 || durationMs > AuthoritativeTimeline::kMaxMediaPositionMs) return false;
    if (fileSizeBytes <= 0) {
        return false;
    }
    if (containerFormat.isEmpty() || !LocalMediaInspector::isSupportedContainer(containerFormat)) {
        return false;
    }
    if (containsPrivatePathLeak(displayTitle) ||
        containsPrivatePathLeak(normalizedTitle) ||
        containsPrivatePathLeak(mediaId)) {
        return false;
    }
    return true;
}

bool LocalMediaDescriptor::containsPrivatePathLeak(const QString &text) {
    if (text.isEmpty()) {
        return false;
    }
    const QString lower = text.toLower();
    if (lower.contains(QLatin1String("file://")) ||
        lower.contains(QLatin1String("\\\\")) ||
        lower.contains(QLatin1String("\\users\\")) ||
        lower.contains(QLatin1String("/users/")) ||
        lower.contains(QLatin1String("/home/")) ||
        lower.contains(QLatin1String("c:/")) ||
        lower.contains(QLatin1String("d:/")) ||
        lower.contains(QLatin1String("e:/")) ||
        lower.contains(QLatin1String("f:/"))) {
        return true;
    }
    if (text.contains(QLatin1Char('\\'))) {
        return true;
    }
    // Windows drive prefix pattern such as "C:" or "D:"
    static const QRegularExpression kDrivePrefixRx(QStringLiteral(R"((?:^|[\s"'])([A-Za-z]:)(?:[/\\]|$))"));
    if (kDrivePrefixRx.match(text).hasMatch()) {
        return true;
    }
    return false;
}

bool LocalMediaDescriptor::containsPrivatePathLeakInJson(const QJsonObject &obj) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (it.value().isString() && containsPrivatePathLeak(it.value().toString())) {
            return true;
        }
        if (it.value().isObject() && containsPrivatePathLeakInJson(it.value().toObject())) {
            return true;
        }
    }
    return false;
}

QString LocalMediaDescriptor::generateSessionMediaId(const QString &sessionId,
                                                     qint64 revision,
                                                     const LocalMediaDescriptor &desc) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    const QString material = QStringLiteral("%1:%2:%3:%4:%5:%6")
                                 .arg(sessionId,
                                      QString::number(revision),
                                      desc.normalizedTitle,
                                      desc.episodeTag,
                                      QString::number(desc.durationMs),
                                      desc.contentFingerprint);
    hash.addData(material.toUtf8());
    return QStringLiteral("LM_") + QString::fromLatin1(hash.result().toHex().left(16));
}

QJsonObject LocalMediaDescriptor::toNetworkJson() const {
    QJsonObject obj;
    obj[QStringLiteral("sid")] = sessionId.left(24);
    obj[QStringLiteral("mid")] = mediaId.left(32);
    obj[QStringLiteral("rev")] = revision;
    obj[QStringLiteral("t")]   = displayTitle.left(90);
    obj[QStringLiteral("nt")]  = normalizedTitle.left(90);
    if (!episodeTag.isEmpty()) {
        obj[QStringLiteral("ep")] = episodeTag.left(16);
    }
    if (!yearTag.isEmpty()) {
        obj[QStringLiteral("yr")] = yearTag.left(8);
    }
    if (!editionTag.isEmpty()) {
        obj[QStringLiteral("ed")] = editionTag.left(24);
    }
    obj[QStringLiteral("dur")] = durationMs;
    obj[QStringLiteral("sz")]  = fileSizeBytes;
    obj[QStringLiteral("fmt")] = containerFormat.left(12);
    if (!videoCodec.isEmpty()) {
        obj[QStringLiteral("vc")] = videoCodec.left(16);
    }
    if (!audioCodec.isEmpty()) {
        obj[QStringLiteral("ac")] = audioCodec.left(16);
    }
    if (width > 0 && height > 0) {
        obj[QStringLiteral("w")] = width;
        obj[QStringLiteral("h")] = height;
    }
    if (frameRate > 0.0) {
        obj[QStringLiteral("fps")] = std::round(frameRate * 100.0) / 100.0;
    }
    if (streamStartOffsetMs != 0) {
        obj[QStringLiteral("so")] = streamStartOffsetMs;
    }
    obj[QStringLiteral("fp")] = contentFingerprint.left(80);
    if (!fullFileSha256.isEmpty()) {
        obj[QStringLiteral("sha")] = fullFileSha256.left(64);
    }
    return obj;
}

LocalMediaDescriptor LocalMediaDescriptor::fromNetworkJson(const QJsonObject &obj) {
    LocalMediaDescriptor d;
    if (containsPrivatePathLeakInJson(obj)) {
        return d;
    }
    d.sessionId          = obj.value(QStringLiteral("sid")).toString().trimmed().left(24);
    d.mediaId            = obj.value(QStringLiteral("mid")).toString().trimmed().left(32);
    d.revision           = obj.value(QStringLiteral("rev")).toVariant().toLongLong();
    d.displayTitle       = obj.value(QStringLiteral("t")).toString().trimmed().left(90);
    d.normalizedTitle    = obj.value(QStringLiteral("nt")).toString().trimmed().left(90);
    d.episodeTag         = obj.value(QStringLiteral("ep")).toString().trimmed().left(16);
    d.yearTag            = obj.value(QStringLiteral("yr")).toString().trimmed().left(8);
    d.editionTag         = obj.value(QStringLiteral("ed")).toString().trimmed().left(24);
    d.durationMs         = obj.value(QStringLiteral("dur")).toVariant().toLongLong();
    d.fileSizeBytes      = obj.value(QStringLiteral("sz")).toVariant().toLongLong();
    d.containerFormat    = obj.value(QStringLiteral("fmt")).toString().trimmed().toUpper().left(12);
    d.videoCodec         = obj.value(QStringLiteral("vc")).toString().trimmed().toUpper().left(16);
    d.audioCodec         = obj.value(QStringLiteral("ac")).toString().trimmed().toUpper().left(16);
    d.width              = obj.value(QStringLiteral("w")).toInt(0);
    d.height             = obj.value(QStringLiteral("h")).toInt(0);
    d.frameRate          = obj.value(QStringLiteral("fps")).toDouble(0.0);
    d.streamStartOffsetMs= obj.value(QStringLiteral("so")).toVariant().toLongLong();
    d.contentFingerprint = obj.value(QStringLiteral("fp")).toString().trimmed().left(80);
    d.fullFileSha256     = obj.value(QStringLiteral("sha")).toString().trimmed().toLower().left(64);
    return d;
}

QVariantMap LocalMediaDescriptor::toVariantMap() const {
    QVariantMap m;
    m.insert(QStringLiteral("sessionId"), sessionId);
    m.insert(QStringLiteral("mediaId"), mediaId);
    m.insert(QStringLiteral("revision"), revision);
    m.insert(QStringLiteral("displayTitle"), displayTitle);
    m.insert(QStringLiteral("normalizedTitle"), normalizedTitle);
    m.insert(QStringLiteral("episodeTag"), episodeTag);
    m.insert(QStringLiteral("yearTag"), yearTag);
    m.insert(QStringLiteral("editionTag"), editionTag);
    m.insert(QStringLiteral("durationMs"), durationMs);
    m.insert(QStringLiteral("fileSizeBytes"), fileSizeBytes);
    m.insert(QStringLiteral("containerFormat"), containerFormat);
    m.insert(QStringLiteral("videoCodec"), videoCodec);
    m.insert(QStringLiteral("audioCodec"), audioCodec);
    m.insert(QStringLiteral("width"), width);
    m.insert(QStringLiteral("height"), height);
    m.insert(QStringLiteral("frameRate"), frameRate);
    m.insert(QStringLiteral("contentFingerprint"), contentFingerprint);
    return m;
}

// ============================================================================
// LocalMediaInspector — Container Parsing, Normalization, Fingerprinting, Cache
// ============================================================================

namespace {

struct CachedMediaEntry {
    qint64 fileSizeBytes = 0;
    qint64 lastModifiedMs = 0;
    bool hasFullHash = false;
    LocalMediaDescriptor descriptor;
};

QMutex g_inspectorCacheMutex;
QHash<QString, CachedMediaEntry> g_inspectorCache;

struct ContainerHeaderInfo {
    qint64 durationMs = 0;
    QString videoCodec;
    QString audioCodec;
    int width = 0;
    int height = 0;
    double frameRate = 0.0;
    qint64 essenceOffset = 0;
    qint64 essenceSize = 0;
};

QString mapIsoCodecFourCC(const QByteArray &fourcc) {
    if (fourcc == "avc1" || fourcc == "avc3") return QStringLiteral("H264");
    if (fourcc == "hvc1" || fourcc == "hev1") return QStringLiteral("HEVC");
    if (fourcc == "av01") return QStringLiteral("AV1");
    if (fourcc == "vp09" || fourcc == "vp08") return QStringLiteral("VP9");
    if (fourcc == "mp4v") return QStringLiteral("MPEG4");
    if (fourcc == "mp4a") return QStringLiteral("AAC");
    if (fourcc == "ac-3") return QStringLiteral("AC3");
    if (fourcc == "ec-3") return QStringLiteral("EAC3");
    if (fourcc == "Opus" || fourcc == "opus") return QStringLiteral("OPUS");
    if (fourcc == "fLaC") return QStringLiteral("FLAC");
    if (fourcc == "alac") return QStringLiteral("ALAC");
    return QString();
}

void parseMp4MoovAtoms(const uchar *data, qint64 len, ContainerHeaderInfo &info) {
    qint64 pos = 0;
    while (pos + 8 <= len) {
        quint64 boxSize = qFromBigEndian<quint32>(data + pos);
        const QByteArray boxType(reinterpret_cast<const char *>(data + pos + 4), 4);
        qint64 headerLen = 8;
        if (boxSize == 1) {
            if (pos + 16 > len) break;
            boxSize = qFromBigEndian<quint64>(data + pos + 8);
            headerLen = 16;
        } else if (boxSize == 0) {
            boxSize = static_cast<quint64>(len - pos);
        }
        if (boxSize < static_cast<quint64>(headerLen) || pos + static_cast<qint64>(boxSize) > len) {
            break;
        }

        const uchar *payload = data + pos + headerLen;
        const qint64 payloadLen = static_cast<qint64>(boxSize) - headerLen;

        if (boxType == "mvhd" && payloadLen >= 20) {
            const uchar version = payload[0];
            if (version == 0 && payloadLen >= 20) {
                const quint32 timescale = qFromBigEndian<quint32>(payload + 12);
                const quint32 duration  = qFromBigEndian<quint32>(payload + 16);
                if (timescale > 0 && duration > 0 && duration != 0xFFFFFFFFU) {
                    info.durationMs = static_cast<qint64>((static_cast<quint64>(duration) * 1000ULL) / timescale);
                }
            } else if (version == 1 && payloadLen >= 32) {
                const quint32 timescale = qFromBigEndian<quint32>(payload + 20);
                const quint64 duration  = qFromBigEndian<quint64>(payload + 24);
                if (timescale > 0 && duration > 0 && duration != 0xFFFFFFFFFFFFFFFFULL) {
                    info.durationMs = static_cast<qint64>((duration * 1000ULL) / timescale);
                }
            }
        } else if (boxType == "tkhd" && payloadLen >= 84) {
            const uchar version = payload[0];
            const qint64 whOffset = (version == 1) ? 88 : 76;
            if (payloadLen >= whOffset + 8) {
                const quint32 wFixed = qFromBigEndian<quint32>(payload + whOffset);
                const quint32 hFixed = qFromBigEndian<quint32>(payload + whOffset + 4);
                const int w = static_cast<int>(wFixed >> 16);
                const int h = static_cast<int>(hFixed >> 16);
                if (w > info.width && h > info.height) {
                    info.width = w;
                    info.height = h;
                }
            }
        } else if (boxType == "stsd" && payloadLen >= 16) {
            const quint32 entryCount = qFromBigEndian<quint32>(payload + 4);
            if (entryCount >= 1 && payloadLen >= 16) {
                const QByteArray sampleFourCC(reinterpret_cast<const char *>(payload + 12), 4);
                const QString mapped = mapIsoCodecFourCC(sampleFourCC);
                if (!mapped.isEmpty()) {
                    if (mapped == QLatin1String("H264") || mapped == QLatin1String("HEVC") ||
                        mapped == QLatin1String("AV1")  || mapped == QLatin1String("VP9")  ||
                        mapped == QLatin1String("MPEG4")) {
                        if (info.videoCodec.isEmpty()) info.videoCodec = mapped;
                    } else {
                        if (info.audioCodec.isEmpty()) info.audioCodec = mapped;
                    }
                }
            }
        } else if (boxType == "trak" || boxType == "mdia" || boxType == "minf" ||
                   boxType == "stbl" || boxType == "edts") {
            parseMp4MoovAtoms(payload, payloadLen, info);
        }

        pos += static_cast<qint64>(boxSize);
    }
}

void parseMp4FileContainer(QFile &file, qint64 fileSize, ContainerHeaderInfo &info) {
    qint64 pos = 0;
    uchar header[16];
    int boxGuard = 0;
    while (pos + 8 <= fileSize && boxGuard < 256) {
        ++boxGuard;
        if (!file.seek(pos)) break;
        const qint64 nRead = file.read(reinterpret_cast<char *>(header), 16);
        if (nRead < 8) break;

        quint64 boxSize = qFromBigEndian<quint32>(header);
        const QByteArray boxType(reinterpret_cast<const char *>(header + 4), 4);
        qint64 headerLen = 8;
        if (boxSize == 1) {
            if (nRead < 16) break;
            boxSize = qFromBigEndian<quint64>(header + 8);
            headerLen = 16;
        } else if (boxSize == 0) {
            boxSize = static_cast<quint64>(fileSize - pos);
        }
        if (boxSize < static_cast<quint64>(headerLen) || pos + static_cast<qint64>(boxSize) > fileSize) {
            break;
        }

        const qint64 payloadOffset = pos + headerLen;
        const qint64 payloadSize = static_cast<qint64>(boxSize) - headerLen;

        if (boxType == "mdat") {
            if (payloadSize > info.essenceSize) {
                info.essenceOffset = payloadOffset;
                info.essenceSize = payloadSize;
            }
        } else if (boxType == "moov" && payloadSize > 0 && payloadSize <= 16 * 1024 * 1024) {
            if (file.seek(payloadOffset)) {
                const QByteArray moovBuf = file.read(payloadSize);
                if (moovBuf.size() == payloadSize) {
                    parseMp4MoovAtoms(reinterpret_cast<const uchar *>(moovBuf.constData()),
                                      moovBuf.size(),
                                      info);
                }
            }
        }

        pos += static_cast<qint64>(boxSize);
    }
}

// Reads an EBML variable-length integer (VINT) value, stripping the length descriptor bit
// when stripMarker is true (used for element data sizes).
bool readEbmlVint(const uchar *buf, qint64 len, qint64 pos, bool stripMarker, quint64 &outVal, int &outWidth) {
    if (pos >= len) return false;
    const uchar first = buf[pos];
    if (first == 0) return false;

    int width = 1;
    uchar mask = 0x80;
    while ((first & mask) == 0 && width <= 8) {
        mask >>= 1;
        ++width;
    }
    if (width > 8 || pos + width > len) return false;

    quint64 val = stripMarker ? static_cast<quint64>(first & (mask - 1)) : static_cast<quint64>(first);
    for (int i = 1; i < width; ++i) {
        val = (val << 8) | static_cast<quint64>(buf[pos + i]);
    }
    outVal = val;
    outWidth = width;
    return true;
}

void parseMkvFileContainer(QFile &file, qint64 fileSize, ContainerHeaderInfo &info) {
    if (!file.seek(0)) return;
    const qint64 scanBytes = std::min<qint64>(fileSize, 512 * 1024);
    const QByteArray headerBuf = file.read(scanBytes);
    if (headerBuf.size() < 4) return;

    const uchar *data = reinterpret_cast<const uchar *>(headerBuf.constData());
    const qint64 len = headerBuf.size();

    // 1. Locate first Matroska Cluster element (0x1F43B675) to isolate essence payload
    const QByteArray clusterId("\x1F\x43\xB6\x75", 4);
    const int clusterIdx = headerBuf.indexOf(clusterId);
    if (clusterIdx >= 0 && clusterIdx + 5 <= len) {
        quint64 clusterSize = 0;
        int vintWidth = 0;
        if (readEbmlVint(data, len, clusterIdx + 4, true, clusterSize, vintWidth)) {
            const qint64 payloadStart = clusterIdx + 4 + vintWidth;
            if (payloadStart < fileSize) {
                info.essenceOffset = payloadStart;
                const qint64 maxAvail = fileSize - payloadStart;
                // Unknown/streaming cluster size uses all-ones VINT; otherwise clamp to file bounds
                if (clusterSize > 0 && clusterSize < static_cast<quint64>(maxAvail)) {
                    info.essenceSize = static_cast<qint64>(clusterSize);
                } else {
                    info.essenceSize = maxAvail;
                }
            }
        }
    }

    // 2. Locate Segment Info (0x1549A966) and parse TimestampScale (0x2AD7B1) + Duration (0x4489)
    const QByteArray segInfoId("\x15\x49\xA9\x66", 4);
    const int segIdx = headerBuf.indexOf(segInfoId);
    if (segIdx >= 0 && segIdx + 5 <= len) {
        quint64 segInfoSize = 0;
        int vintWidth = 0;
        if (readEbmlVint(data, len, segIdx + 4, true, segInfoSize, vintWidth)) {
            qint64 p = segIdx + 4 + vintWidth;
            const qint64 segEnd = std::min<qint64>(len, p + static_cast<qint64>(segInfoSize));
            quint64 timestampScaleNs = 1000000ULL; // Default 1 ms per Matroska spec
            double rawDuration = 0.0;

            while (p < segEnd) {
                quint64 elemId = 0;
                int idWidth = 0;
                if (!readEbmlVint(data, segEnd, p, false, elemId, idWidth)) break;
                p += idWidth;

                quint64 elemSize = 0;
                int sizeWidth = 0;
                if (!readEbmlVint(data, segEnd, p, true, elemSize, sizeWidth)) break;
                p += sizeWidth;

                if (p + static_cast<qint64>(elemSize) > segEnd) break;

                if (elemId == 0x2AD7B1ULL && elemSize >= 1 && elemSize <= 8) {
                    quint64 scale = 0;
                    for (quint64 i = 0; i < elemSize; ++i) {
                        scale = (scale << 8) | data[p + i];
                    }
                    if (scale > 0) timestampScaleNs = scale;
                } else if (elemId == 0x4489ULL && (elemSize == 4 || elemSize == 8)) {
                    if (elemSize == 4) {
                        const quint32 bits = qFromBigEndian<quint32>(data + p);
                        float f = 0.0f;
                        std::memcpy(&f, &bits, sizeof(float));
                        rawDuration = static_cast<double>(f);
                    } else {
                        const quint64 bits = qFromBigEndian<quint64>(data + p);
                        double d = 0.0;
                        std::memcpy(&d, &bits, sizeof(double));
                        rawDuration = d;
                    }
                }
                p += static_cast<qint64>(elemSize);
            }

            if (rawDuration > 0.0 && std::isfinite(rawDuration)) {
                info.durationMs = std::llround((rawDuration * static_cast<double>(timestampScaleNs)) / 1000000.0);
            }
        }
    }

    // 3. Detect Matroska CodecID strings in header region
    if (headerBuf.contains("V_MPEG4/ISO/AVC")) info.videoCodec = QStringLiteral("H264");
    else if (headerBuf.contains("V_MPEGH/ISO/HEVC")) info.videoCodec = QStringLiteral("HEVC");
    else if (headerBuf.contains("V_AV1")) info.videoCodec = QStringLiteral("AV1");
    else if (headerBuf.contains("V_VP9")) info.videoCodec = QStringLiteral("VP9");

    if (headerBuf.contains("A_AAC")) info.audioCodec = QStringLiteral("AAC");
    else if (headerBuf.contains("A_EAC3")) info.audioCodec = QStringLiteral("EAC3");
    else if (headerBuf.contains("A_AC3")) info.audioCodec = QStringLiteral("AC3");
    else if (headerBuf.contains("A_OPUS")) info.audioCodec = QStringLiteral("OPUS");
    else if (headerBuf.contains("A_FLAC")) info.audioCodec = QStringLiteral("FLAC");
    else if (headerBuf.contains("A_VORBIS")) info.audioCodec = QStringLiteral("VORBIS");
}

void parseAviFileContainer(QFile &file, qint64 fileSize, ContainerHeaderInfo &info) {
    if (!file.seek(0)) return;
    const QByteArray headerBuf = file.read(std::min<qint64>(fileSize, 65536));
    if (headerBuf.size() < 32 || !headerBuf.startsWith("RIFF") || headerBuf.mid(8, 4) != "AVI ") {
        return;
    }
    const int avihIdx = headerBuf.indexOf("avih");
    if (avihIdx >= 0 && avihIdx + 8 + 24 <= headerBuf.size()) {
        const uchar *avih = reinterpret_cast<const uchar *>(headerBuf.constData() + avihIdx + 8);
        const quint32 usecPerFrame = qFromLittleEndian<quint32>(avih);
        const quint32 totalFrames  = qFromLittleEndian<quint32>(avih + 16);
        if (usecPerFrame > 0 && totalFrames > 0) {
            info.durationMs = static_cast<qint64>((static_cast<quint64>(usecPerFrame) * totalFrames) / 1000ULL);
            info.frameRate = 1000000.0 / static_cast<double>(usecPerFrame);
        }
        if (avihIdx + 8 + 40 <= headerBuf.size()) {
            info.width  = static_cast<int>(qFromLittleEndian<quint32>(avih + 32));
            info.height = static_cast<int>(qFromLittleEndian<quint32>(avih + 36));
        }
    }
    const int moviIdx = headerBuf.indexOf("movi");
    if (moviIdx >= 8) {
        const uchar *listHdr = reinterpret_cast<const uchar *>(headerBuf.constData() + moviIdx - 4);
        const quint32 moviSize = qFromLittleEndian<quint32>(listHdr);
        const qint64 payloadStart = moviIdx + 4;
        if (payloadStart < fileSize) {
            info.essenceOffset = payloadStart;
            info.essenceSize = (moviSize > 4 && payloadStart + moviSize - 4 <= fileSize)
                                   ? static_cast<qint64>(moviSize - 4)
                                   : (fileSize - payloadStart);
        }
    }
}

QString hashRegionWindows(QFile &file,
                          qint64 regionOffset,
                          qint64 regionSize,
                          int windowCount,
                          qint64 windowBytes,
                          LocalMediaInspector::CancelToken cancelToken) {
    if (regionSize <= 0 || windowCount <= 0 || windowBytes <= 0) {
        return QString();
    }
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    const QByteArray sizeTag = QByteArray::number(regionSize) + ":";
    hasher.addData(sizeTag);

    const qint64 chunkLen = std::min<qint64>(windowBytes, regionSize);
    for (int i = 0; i < windowCount; ++i) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            return QString();
        }
        qint64 relOffset = 0;
        if (windowCount > 1 && regionSize > chunkLen) {
            relOffset = ((regionSize - chunkLen) * i) / (windowCount - 1);
        }
        if (!file.seek(regionOffset + relOffset)) {
            return QString();
        }
        const QByteArray buf = file.read(chunkLen);
        if (buf.isEmpty()) {
            return QString();
        }
        hasher.addData(buf);
    }
    return QString::fromLatin1(hasher.result().toHex().left(16));
}

bool isKnownTechnicalToken(const QString &tok) {
    static const QSet<QString> kTechTokens = {
        QStringLiteral("480p"), QStringLiteral("576p"), QStringLiteral("720p"),
        QStringLiteral("1080p"), QStringLiteral("1080i"), QStringLiteral("1440p"),
        QStringLiteral("2160p"), QStringLiteral("4k"), QStringLiteral("uhd"),
        QStringLiteral("fhd"), QStringLiteral("hd"),
        QStringLiteral("bluray"), QStringLiteral("bdrip"), QStringLiteral("brrip"),
        QStringLiteral("webrip"), QStringLiteral("webdl"), QStringLiteral("web"),
        QStringLiteral("hdrip"), QStringLiteral("dvdrip"), QStringLiteral("hdtv"),
        QStringLiteral("remux"),
        QStringLiteral("x264"), QStringLiteral("h264"), QStringLiteral("avc"),
        QStringLiteral("x265"), QStringLiteral("h265"), QStringLiteral("hevc"),
        QStringLiteral("av1"), QStringLiteral("xvid"), QStringLiteral("divx"),
        QStringLiteral("10bit"), QStringLiteral("8bit"), QStringLiteral("hdr"),
        QStringLiteral("hdr10"), QStringLiteral("hdr10plus"), QStringLiteral("dv"),
        QStringLiteral("dolbyvision"),
        QStringLiteral("aac"), QStringLiteral("ac3"), QStringLiteral("eac3"),
        QStringLiteral("ddp5"), QStringLiteral("dd5"), QStringLiteral("dts"),
        QStringLiteral("hdma"), QStringLiteral("truehd"), QStringLiteral("atmos"),
        QStringLiteral("flac"), QStringLiteral("mp3"), QStringLiteral("opus"),
        QStringLiteral("dual"), QStringLiteral("multi"), QStringLiteral("proper"),
        QStringLiteral("repack"), QStringLiteral("yify"), QStringLiteral("yts"),
        QStringLiteral("rarbg"), QStringLiteral("psa"), QStringLiteral("qxr"),
        QStringLiteral("tigole")
    };
    return kTechTokens.contains(tok);
}

void splitFingerprintDigests(const QString &fp,
                             QString &outVersion,
                             QString &outByteDigest,
                             QString &outPayloadDigest) {
    outVersion.clear();
    outByteDigest.clear();
    outPayloadDigest.clear();
    const QStringList parts = fp.trimmed().split(QLatin1Char(':'));
    if (parts.size() >= 4 && parts[0] == QLatin1String("v1")) {
        outVersion = parts[0];
        outByteDigest = parts[3].toLower();
        outPayloadDigest = (parts.size() >= 5) ? parts[4].toLower() : outByteDigest;
    }
}

} // namespace

QString LocalMediaInspector::normalizeContainerFormat(const QString &filePathOrExt) {
    QString raw = filePathOrExt.trimmed();
    if (raw.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        raw = QUrl(raw).toLocalFile();
    }
    QString ext = raw;
    const int dotIdx = raw.lastIndexOf(QLatin1Char('.'));
    if (dotIdx >= 0) {
        ext = raw.mid(dotIdx + 1);
    }
    ext = ext.trimmed().toUpper();
    if (ext == QLatin1String("M4V")) {
        return QStringLiteral("MP4");
    }
    return ext;
}

bool LocalMediaInspector::isSupportedContainer(const QString &containerFormat) {
    const QString fmt = containerFormat.trimmed().toUpper();
    return fmt == QLatin1String("MKV")  ||
           fmt == QLatin1String("MP4")  ||
           fmt == QLatin1String("AVI")  ||
           fmt == QLatin1String("MOV")  ||
           fmt == QLatin1String("WEBM") ||
           fmt == QLatin1String("TS");
}

void LocalMediaInspector::parseTitleAndTags(const QString &rawNameOrTitle,
                                            QString &outNormalizedTitle,
                                            QString &outDisplayTitle,
                                            QString &outEpisodeTag,
                                            QString &outYearTag,
                                            QString &outEditionTag) {
    outNormalizedTitle.clear();
    outDisplayTitle.clear();
    outEpisodeTag.clear();
    outYearTag.clear();
    outEditionTag.clear();

    QString input = rawNameOrTitle.trimmed();
    if (input.isEmpty()) {
        return;
    }

    // Ensure any accidental path or URL prefix is stripped to the leaf filename
    if (input.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        input = QUrl(input).toLocalFile();
    }
    input.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int slashIdx = input.lastIndexOf(QLatin1Char('/'));
    if (slashIdx >= 0) {
        input = input.mid(slashIdx + 1);
    }

    // Strip known video container extension if present
    static const QRegularExpression kExtRx(
        QStringLiteral(R"(\.(mkv|mp4|m4v|avi|mov|webm|ts)$)"),
        QRegularExpression::CaseInsensitiveOption);
    input.remove(kExtRx);

    // Replace punctuation/separators with spaces
    QString cleaned = input;
    cleaned.replace(QRegularExpression(QStringLiteral(R"([._\-\[\]\(\)\{\}+]+)")), QStringLiteral(" "));
    cleaned = cleaned.simplified();

    const QString lowerAll = cleaned.toLower();

    // 1. Detect edition/cut phrases before token splitting
    if (lowerAll.contains(QLatin1String("director's cut")) ||
        lowerAll.contains(QLatin1String("directors cut"))) {
        outEditionTag = QStringLiteral("directors cut");
    } else if (lowerAll.contains(QLatin1String("extended cut")) ||
               lowerAll.contains(QLatin1String("extended edition")) ||
               QRegularExpression(QStringLiteral(R"(\bextended\b)")).match(lowerAll).hasMatch()) {
        outEditionTag = QStringLiteral("extended");
    } else if (lowerAll.contains(QLatin1String("theatrical cut")) ||
               QRegularExpression(QStringLiteral(R"(\btheatrical\b)")).match(lowerAll).hasMatch()) {
        outEditionTag = QStringLiteral("theatrical");
    } else if (QRegularExpression(QStringLiteral(R"(\bimax\b)")).match(lowerAll).hasMatch()) {
        outEditionTag = QStringLiteral("imax");
    } else if (QRegularExpression(QStringLiteral(R"(\buncut\b)")).match(lowerAll).hasMatch()) {
        outEditionTag = QStringLiteral("uncut");
    } else if (QRegularExpression(QStringLiteral(R"(\bunrated\b)")).match(lowerAll).hasMatch()) {
        outEditionTag = QStringLiteral("unrated");
    } else if (QRegularExpression(QStringLiteral(R"(\bremastered\b)")).match(lowerAll).hasMatch()) {
        outEditionTag = QStringLiteral("remastered");
    } else if (lowerAll.contains(QLatin1String("final cut"))) {
        outEditionTag = QStringLiteral("final cut");
    }

    // 2. Detect structured episode markers: S01E02, 1x02, Episode 2, E02
    static const QRegularExpression kSeEpRx(QStringLiteral(R"(\bs(\d{1,2})\s*e(\d{1,3})\b)"),
                                            QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kNxMRx(QStringLiteral(R"(\b(\d{1,2})x(\d{1,3})\b)"),
                                           QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression kEpWordRx(QStringLiteral(R"(\b(?:episode|ep)\s*(\d{1,3})\b)"),
                                              QRegularExpression::CaseInsensitiveOption);

    int episodeTokenIndex = -1;
    const QStringList rawTokens = cleaned.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (int i = 0; i < rawTokens.size(); ++i) {
        const QString tokLower = rawTokens[i].toLower();
        auto m1 = kSeEpRx.match(tokLower);
        if (m1.hasMatch()) {
            const int s = m1.captured(1).toInt();
            const int e = m1.captured(2).toInt();
            outEpisodeTag = QStringLiteral("s%1e%2")
                                .arg(s, 2, 10, QLatin1Char('0'))
                                .arg(e, 2, 10, QLatin1Char('0'));
            episodeTokenIndex = i;
            break;
        }
        auto m2 = kNxMRx.match(tokLower);
        if (m2.hasMatch()) {
            const int s = m2.captured(1).toInt();
            const int e = m2.captured(2).toInt();
            outEpisodeTag = QStringLiteral("s%1e%2")
                                .arg(s, 2, 10, QLatin1Char('0'))
                                .arg(e, 2, 10, QLatin1Char('0'));
            episodeTokenIndex = i;
            break;
        }
    }
    if (outEpisodeTag.isEmpty()) {
        auto mEp = kEpWordRx.match(lowerAll);
        if (mEp.hasMatch()) {
            const int e = mEp.captured(1).toInt();
            outEpisodeTag = QStringLiteral("s00e%1").arg(e, 2, 10, QLatin1Char('0'));
        }
    }

    // 3. Collect core title words prior to technical release tags / episode markers
    static const QRegularExpression kYearRx(QStringLiteral(R"(^(19[2-9]\d|20[0-2]\d|2030)$)"));
    QStringList normWords;
    QStringList dispWords;
    bool hitMetadataBoundary = false;

    for (int i = 0; i < rawTokens.size(); ++i) {
        const QString origTok = rawTokens[i];
        QString tokLower = origTok.toLower();
        tokLower.remove(QLatin1Char('\''));

        if (i == episodeTokenIndex) {
            hitMetadataBoundary = true;
            continue;
        }

        // Check if token is a 4-digit release year after at least one title word
        if (kYearRx.match(tokLower).hasMatch() && !normWords.isEmpty()) {
            // Treat as release year tag if it's at the end or followed by technical/edition tokens
            bool followedByTechOrEnd = (i == rawTokens.size() - 1);
            if (!followedByTechOrEnd) {
                const QString nextLower = rawTokens[i + 1].toLower();
                if (isKnownTechnicalToken(nextLower) ||
                    nextLower == QLatin1String("imax") ||
                    nextLower == QLatin1String("extended") ||
                    nextLower == QLatin1String("theatrical") ||
                    nextLower == QLatin1String("directors") ||
                    nextLower == QLatin1String("director's") ||
                    nextLower == QLatin1String("remastered") ||
                    nextLower == QLatin1String("uncut") ||
                    nextLower == QLatin1String("unrated") ||
                    i == episodeTokenIndex - 1) {
                    followedByTechOrEnd = true;
                }
            }
            if (followedByTechOrEnd && outYearTag.isEmpty()) {
                outYearTag = tokLower;
                hitMetadataBoundary = true;
                continue;
            }
        }

        // Skip edition words from core normalized title so EditionMismatch can be reported explicitly
        if (tokLower == QLatin1String("imax") ||
            tokLower == QLatin1String("extended") ||
            tokLower == QLatin1String("theatrical") ||
            tokLower == QLatin1String("uncut") ||
            tokLower == QLatin1String("unrated") ||
            tokLower == QLatin1String("remastered")) {
            hitMetadataBoundary = true;
            continue;
        }
        if ((tokLower == QLatin1String("directors") || tokLower == QLatin1String("final")) &&
            i + 1 < rawTokens.size() && rawTokens[i + 1].compare(QLatin1String("cut"), Qt::CaseInsensitive) == 0) {
            hitMetadataBoundary = true;
            ++i;
            continue;
        }

        if (isKnownTechnicalToken(tokLower)) {
            hitMetadataBoundary = true;
            continue;
        }

        if (!hitMetadataBoundary) {
            normWords.append(tokLower);
            dispWords.append(origTok);
        }
    }

    if (normWords.isEmpty() && !rawTokens.isEmpty()) {
        normWords.append(rawTokens.first().toLower());
        dispWords.append(rawTokens.first());
    }

    outNormalizedTitle = normWords.join(QLatin1Char(' '));
    outDisplayTitle = dispWords.join(QLatin1Char(' '));
    if (!outEpisodeTag.isEmpty()) {
        outDisplayTitle += QStringLiteral(" (%1)").arg(outEpisodeTag.toUpper());
    }
}

QString LocalMediaInspector::computeMultiWindowFingerprint(const QString &filePath,
                                                           int windowCount,
                                                           qint64 windowBytes,
                                                           CancelToken cancelToken) {
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    const qint64 fileSize = file.size();
    if (fileSize <= 0) {
        return QString();
    }

    const QString fmt = normalizeContainerFormat(filePath);
    ContainerHeaderInfo hdr;
    if (fmt == QLatin1String("MP4") || fmt == QLatin1String("MOV")) {
        parseMp4FileContainer(file, fileSize, hdr);
    } else if (fmt == QLatin1String("MKV") || fmt == QLatin1String("WEBM")) {
        parseMkvFileContainer(file, fileSize, hdr);
    } else if (fmt == QLatin1String("AVI")) {
        parseAviFileContainer(file, fileSize, hdr);
    }

    const QString byteDigest = hashRegionWindows(file, 0, fileSize, windowCount, windowBytes, cancelToken);
    if (byteDigest.isEmpty()) {
        return QString();
    }

    QString payloadDigest = byteDigest;
    if (hdr.essenceSize > 0 && hdr.essenceOffset >= 0 && hdr.essenceOffset + hdr.essenceSize <= fileSize) {
        payloadDigest = hashRegionWindows(
            file, hdr.essenceOffset, hdr.essenceSize, windowCount, windowBytes, cancelToken);
        if (payloadDigest.isEmpty()) {
            return QString();
        }
    }

    return QStringLiteral("v1:%1:%2:%3:%4")
        .arg(windowCount)
        .arg(windowBytes)
        .arg(byteDigest, payloadDigest);
}

QString LocalMediaInspector::computeFullFileSha256(const QString &filePath, CancelToken cancelToken) {
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        return QString();
    }
    QCryptographicHash hasher(QCryptographicHash::Sha256);
    constexpr qint64 kChunkSize = 256 * 1024;
    while (!file.atEnd()) {
        if (cancelToken && cancelToken->load(std::memory_order_relaxed)) {
            return QString();
        }
        const QByteArray chunk = file.read(kChunkSize);
        if (chunk.isEmpty()) break;
        hasher.addData(chunk);
    }
    return QString::fromLatin1(hasher.result().toHex().toLower());
}

LocalMediaDescriptor LocalMediaInspector::inspectFile(const QString &filePath,
                                                      const QString &titleHint,
                                                      qint64 knownDurationMs,
                                                      bool computeFullHash,
                                                      LocalCompatibilityReason *outErrorReason,
                                                      CancelToken cancelToken,
                                                       bool useCache,
                                                       bool computeFingerprint) {
    if (outErrorReason) {
        *outErrorReason = LocalCompatibilityReason::None;
    }

    QString localPath = filePath.trimmed();
    if (localPath.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        localPath = QUrl(localPath).toLocalFile();
    }

    LocalMediaDescriptor desc;
    if (localPath.isEmpty()) {
        if (outErrorReason) *outErrorReason = LocalCompatibilityReason::MissingFile;
        return desc;
    }

    const QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        if (outErrorReason) *outErrorReason = LocalCompatibilityReason::MissingFile;
        return desc;
    }

    const qint64 fileSize = fi.size();
    if (fileSize <= 0) {
        if (outErrorReason) *outErrorReason = LocalCompatibilityReason::UnreadableOrEmptyFile;
        return desc;
    }

    const QString fmt = normalizeContainerFormat(fi.fileName());
    if (!isSupportedContainer(fmt)) {
        if (outErrorReason) *outErrorReason = LocalCompatibilityReason::UnsupportedContainer;
        return desc;
    }

    const QString canonicalKey = fi.canonicalFilePath().isEmpty() ? fi.absoluteFilePath() : fi.canonicalFilePath();
    const qint64 mtimeMs = fi.lastModified().toMSecsSinceEpoch();

    // Check cache validity against (fileSizeBytes, lastModifiedMs)
    if (useCache) {
        QMutexLocker locker(&g_inspectorCacheMutex);
        auto it = g_inspectorCache.constFind(canonicalKey);
        if (it != g_inspectorCache.constEnd()) {
            const CachedMediaEntry &entry = it.value();
            if (entry.fileSizeBytes == fileSize &&
                entry.lastModifiedMs == mtimeMs &&
                (!computeFullHash || entry.hasFullHash) &&
                (!computeFingerprint || !entry.descriptor.contentFingerprint.isEmpty())) {
                LocalMediaDescriptor cached = entry.descriptor;
                if (!titleHint.trimmed().isEmpty()) {
                    QString hintNorm, hintDisp, hintEp, hintYr, hintEd;
                    parseTitleAndTags(titleHint, hintNorm, hintDisp, hintEp, hintYr, hintEd);
                    if (!hintDisp.isEmpty() && !LocalMediaDescriptor::containsPrivatePathLeak(hintDisp)) {
                        cached.displayTitle = hintDisp;
                    }
                }
                if (cached.durationMs <= 0 && knownDurationMs > 0) {
                    cached.durationMs = knownDurationMs;
                }
                if (cached.durationMs <= 0) {
                    if (outErrorReason) *outErrorReason = LocalCompatibilityReason::InsufficientMetadata;
                }
                return cached;
            }
        }
    }

    QFile file(localPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (outErrorReason) *outErrorReason = LocalCompatibilityReason::UnreadableOrEmptyFile;
        return desc;
    }

    ContainerHeaderInfo hdr;
    if (fmt == QLatin1String("MP4") || fmt == QLatin1String("MOV")) {
        parseMp4FileContainer(file, fileSize, hdr);
    } else if (fmt == QLatin1String("MKV") || fmt == QLatin1String("WEBM")) {
        parseMkvFileContainer(file, fileSize, hdr);
    } else if (fmt == QLatin1String("AVI")) {
        parseAviFileContainer(file, fileSize, hdr);
    }

    QString byteDigest;
    QString payloadDigest;
    if (computeFingerprint) {
        byteDigest = hashRegionWindows(
            file, 0, fileSize, kDefaultWindowCount, kDefaultWindowBytes, cancelToken);
        if (byteDigest.isEmpty()) {
            if (outErrorReason) *outErrorReason = LocalCompatibilityReason::UnreadableOrEmptyFile;
            return desc;
        }
        payloadDigest = byteDigest;
        if (hdr.essenceSize > 0 && hdr.essenceOffset >= 0 && hdr.essenceOffset + hdr.essenceSize <= fileSize) {
            payloadDigest = hashRegionWindows(
                file, hdr.essenceOffset, hdr.essenceSize, kDefaultWindowCount, kDefaultWindowBytes, cancelToken);
            if (payloadDigest.isEmpty()) {
                if (outErrorReason) *outErrorReason = LocalCompatibilityReason::UnreadableOrEmptyFile;
                return desc;
            }
        }
    }
    file.close();

    parseTitleAndTags(fi.fileName(),
                      desc.normalizedTitle,
                      desc.displayTitle,
                      desc.episodeTag,
                      desc.yearTag,
                      desc.editionTag);

    bool isNumericOnly = true;
    for (const QChar &ch : desc.normalizedTitle) {
        if (!ch.isDigit() && !ch.isSpace()) {
            isNumericOnly = false;
            break;
        }
    }
    if (isNumericOnly || desc.normalizedTitle.isEmpty()) {
        QDir parentDir = fi.dir();
        QString parentDirName = parentDir.dirName();
        if (parentDirName.compare(QLatin1String("Series"), Qt::CaseInsensitive) == 0 ||
            parentDirName.compare(QLatin1String("Individuals"), Qt::CaseInsensitive) == 0) {
            parentDirName.clear();
        }
        if (!parentDirName.isEmpty()) {
            QString pNorm, pDisp, pEp, pYr, pEd;
            parseTitleAndTags(parentDirName, pNorm, pDisp, pEp, pYr, pEd);
            if (!pNorm.isEmpty()) {
                desc.normalizedTitle = pNorm;
                if (!pDisp.isEmpty()) desc.displayTitle = pDisp;
                if (desc.yearTag.isEmpty() && !pYr.isEmpty()) desc.yearTag = pYr;
                if (desc.editionTag.isEmpty() && !pEd.isEmpty()) desc.editionTag = pEd;
                if (desc.episodeTag.isEmpty()) {
                    int epNum = fi.baseName().toInt();
                    if (epNum > 0) {
                        int sNum = 1;
                        static const QRegularExpression sRegex(QStringLiteral("s(\\d{1,2})"), QRegularExpression::CaseInsensitiveOption);
                        auto sm = sRegex.match(pEp);
                        if (sm.hasMatch()) sNum = sm.captured(1).toInt();
                        desc.episodeTag = QStringLiteral("s%1e%2").arg(sNum, 2, 10, QChar('0')).arg(epNum, 2, 10, QChar('0'));
                    }
                }
            }
        }
    }

    if (!titleHint.trimmed().isEmpty()) {
        QString hintNorm, hintDisp, hintEp, hintYr, hintEd;
        parseTitleAndTags(titleHint.trimmed(), hintNorm, hintDisp, hintEp, hintYr, hintEd);
        if (!hintDisp.isEmpty() && !LocalMediaDescriptor::containsPrivatePathLeak(hintDisp)) {
            desc.displayTitle = hintDisp;
        }
        if ((desc.normalizedTitle.isEmpty() || isNumericOnly) && !hintNorm.isEmpty()) desc.normalizedTitle = hintNorm;
        if (desc.episodeTag.isEmpty() && !hintEp.isEmpty())        desc.episodeTag = hintEp;
        if (desc.yearTag.isEmpty() && !hintYr.isEmpty())           desc.yearTag = hintYr;
        if (desc.editionTag.isEmpty() && !hintEd.isEmpty())        desc.editionTag = hintEd;
    }

    desc.fileSizeBytes = fileSize;
    desc.lastModifiedMs = mtimeMs;
    desc.containerFormat = fmt;
    desc.durationMs = (hdr.durationMs > 0) ? hdr.durationMs : knownDurationMs;
    desc.videoCodec = hdr.videoCodec;
    desc.audioCodec = hdr.audioCodec;
    desc.width = hdr.width;
    desc.height = hdr.height;
    desc.frameRate = hdr.frameRate;
    if (computeFingerprint) {
        desc.contentFingerprint = QStringLiteral("v1:%1:%2:%3:%4")
                                      .arg(kDefaultWindowCount)
                                      .arg(kDefaultWindowBytes)
                                      .arg(byteDigest, payloadDigest);
    }

    if (computeFullHash) {
        desc.fullFileSha256 = computeFullFileSha256(localPath, cancelToken);
    }

    if (useCache && (!cancelToken || !cancelToken->load(std::memory_order_relaxed))) {
        QMutexLocker locker(&g_inspectorCacheMutex);
        CachedMediaEntry entry;
        entry.fileSizeBytes = fileSize;
        entry.lastModifiedMs = mtimeMs;
        entry.hasFullHash = computeFullHash && !desc.fullFileSha256.isEmpty();
        entry.descriptor = desc;
        g_inspectorCache.insert(canonicalKey, entry);
    }

    if (desc.durationMs <= 0) {
        if (outErrorReason) *outErrorReason = LocalCompatibilityReason::InsufficientMetadata;
    }

    return desc;
}

LocalMediaInspector::CancelToken LocalMediaInspector::inspectFileAsync(QObject *context,
                                                                       const QString &filePath,
                                                                       const QString &titleHint,
                                                                       qint64 knownDurationMs,
                                                                       bool computeFullHash,
                                                                       InspectCallback callback,
                                                                       bool computeFingerprint) {
    auto cancelToken = std::make_shared<std::atomic<bool>>(false);
    QPointer<QObject> weakContext(context);

    QThreadPool::globalInstance()->start([weakContext, filePath, titleHint, knownDurationMs, computeFullHash, computeFingerprint, cancelToken, cb = std::move(callback)]() mutable {
        if (cancelToken->load(std::memory_order_relaxed)) {
            return;
        }
        LocalCompatibilityReason errReason = LocalCompatibilityReason::None;
        LocalMediaDescriptor desc = inspectFile(
            filePath, titleHint, knownDurationMs, computeFullHash, &errReason, cancelToken, true, computeFingerprint);
        if (cancelToken->load(std::memory_order_relaxed)) {
            return;
        }
        if (weakContext) {
            QMetaObject::invokeMethod(weakContext.data(), [cb = std::move(cb), desc = std::move(desc), errReason, cancelToken]() {
                if (!cancelToken->load(std::memory_order_relaxed) && cb) {
                    cb(desc, errReason);
                }
            }, Qt::QueuedConnection);
        }
    });

    return cancelToken;
}

bool LocalMediaInspector::hasFileChangedSince(const QString &filePath, const LocalMediaDescriptor &desc) {
    QString localPath = filePath.trimmed();
    if (localPath.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        localPath = QUrl(localPath).toLocalFile();
    }
    const QFileInfo fi(localPath);
    if (!fi.exists() || !fi.isFile()) {
        return true;
    }
    if (fi.size() != desc.fileSizeBytes) {
        return true;
    }
    if (desc.lastModifiedMs > 0 && fi.lastModified().toMSecsSinceEpoch() != desc.lastModifiedMs) {
        return true;
    }
    return false;
}

void LocalMediaInspector::clearCache() {
    QMutexLocker locker(&g_inspectorCacheMutex);
    g_inspectorCache.clear();
}

// ============================================================================
// LocalMediaVerifier
// ============================================================================

LocalCompatibilityResult LocalMediaVerifier::verify(const LocalMediaDescriptor &hostDesc,
                                                    const LocalMediaDescriptor &guestDesc,
                                                    const LocalVerificationPolicy &policy) {
    LocalCompatibilityResult res;

    // 1. Validate host descriptor completeness
    if (!hostDesc.isValidForSession()) {
        res.state = LocalVerificationState::UnresolvedMetadata;
        res.reason = LocalCompatibilityReason::InvalidSessionDescriptor;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }

    // 2. Validate guest file basic readability, container, and metadata completeness
    if (guestDesc.fileSizeBytes <= 0) {
        res.state = LocalVerificationState::PlaybackError;
        res.reason = LocalCompatibilityReason::UnreadableOrEmptyFile;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }
    if (guestDesc.containerFormat.isEmpty() ||
        !LocalMediaInspector::isSupportedContainer(guestDesc.containerFormat)) {
        res.state = LocalVerificationState::IncompatibleMedia;
        res.reason = LocalCompatibilityReason::UnsupportedContainer;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }
    if (guestDesc.normalizedTitle.isEmpty()) {
        res.state = LocalVerificationState::UnresolvedMetadata;
        res.reason = LocalCompatibilityReason::InsufficientMetadata;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }
    if (policy.requireContentFingerprint &&
        (guestDesc.contentFingerprint.isEmpty() || !guestDesc.contentFingerprint.startsWith(QLatin1String("v1:")))) {
        res.state = LocalVerificationState::UnresolvedMetadata;
        res.reason = LocalCompatibilityReason::InsufficientMetadata;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }

    // 3. Container & file size policy checks (if configured strictly)
    if (!policy.allowDifferentContainers &&
        hostDesc.containerFormat.compare(guestDesc.containerFormat, Qt::CaseInsensitive) != 0) {
        res.state = LocalVerificationState::IncompatibleMedia;
        res.reason = LocalCompatibilityReason::UnsupportedContainer;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }
    if (!policy.allowDifferentFileSizes && hostDesc.fileSizeBytes != guestDesc.fileSizeBytes) {
        res.state = LocalVerificationState::IncompatibleMedia;
        res.reason = LocalCompatibilityReason::FingerprintMismatch;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }

    // 4. Parse content fingerprints first so we know if the files are byte-identical
    QString hostFpVer, hostByteDigest, hostPayloadDigest;
    QString guestFpVer, guestByteDigest, guestPayloadDigest;
    splitFingerprintDigests(hostDesc.contentFingerprint, hostFpVer, hostByteDigest, hostPayloadDigest);
    splitFingerprintDigests(guestDesc.contentFingerprint, guestFpVer, guestByteDigest, guestPayloadDigest);

    res.exactContentMatch = (!hostByteDigest.isEmpty() &&
                             hostByteDigest == guestByteDigest &&
                             hostDesc.fileSizeBytes == guestDesc.fileSizeBytes);
    res.payloadEssenceMatch = res.exactContentMatch ||
                              (!hostPayloadDigest.isEmpty() && hostPayloadDigest == guestPayloadDigest);

    // 5. Title, Episode, and Edition comparison
    //    Note: Do not assume differently named files are identical unless their normalized title
    //    matches (or they are byte-for-byte identical copies of the exact same file).
    res.titleMatched = (hostDesc.normalizedTitle.compare(guestDesc.normalizedTitle, Qt::CaseInsensitive) == 0);
    if (!res.titleMatched && !res.exactContentMatch) {
        res.state = LocalVerificationState::IncompatibleMedia;
        res.reason = LocalCompatibilityReason::TitleMismatch;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }

    // If either side has an episode tag, they must match (prevents S01E01 matching S01E02 of same duration)
    if ((!hostDesc.episodeTag.isEmpty() || !guestDesc.episodeTag.isEmpty()) &&
        hostDesc.episodeTag.compare(guestDesc.episodeTag, Qt::CaseInsensitive) != 0 &&
        !res.exactContentMatch) {
        res.state = LocalVerificationState::IncompatibleMedia;
        res.reason = LocalCompatibilityReason::EpisodeMismatch;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }

    // If both specify an edition/cut tag (e.g. "extended" vs "theatrical"), they must match
    if (!hostDesc.editionTag.isEmpty() && !guestDesc.editionTag.isEmpty() &&
        hostDesc.editionTag.compare(guestDesc.editionTag, Qt::CaseInsensitive) != 0 &&
        !res.exactContentMatch) {
        res.state = LocalVerificationState::IncompatibleMedia;
        res.reason = LocalCompatibilityReason::EditionMismatch;
        res.userMessage = localCompatibilityReasonToMessage(res.reason);
        return res;
    }

    // 6. Duration tolerance check:
    // When both durations are known (> 0), enforce duration tolerance to prevent trailers or different cuts matching.
    if (hostDesc.durationMs > 0 && guestDesc.durationMs > 0) {
        res.durationDeltaMs = std::llabs(hostDesc.durationMs - guestDesc.durationMs);
        res.durationWithinTolerance = (res.durationDeltaMs <= std::max<qint64>(0, policy.durationToleranceMs));
        if (!res.durationWithinTolerance) {
            res.state = LocalVerificationState::IncompatibleMedia;
            res.reason = LocalCompatibilityReason::DurationMismatch;
            res.userMessage = localCompatibilityReasonToMessage(res.reason);
            return res;
        }
    } else {
        res.durationDeltaMs = 0;
        res.durationWithinTolerance = true;
    }

    // 7. Content fingerprint verification
    if (policy.requireContentFingerprint) {
        if (hostByteDigest.isEmpty() || guestByteDigest.isEmpty()) {
            res.state = LocalVerificationState::UnresolvedMetadata;
            res.reason = LocalCompatibilityReason::InsufficientMetadata;
            res.userMessage = localCompatibilityReasonToMessage(res.reason);
            return res;
        }

        if (!res.exactContentMatch && !res.payloadEssenceMatch && !policy.allowSameTitleAndDurationWhenRemuxed) {
            res.state = LocalVerificationState::IncompatibleMedia;
            res.reason = LocalCompatibilityReason::FingerprintMismatch;
            res.userMessage = localCompatibilityReasonToMessage(res.reason);
            return res;
        }
    }

    // 8. Optional strict full-file SHA-256 verification
    if (policy.requireExactFullFileHash) {
        if (hostDesc.fullFileSha256.isEmpty() || guestDesc.fullFileSha256.isEmpty()) {
            res.state = LocalVerificationState::UnresolvedMetadata;
            res.reason = LocalCompatibilityReason::InsufficientMetadata;
            res.userMessage = localCompatibilityReasonToMessage(res.reason);
            return res;
        }
        if (hostDesc.fullFileSha256.compare(guestDesc.fullFileSha256, Qt::CaseInsensitive) != 0) {
            res.state = LocalVerificationState::IncompatibleMedia;
            res.reason = LocalCompatibilityReason::StrictHashMismatch;
            res.userMessage = localCompatibilityReasonToMessage(res.reason);
            return res;
        }
    }

    res.state = LocalVerificationState::VerifiedCompatible;
    res.reason = (res.exactContentMatch &&
                  hostDesc.containerFormat.compare(guestDesc.containerFormat, Qt::CaseInsensitive) == 0)
                     ? LocalCompatibilityReason::MatchedExact
                     : LocalCompatibilityReason::MatchedRemuxOrReencode;
    res.userMessage = localCompatibilityReasonToMessage(res.reason);
    return res;
}

} // namespace Flux

