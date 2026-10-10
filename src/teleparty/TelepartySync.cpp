#include "TelepartySync.h"
#include "TelepartySession.h"
#include "../media/VLCPlayer.h"
#include "../core/Logger.h"

#include <QDateTime>
#include <QUrl>
#include <algorithm>

namespace Flux {

namespace {

constexpr int kMaxUrlLength = 2048;
constexpr int kMaxTitleLength = 300;
constexpr qint64 kOpenDedupeMs = 8000;     // same video opened twice within this = one open
constexpr int kSeekMergeMs = 200;          // local seeks are merged over this window
constexpr int kStateWaitMs = 5000;         // how long we accept a "state" answer after asking
constexpr qint64 kPlayDriftMs = 1500;      // playing: ignore drift smaller than this
constexpr qint64 kPauseDriftMs = 500;      // paused: everybody should show the same frame
constexpr qint64 kCatchUpDriftMs = 1500;

qint64 nowMs() {
    return QDateTime::currentMSecsSinceEpoch();
}

qint64 cleanTime(const QVariant &v) {
    // Positions come from other machines: never trust them blindly
    const qint64 t = v.toLongLong();
    constexpr qint64 kMaxMs = 48LL * 3600 * 1000;
    return std::clamp<qint64>(t, 0, kMaxMs);
}

} // namespace

TelepartySync::TelepartySync(TelepartySession *session, VLCPlayer *player, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_player(player) {

    m_stateTimer.setSingleShot(true);
    connect(&m_stateTimer, &QTimer::timeout, this, [this]() { m_needState = false; });

    m_seekTimer.setSingleShot(true);
    m_seekTimer.setInterval(kSeekMergeMs);
    connect(&m_seekTimer, &QTimer::timeout, this, &TelepartySync::flushSeek);

    // Session -> player
    connect(m_session, &TelepartySession::eventReceived, this, &TelepartySync::onEvent);
    connect(m_session, &TelepartySession::stateChanged, this, &TelepartySync::onSessionStateChanged);
    connect(m_session, &TelepartySession::memberJoined, this, [this](const QString &) {
        // Someone (re)joined: tell them what we are watching
        sendState();
    });
    connect(m_session, &TelepartySession::joinedSession, this, [this]() {
        resetParty();
        // A newcomer learns what the party is watching from the members' answers to its hello
        if (!m_session->isHost()) armNeedState();
    });

    // Player -> session
    connect(m_player, &VLCPlayer::userToggledPlay, this, &TelepartySync::onUserToggledPlay);
    connect(m_player, &VLCPlayer::userSeeked, this, &TelepartySync::onUserSeeked);
    connect(m_player, &VLCPlayer::stateChanged, this, &TelepartySync::onPlayerStateChanged);
}

// ============================================================================
// UI hooks
// ============================================================================

void TelepartySync::setWatching(bool watching) {
    if (m_watching == watching) return;
    m_watching = watching;
    emit watchingChanged();

    // Coming back to a video the party is on (e.g. via "Now Playing"): catch up with it
    if (watching && m_session->active() && !m_currentUrl.isEmpty() && m_player->url() == m_currentUrl) {
        armNeedState();
        m_session->sendEvent(QStringLiteral("syncme"));
    }
}

bool TelepartySync::isStreamUrl(const QString &url) const {
    if (url.isEmpty() || url.size() > kMaxUrlLength) return false;
    const QUrl u(url);
    const QString scheme = u.scheme().toLower();
    return u.isValid() && (scheme == QLatin1String("http") || scheme == QLatin1String("https"))
           && !u.host().isEmpty();
}

void TelepartySync::broadcastOpen(const QString &url, const QString &title) {
    if (!m_session->active() || !isStreamUrl(url)) return;

    flushSeek();
    noteOpen(url, title.left(kMaxTitleLength));
    m_needState = false;
    m_stateTimer.stop();

    QVariantMap d;
    d.insert(QStringLiteral("url"), url);
    d.insert(QStringLiteral("title"), m_currentTitle);
    m_session->sendEvent(QStringLiteral("open"), d);
}

void TelepartySync::adoptCurrent(const QString &url, const QString &title) {
    if (!m_session->active() || !isStreamUrl(url)) return;
    m_currentUrl = url;
    m_currentTitle = title.left(kMaxTitleLength);
}

// ============================================================================
// Local user -> everyone else
// ============================================================================

void TelepartySync::onUserToggledPlay(bool playing, qint64 timeMs) {
    if (!m_session->active() || m_currentUrl.isEmpty()) return;

    flushSeek();   // keep the order the user performed things in
    QVariantMap d;
    d.insert(QStringLiteral("t"), qMax<qint64>(0, timeMs));
    m_session->sendEvent(playing ? QStringLiteral("play") : QStringLiteral("pause"), d);
}

void TelepartySync::onUserSeeked(qint64 timeMs) {
    if (!m_session->active() || m_currentUrl.isEmpty()) return;

    m_pendingSeekMs = qMax<qint64>(0, timeMs);
    m_hasPendingSeek = true;
    m_seekTimer.start();
}

void TelepartySync::flushSeek() {
    m_seekTimer.stop();
    if (!m_hasPendingSeek) return;
    m_hasPendingSeek = false;

    if (!m_session->active() || m_currentUrl.isEmpty()) return;
    QVariantMap d;
    d.insert(QStringLiteral("t"), m_pendingSeekMs);
    m_session->sendEvent(QStringLiteral("seek"), d);
}

void TelepartySync::sendState() {
    if (!m_session->active() || !m_watching || m_currentUrl.isEmpty()) return;
    if (m_needState || m_player->url() != m_currentUrl) return;   // not in a position to answer

    QVariantMap d;
    d.insert(QStringLiteral("url"), m_currentUrl);
    d.insert(QStringLiteral("title"), m_currentTitle);
    d.insert(QStringLiteral("t"), m_player->timeMs());
    d.insert(QStringLiteral("playing"), playerRunning());
    m_session->sendEvent(QStringLiteral("state"), d);
}

// ============================================================================
// Everyone else -> local player
// ============================================================================

void TelepartySync::onEvent(const QString &type, const QVariantMap &data, const QString &) {
    if (type == QLatin1String("open")) {
        handleOpen(data);
    } else if (type == QLatin1String("state")) {
        handleState(data);
    } else if (type == QLatin1String("syncme")) {
        sendState();
    } else if (type == QLatin1String("play") || type == QLatin1String("pause") || type == QLatin1String("seek")) {
        // Not on the player page (or nothing shared yet): nothing to keep in step. Coming
        // back to the player triggers a catch-up instead.
        if (!m_watching || m_currentUrl.isEmpty()) return;

        const qint64 t = cleanTime(data.value(QStringLiteral("t")));
        if (type == QLatin1String("play")) {
            align(t, true, QStringLiteral("Resumed by a member"));
        } else if (type == QLatin1String("pause")) {
            align(t, false, QStringLiteral("Paused by a member"));
        } else {
            applySeek(t);
        }
    }
}

void TelepartySync::handleOpen(const QVariantMap &data) {
    const QString url = data.value(QStringLiteral("url")).toString();
    if (!isStreamUrl(url)) return;   // only streams can be shared (and nothing else is ever opened)

    // Autoplay can make two members open the same next episode at the same moment
    if (url == m_lastOpenUrl && nowMs() - m_lastOpenAt < kOpenDedupeMs) return;

    const QString title = data.value(QStringLiteral("title")).toString().left(kMaxTitleLength);
    noteOpen(url, title);
    m_needState = false;
    m_stateTimer.stop();

    FLUX_LOG_INFO("Teleparty", QString("A member opened: %1").arg(title.isEmpty() ? url : title));
    emit remoteOpenRequested(url, title, 0);
}

void TelepartySync::handleState(const QVariantMap &data) {
    if (!m_needState) return;   // we did not ask (or already have an answer)

    const QString url = data.value(QStringLiteral("url")).toString();
    if (!isStreamUrl(url)) return;

    m_needState = false;
    m_stateTimer.stop();

    const QString title = data.value(QStringLiteral("title")).toString().left(kMaxTitleLength);
    const qint64 t = cleanTime(data.value(QStringLiteral("t")));
    const bool playing = data.value(QStringLiteral("playing"), true).toBool();

    // Already on that video (came back to the player): just line up with the others
    if (m_watching && url == m_player->url()) {
        m_currentUrl = url;
        m_currentTitle = title;
        align(t, playing, QString());
        return;
    }

    // Otherwise open it at the party's position. libVLC needs a moment to start, so the
    // exact position is corrected once playback actually begins (see onPlayerStateChanged).
    noteOpen(url, title);
    m_pendingSync = true;
    m_pendingTimeMs = t;
    m_pendingPlaying = playing;
    m_pendingClock.start();

    FLUX_LOG_INFO("Teleparty", QString("Catching up with the party: %1 at %2")
                                   .arg(title.isEmpty() ? url : title, formatTime(t)));
    emit remoteOpenRequested(url, title, t);
}

void TelepartySync::align(qint64 timeMs, bool playing, const QString &note) {
    const QString st = m_player->state();

    if (st == QLatin1String("Ended") || st == QLatin1String("Stopped")) {
        if (playing) m_player->remoteRestart(timeMs);
    } else if (playing) {
        if (qAbs(m_player->timeMs() - timeMs) > kPlayDriftMs) m_player->remoteSeekTo(timeMs);
        if (m_player->isPaused()) m_player->remoteResume();
    } else {
        m_player->remotePause();
        if (qAbs(m_player->timeMs() - timeMs) > kPauseDriftMs) m_player->remoteSeekTo(timeMs);
    }

    if (!note.isEmpty()) emit activity(note);
}

void TelepartySync::applySeek(qint64 timeMs) {
    m_player->remoteSeekTo(timeMs);
    emit activity(QStringLiteral("Skipped to ") + formatTime(timeMs));
}

void TelepartySync::onPlayerStateChanged() {
    if (!m_pendingSync) return;

    const QString st = m_player->state();
    if (st == QLatin1String("Error")) {
        m_pendingSync = false;
        return;
    }
    if (st != QLatin1String("Playing")) return;

    m_pendingSync = false;
    // The party kept playing while we were loading
    const qint64 target = m_pendingTimeMs + (m_pendingPlaying ? m_pendingClock.elapsed() : 0);
    if (qAbs(m_player->timeMs() - target) > kCatchUpDriftMs) m_player->remoteSeekTo(target);
    if (!m_pendingPlaying) m_player->remotePause();
}

// ============================================================================
// Session bookkeeping
// ============================================================================

void TelepartySync::onSessionStateChanged() {
    const QString s = m_session->state();

    if (s == QLatin1String("idle")) {
        resetParty();
    } else if (s == QLatin1String("joined") && m_prevSessionState == QLatin1String("reconnecting")) {
        // We may have missed things while offline; our re-announcement makes the other
        // members send their state, which we are now ready to accept
        armNeedState();
    }
    m_prevSessionState = s;
}

void TelepartySync::armNeedState() {
    m_needState = true;
    m_stateTimer.start(kStateWaitMs);
}

void TelepartySync::noteOpen(const QString &url, const QString &title) {
    m_currentUrl = url;
    m_currentTitle = title;
    m_lastOpenUrl = url;
    m_lastOpenAt = nowMs();
    m_pendingSync = false;
}

void TelepartySync::resetParty() {
    m_currentUrl.clear();
    m_currentTitle.clear();
    m_lastOpenUrl.clear();
    m_lastOpenAt = 0;
    m_needState = false;
    m_pendingSync = false;
    m_hasPendingSeek = false;
    m_stateTimer.stop();
    m_seekTimer.stop();
}

bool TelepartySync::playerRunning() const {
    const QString st = m_player->state();
    return st == QLatin1String("Playing") || st == QLatin1String("Opening") || st.startsWith(QLatin1String("Buffering"));
}

QString TelepartySync::formatTime(qint64 ms) {
    const qint64 total = qMax<qint64>(0, ms) / 1000;
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    if (h > 0) {
        return QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
    }
    return QStringLiteral("%1:%2").arg(m).arg(s, 2, 10, QLatin1Char('0'));
}

} // namespace Flux
