#include "VLCPlayer.h"
#include "VLCInstance.h"
#include "../core/Logger.h"
#include <QVariantMap>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace Flux {

VLCPlayer::VLCPlayer(QObject *parent)
    : QObject(parent) {

    auto *vlc = VLCInstance::instance().get();
    if (!vlc) {
        FLUX_LOG_ERROR("VLCPlayer", "libVLC instance is not initialized!");
        return;
    }

    m_mediaPlayer = libvlc_media_player_new(vlc);
    if (!m_mediaPlayer) {
        FLUX_LOG_ERROR("VLCPlayer", "Failed to create libVLC media player!");
        return;
    }

    setupVlcEvents();

    // Fallback polling timer (250ms) to ensure UI timeline stays smooth even if VLC event frequency varies
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(250);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        if (!m_mediaPlayer || !isPlaying()) return;

        // While a seek is pending/settling, libVLC keeps reporting the OLD position for a
        // moment. Ignoring it stops the slider and clock jumping back and forth (the
        // brief "freeze" feel after seeking).
        if (!seekGuardActive()) {
            qint64 t = libvlc_media_player_get_time(m_mediaPlayer);
            if (t >= 0 && t != m_timeMs) {
                m_timeMs = t;
                emit timeChanged();
            }

            float p = libvlc_media_player_get_position(m_mediaPlayer);
            if (p >= 0.0f && std::abs(p - static_cast<float>(m_position)) > 0.001f) {
                m_position = p;
                emit positionChanged();
            }
        }

        qint64 len = libvlc_media_player_get_length(m_mediaPlayer);
        if (len > 0 && len != m_durationMs) {
            m_durationMs = len;
            emit durationChanged();
            updateTracks();
        }
    });

    // Seek coalescing timer: at most one libVLC seek every 120ms, always with the latest target
    m_uptime.start();
    m_seekTimer = new QTimer(this);
    m_seekTimer->setSingleShot(true);
    m_seekTimer->setInterval(120);
    connect(m_seekTimer, &QTimer::timeout, this, [this]() { applyPendingSeek(); });

    FLUX_LOG_INFO("VLCPlayer", "VLCPlayer created successfully");
}

VLCPlayer::~VLCPlayer() {
    stop();
    detachVlcEvents();

    if (m_currentMedia) {
        libvlc_media_release(m_currentMedia);
        m_currentMedia = nullptr;
    }

    if (m_mediaPlayer) {
        libvlc_media_player_release(m_mediaPlayer);
        m_mediaPlayer = nullptr;
    }
}

void VLCPlayer::setupVlcEvents() {
    if (!m_mediaPlayer) return;

    libvlc_event_manager_t *em = libvlc_media_player_event_manager(m_mediaPlayer);
    if (!em) return;

    const libvlc_event_type_t events[] = {
        libvlc_MediaPlayerOpening,
        libvlc_MediaPlayerBuffering,
        libvlc_MediaPlayerPlaying,
        libvlc_MediaPlayerPaused,
        libvlc_MediaPlayerStopped,
        libvlc_MediaPlayerEndReached,
        libvlc_MediaPlayerEncounteredError,
        libvlc_MediaPlayerTimeChanged,
        libvlc_MediaPlayerPositionChanged,
        libvlc_MediaPlayerLengthChanged,
        libvlc_MediaPlayerVout,
    };

    for (auto ev : events) {
        libvlc_event_attach(em, ev, handleVlcEvent, this);
    }
}

void VLCPlayer::detachVlcEvents() {
    if (!m_mediaPlayer) return;

    libvlc_event_manager_t *em = libvlc_media_player_event_manager(m_mediaPlayer);
    if (!em) return;

    const libvlc_event_type_t events[] = {
        libvlc_MediaPlayerOpening,
        libvlc_MediaPlayerBuffering,
        libvlc_MediaPlayerPlaying,
        libvlc_MediaPlayerPaused,
        libvlc_MediaPlayerStopped,
        libvlc_MediaPlayerEndReached,
        libvlc_MediaPlayerEncounteredError,
        libvlc_MediaPlayerTimeChanged,
        libvlc_MediaPlayerPositionChanged,
        libvlc_MediaPlayerLengthChanged,
        libvlc_MediaPlayerVout,
    };

    for (auto ev : events) {
        libvlc_event_detach(em, ev, handleVlcEvent, this);
    }
}

void VLCPlayer::handleVlcEvent(const libvlc_event_t *event, void *opaque) {
    auto *player = static_cast<VLCPlayer*>(opaque);
    if (!player) return;

    switch (event->type) {
    case libvlc_MediaPlayerOpening:
        QMetaObject::invokeMethod(player, [player]() {
            player->m_state = "Opening";
            player->m_isBuffering = true;
            player->m_errorMessage.clear();
            emit player->stateChanged();
            emit player->bufferingChanged();
            FLUX_LOG_INFO("VLCPlayer", "Media opening...");
        }, Qt::QueuedConnection);
        break;

    case libvlc_MediaPlayerBuffering: {
        float percent = event->u.media_player_buffering.new_cache;
        QMetaObject::invokeMethod(player, [player, percent]() {
            player->m_bufferingPercent = percent;
            player->m_isBuffering = (percent < 100.0f);
            if (player->m_isBuffering) {
                player->m_state = QString("Buffering %1%").arg(static_cast<int>(percent));
            } else if (player->isPlaying()) {
                player->m_state = "Playing";
            }
            emit player->bufferingChanged();
            emit player->stateChanged();
        }, Qt::QueuedConnection);
        break;
    }

    case libvlc_MediaPlayerPlaying:
        QMetaObject::invokeMethod(player, [player]() {
            player->m_state = "Playing";
            player->m_isBuffering = false;
            player->m_pollTimer->start();
            // Re-apply the user's volume/mute: libVLC's audio output only exists once playback
            // has started, and a brand-new stream would otherwise reset to 100%.
            libvlc_audio_set_volume(player->m_mediaPlayer, player->m_volume);
            libvlc_audio_set_mute(player->m_mediaPlayer, player->m_muted ? 1 : 0);
            emit player->stateChanged();
            emit player->bufferingChanged();
            player->updateTracks();
            FLUX_LOG_INFO("VLCPlayer", "Playback started");
        }, Qt::QueuedConnection);
        break;

    case libvlc_MediaPlayerPaused:
        QMetaObject::invokeMethod(player, [player]() {
            player->m_state = "Paused";
            emit player->stateChanged();
            FLUX_LOG_INFO("VLCPlayer", "Playback paused");
        }, Qt::QueuedConnection);
        break;

    case libvlc_MediaPlayerStopped:
        QMetaObject::invokeMethod(player, [player]() {
            player->m_state = "Stopped";
            player->m_isBuffering = false;
            player->m_pollTimer->stop();
            emit player->stateChanged();
            emit player->bufferingChanged();
            FLUX_LOG_INFO("VLCPlayer", "Playback stopped");
        }, Qt::QueuedConnection);
        break;

    case libvlc_MediaPlayerEndReached:
        QMetaObject::invokeMethod(player, [player]() {
            player->m_state = "Ended";
            player->m_pollTimer->stop();
            emit player->stateChanged();
            FLUX_LOG_INFO("VLCPlayer", "Reached end of media");
        }, Qt::QueuedConnection);
        break;

    case libvlc_MediaPlayerEncounteredError:
        QMetaObject::invokeMethod(player, [player]() {
            player->m_state = "Error";
            player->m_isBuffering = false;
            player->m_pollTimer->stop();
            player->m_errorMessage = "libVLC encountered an error while opening or playing this stream.";
            emit player->stateChanged();
            emit player->bufferingChanged();
            emit player->errorOccurred(player->m_errorMessage);
            FLUX_LOG_ERROR("VLCPlayer", QString("Playback failed: %1").arg(player->m_errorMessage));
        }, Qt::QueuedConnection);
        break;

    case libvlc_MediaPlayerLengthChanged: {
        qint64 len = event->u.media_player_length_changed.new_length;
        QMetaObject::invokeMethod(player, [player, len]() {
            player->m_durationMs = len;
            emit player->durationChanged();
            player->updateTracks();
        }, Qt::QueuedConnection);
        break;
    }

    case libvlc_MediaPlayerVout:
        QMetaObject::invokeMethod(player, [player]() {
            player->updateTracks();
        }, Qt::QueuedConnection);
        break;

    default:
        break;
    }
}

void VLCPlayer::setUrl(const QString &url) {
    if (m_url == url) return;
    m_url = url;
    emit urlChanged();
}

void VLCPlayer::play(const QString &mediaUrl) {
    if (!mediaUrl.isEmpty()) {
        setUrl(mediaUrl);
    }

    if (m_url.isEmpty()) {
        FLUX_LOG_WARN("VLCPlayer", "play() requested but URL is empty");
        return;
    }

    auto *vlc = VLCInstance::instance().get();
    if (!vlc || !m_mediaPlayer) {
        FLUX_LOG_ERROR("VLCPlayer", "Cannot play: libVLC not initialized");
        return;
    }

    FLUX_LOG_INFO("VLCPlayer", QString("Starting direct stream: %1").arg(m_url));

    stop();

    if (m_currentMedia) {
        libvlc_media_release(m_currentMedia);
        m_currentMedia = nullptr;
    }

    // Direct HTTP Range streaming from URL
    m_currentMedia = libvlc_media_new_location(vlc, m_url.toUtf8().constData());
    if (!m_currentMedia) {
        m_errorMessage = QString("Failed to create libVLC media object for URL: %1").arg(m_url);
        m_state = "Error";
        emit stateChanged();
        emit errorOccurred(m_errorMessage);
        FLUX_LOG_ERROR("VLCPlayer", m_errorMessage);
        return;
    }

    // Configure options for progressive HTTP range streaming.
    //  - network-caching: pre-roll (ms) that must be filled before playback resumes. This is
    //    what you wait for after every seek, so keep it short on a fast LAN.
    //  - prefetch-*: BYTE-level read-ahead in front of the demuxer. This is what makes
    //    playback smooth and lets short forward seeks be served from memory with no new
    //    HTTP request. (Sizes: KiB / bytes.) Unknown options are ignored harmlessly.
    libvlc_media_add_option(m_currentMedia, ":network-caching=1500");
    libvlc_media_add_option(m_currentMedia, ":http-reconnect");
    libvlc_media_add_option(m_currentMedia, ":prefetch-buffer-size=131072");
    libvlc_media_add_option(m_currentMedia, ":prefetch-read-size=262144");

    // Resume: begin directly at the saved position instead of starting at 0 and seeking
    // (avoids a wasted buffer at the start of the file). One-shot.
    const qint64 startMs = m_startTimeMs;
    m_startTimeMs = 0;
    if (startMs > 1000) {
        const QByteArray startOpt = QString(":start-time=%1").arg(static_cast<double>(startMs) / 1000.0, 0, 'f', 2).toUtf8();
        libvlc_media_add_option(m_currentMedia, startOpt.constData());
        FLUX_LOG_INFO("VLCPlayer", QString("Resuming from %1 ms").arg(startMs));
    }

    libvlc_media_player_set_media(m_mediaPlayer, m_currentMedia);

    // A new stream starts fresh: drop any seek still queued for the previous one
    m_hasPendingSeek = false;
    m_seekGuardUntil = 0;
    if (m_seekTimer) m_seekTimer->stop();

    // New stream: language auto-selection starts over
    m_audioPrefApplied = false;
    m_subPrefApplied = false;
    m_autoTrackAttempts = 0;

    // Show the resume position immediately instead of flashing 00:00
    if (startMs > 1000) {
        m_timeMs = startMs;
        emit timeChanged();
        m_seekGuardUntil = m_uptime.elapsed() + 2000;
    }

    m_state = "Opening";
    m_isBuffering = true;
    m_errorMessage.clear();
    emit stateChanged();
    emit bufferingChanged();

    int res = libvlc_media_player_play(m_mediaPlayer);
    if (res != 0) {
        m_errorMessage = "libvlc_media_player_play call returned error code.";
        m_state = "Error";
        emit stateChanged();
        emit errorOccurred(m_errorMessage);
        FLUX_LOG_ERROR("VLCPlayer", m_errorMessage);
    }
}

void VLCPlayer::pause() {
    if (!m_mediaPlayer) return;
    libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
    if (st == libvlc_Playing || st == libvlc_Buffering || st == libvlc_Opening) {
        libvlc_media_player_pause(m_mediaPlayer);
        m_state = "Paused";
        emit stateChanged();
        FLUX_LOG_INFO("VLCPlayer", "Playback paused immediately");
    }
}

void VLCPlayer::resume() {
    if (m_mediaPlayer && isPaused()) {
        libvlc_media_player_play(m_mediaPlayer);
    }
}

void VLCPlayer::togglePlay() {
    if (!m_mediaPlayer) return;

    // Decide from libVLC's real state. The old check (isPlaying() == false => play())
    // treated "Buffering"/"Opening" as stopped and RESTARTED the stream from the
    // beginning whenever you clicked during a buffer.
    libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
    if (st == libvlc_Playing || st == libvlc_Buffering || st == libvlc_Opening) {
        pause();
    } else if (st == libvlc_Paused) {
        resume();
    } else if (!m_url.isEmpty()) {
        play();
    }
}

void VLCPlayer::stop() {
    if (m_mediaPlayer) {
        libvlc_media_player_stop(m_mediaPlayer);
    }
    m_pollTimer->stop();
    m_state = "Stopped";
    m_isBuffering = false;
    m_timeMs = 0;
    m_position = 0.0;
    emit stateChanged();
    emit bufferingChanged();
    emit timeChanged();
    emit positionChanged();
}

bool VLCPlayer::seekGuardActive() const {
    return m_uptime.isValid() && m_uptime.elapsed() < m_seekGuardUntil;
}

void VLCPlayer::seek(qreal pos) {
    if (!m_mediaPlayer) return;
    pos = std::clamp(pos, 0.0, 1.0);

    m_pendingIsTime = false;
    m_pendingPos = pos;
    m_hasPendingSeek = true;

    // Optimistic UI update: the slider and clock move instantly while the actual
    // (network) seek is coalesced and issued a moment later.
    m_position = pos;
    if (m_durationMs > 0) {
        m_timeMs = static_cast<qint64>(pos * static_cast<qreal>(m_durationMs));
    }
    m_seekGuardUntil = m_uptime.elapsed() + 900;
    emit positionChanged();
    emit timeChanged();

    if (m_seekTimer && !m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}

void VLCPlayer::seekRelative(qint64 deltaMs) {
    if (!m_mediaPlayer) return;

    // m_timeMs already includes any seek that is still pending, so repeated presses
    // accumulate (e.g. 6 presses of +10s become a single +60s seek).
    qint64 newTime = std::max<qint64>(0, m_timeMs + deltaMs);
    if (m_durationMs > 1000) {
        newTime = std::min(newTime, m_durationMs - 1000);   // never seek past the very end
    }

    m_pendingIsTime = true;
    m_pendingTimeMs = newTime;
    m_hasPendingSeek = true;

    m_timeMs = newTime;
    if (m_durationMs > 0) {
        m_position = static_cast<qreal>(newTime) / static_cast<qreal>(m_durationMs);
    }
    m_seekGuardUntil = m_uptime.elapsed() + 900;
    emit timeChanged();
    emit positionChanged();

    if (m_seekTimer && !m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}

void VLCPlayer::applyPendingSeek() {
    if (!m_hasPendingSeek || !m_mediaPlayer) {
        m_hasPendingSeek = false;
        return;
    }
    m_hasPendingSeek = false;

    if (m_pendingIsTime) {
        FLUX_LOG_INFO("VLCPlayer", QString("Seeking to %1 ms").arg(m_pendingTimeMs));
        libvlc_media_player_set_time(m_mediaPlayer, m_pendingTimeMs);
    } else {
        FLUX_LOG_INFO("VLCPlayer", QString("Seeking to position: %1%").arg(QString::number(m_pendingPos * 100.0, 'f', 1)));
        libvlc_media_player_set_position(m_mediaPlayer, static_cast<float>(m_pendingPos));
    }

    // Give libVLC time to settle before trusting its reported position again
    m_seekGuardUntil = m_uptime.elapsed() + 700;
}

bool VLCPlayer::isPlaying() const {
    return m_mediaPlayer && libvlc_media_player_is_playing(m_mediaPlayer);
}

bool VLCPlayer::isPaused() const {
    if (!m_mediaPlayer) return false;
    libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
    return (st == libvlc_Paused);
}

qreal VLCPlayer::position() const {
    if (!m_mediaPlayer) return 0.0;
    if (seekGuardActive()) return m_position;   // our own value is newer than libVLC's
    float p = libvlc_media_player_get_position(m_mediaPlayer);
    return (p >= 0.0f) ? static_cast<qreal>(p) : 0.0;
}

void VLCPlayer::setPosition(qreal pos) {
    seek(pos);
}

int VLCPlayer::volume() const {
    return m_volume;
}

void VLCPlayer::setVolume(int vol) {
    // libVLC supports software amplification up to 200%
    vol = std::clamp(vol, 0, 200);
    m_volume = vol;
    if (m_mediaPlayer) {
        libvlc_audio_set_volume(m_mediaPlayer, vol);
    }
    emit volumeChanged();
}

bool VLCPlayer::isMuted() const {
    return m_muted;
}

void VLCPlayer::setMuted(bool mute) {
    m_muted = mute;
    if (m_mediaPlayer) {
        libvlc_audio_set_mute(m_mediaPlayer, mute ? 1 : 0);
    }
    emit muteChanged();
}

void VLCPlayer::notifyVideoSize(int w, int h) {
    if (m_videoWidth != w || m_videoHeight != h) {
        m_videoWidth = w;
        m_videoHeight = h;
        emit videoSizeChanged();
    }
}

void VLCPlayer::updateTracks() {
    if (!m_mediaPlayer) return;

    // 1. Audio Tracks
    QVariantList audioList;
    libvlc_track_description_t *audioDesc = libvlc_audio_get_track_description(m_mediaPlayer);
    libvlc_track_description_t *cur = audioDesc;
    while (cur) {
        QVariantMap track;
        track["id"] = cur->i_id;
        track["name"] = cur->psz_name ? QString::fromUtf8(cur->psz_name) : QString("Track %1").arg(cur->i_id);
        audioList.append(track);
        cur = cur->p_next;
    }
    if (audioDesc) {
        libvlc_track_description_list_release(audioDesc);
    }
    m_audioTracks = audioList;
    emit audioTracksChanged();

    // 2. Subtitle (SPU) Tracks
    QVariantList subList;
    libvlc_track_description_t *spuDesc = libvlc_video_get_spu_description(m_mediaPlayer);
    cur = spuDesc;
    while (cur) {
        QVariantMap track;
        track["id"] = cur->i_id;
        track["name"] = cur->psz_name ? QString::fromUtf8(cur->psz_name) : QString("Subtitle %1").arg(cur->i_id);
        subList.append(track);
        cur = cur->p_next;
    }
    if (spuDesc) {
        libvlc_track_description_list_release(spuDesc);
    }
    m_subtitleTracks = subList;
    emit subtitleTracksChanged();

    // 3. Current selections. libVLC reports these live, but QML only re-reads a
    // property when its NOTIFY signal fires, so announce them whenever the track
    // lists are refreshed (otherwise the UI keeps the stale initial value).
    emit selectedAudioTrackChanged();
    emit selectedSubtitleTrackChanged();

    // 4. Auto-pick preferred audio / subtitle language once tracks are known
    applyLanguagePreferences();
}

void VLCPlayer::playFrom(const QString &url, qint64 startMs) {
    m_startTimeMs = std::max<qint64>(0, startMs);
    play(url);
}

void VLCPlayer::setLanguagePreferences(const QStringList &audio, const QString &subtitle) {
    m_preferredAudio.clear();
    for (const QString &a : audio) {
        if (!a.trimmed().isEmpty()) m_preferredAudio.append(a.trimmed());
    }
    m_preferredSubtitle = subtitle.trimmed();
}

QString VLCPlayer::languageKey(const QString &trackName) {
    // libVLC names look like "Track 1 - [English]"; the bracket text is the language.
    static const QRegularExpression bracket(QStringLiteral("\\[([^\\]]+)\\]"));
    const QRegularExpressionMatch m = bracket.match(trackName);
    if (m.hasMatch()) return m.captured(1).trimmed();

    // A bare "Track 2" carries no language information: useless as a preference
    static const QRegularExpression generic(QStringLiteral("^\\s*(Track|Subtitle|Audio)\\s*\\d+\\s*$"),
                                            QRegularExpression::CaseInsensitiveOption);
    if (generic.match(trackName).hasMatch()) return QString();
    return trackName.trimmed();
}

void VLCPlayer::applyLanguagePreferences() {
    if (!m_mediaPlayer || m_state != "Playing") return;
    if (m_audioPrefApplied && m_subPrefApplied) return;
    ++m_autoTrackAttempts;

    // libVLC lists a "Disable" entry with id -1; only real tracks are candidates.
    auto realTracks = [](const QVariantList &list) {
        QVariantList out;
        for (const QVariant &v : list) {
            if (v.toMap().value("id").toInt() >= 0) out.append(v);
        }
        return out;
    };

    // ---- Audio ----
    if (!m_audioPrefApplied) {
        const QVariantList real = realTracks(m_audioTracks);
        if (!real.isEmpty()) {
            m_audioPrefApplied = true;
            if (real.size() > 1) {
                const int current = libvlc_audio_get_track(m_mediaPlayer);
                bool done = false;
                for (const QString &pref : m_preferredAudio) {
                    for (const QVariant &v : real) {
                        const QVariantMap t = v.toMap();
                        if (t.value("name").toString().contains(pref, Qt::CaseInsensitive)) {
                            const int id = t.value("id").toInt();
                            if (id != current) {
                                FLUX_LOG_INFO("VLCPlayer", QString("Auto-selecting preferred audio '%1' (track %2)").arg(pref).arg(id));
                                libvlc_audio_set_track(m_mediaPlayer, id);
                                emit selectedAudioTrackChanged();
                            }
                            done = true;
                            break;
                        }
                    }
                    if (done) break;
                }
            }
        }
    }

    // ---- Subtitles ----
    if (!m_subPrefApplied) {
        const QString pref = m_preferredSubtitle.trimmed();
        if (pref.isEmpty()) {
            m_subPrefApplied = true;
        } else if (pref.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0) {
            m_subPrefApplied = true;
            if (libvlc_video_get_spu(m_mediaPlayer) != -1) {
                libvlc_video_set_spu(m_mediaPlayer, -1);
                emit selectedSubtitleTrackChanged();
            }
        } else {
            const QVariantList real = realTracks(m_subtitleTracks);
            if (!real.isEmpty()) {
                m_subPrefApplied = true;
                for (const QVariant &v : real) {
                    const QVariantMap t = v.toMap();
                    if (t.value("name").toString().contains(pref, Qt::CaseInsensitive)) {
                        const int id = t.value("id").toInt();
                        if (id != libvlc_video_get_spu(m_mediaPlayer)) {
                            FLUX_LOG_INFO("VLCPlayer", QString("Auto-selecting preferred subtitle '%1' (track %2)").arg(pref).arg(id));
                            libvlc_video_set_spu(m_mediaPlayer, id);
                            emit selectedSubtitleTrackChanged();
                        }
                        break;
                    }
                }
            } else if (m_autoTrackAttempts > 8) {
                m_subPrefApplied = true;   // stream has no subtitles; stop trying
            }
        }
    }
}

void VLCPlayer::learnAudioChoice(int trackId) {
    QString name;
    for (const QVariant &v : m_audioTracks) {
        const QVariantMap t = v.toMap();
        if (t.value("id").toInt() == trackId) { name = t.value("name").toString(); break; }
    }
    const QString key = languageKey(name);
    if (key.isEmpty() || trackId < 0) return;

    QStringList updated;
    updated.append(key);
    for (const QString &p : m_preferredAudio) {
        if (p.compare(key, Qt::CaseInsensitive) != 0) updated.append(p);
    }
    while (updated.size() > 4) updated.removeLast();

    if (updated != m_preferredAudio) {
        m_preferredAudio = updated;
        FLUX_LOG_INFO("VLCPlayer", QString("Learned audio preference: %1").arg(m_preferredAudio.join(", ")));
        emit languagePreferencesChanged();
    }
}

void VLCPlayer::learnSubtitleChoice(int spuId) {
    QString key;
    if (spuId < 0) {
        key = QStringLiteral("off");
    } else {
        QString name;
        for (const QVariant &v : m_subtitleTracks) {
            const QVariantMap t = v.toMap();
            if (t.value("id").toInt() == spuId) { name = t.value("name").toString(); break; }
        }
        key = languageKey(name);
    }
    if (key.isEmpty()) return;

    if (key != m_preferredSubtitle) {
        m_preferredSubtitle = key;
        FLUX_LOG_INFO("VLCPlayer", QString("Learned subtitle preference: %1").arg(m_preferredSubtitle));
        emit languagePreferencesChanged();
    }
}

void VLCPlayer::refreshTracks() {
    updateTracks();
}

int VLCPlayer::selectedAudioTrack() const {
    if (!m_mediaPlayer) return -1;
    return libvlc_audio_get_track(m_mediaPlayer);
}

void VLCPlayer::selectAudioTrack(int trackId) {
    if (!m_mediaPlayer) return;
    FLUX_LOG_INFO("VLCPlayer", QString("Switching audio track to ID %1").arg(trackId));
    libvlc_audio_set_track(m_mediaPlayer, trackId);
    emit selectedAudioTrackChanged();
    learnAudioChoice(trackId);   // remember this language for future streams
}

int VLCPlayer::selectedSubtitleTrack() const {
    if (!m_mediaPlayer) return -1;
    return libvlc_video_get_spu(m_mediaPlayer);
}

void VLCPlayer::selectSubtitleTrack(int spuId) {
    if (!m_mediaPlayer) return;
    FLUX_LOG_INFO("VLCPlayer", QString("Switching subtitle track to ID %1").arg(spuId));
    libvlc_video_set_spu(m_mediaPlayer, spuId);
    emit selectedSubtitleTrackChanged();
    learnSubtitleChoice(spuId);  // remember this choice for future streams
}

QString VLCPlayer::formattedTime() const {
    return formatMilliseconds(m_timeMs);
}

QString VLCPlayer::formattedDuration() const {
    return formatMilliseconds(m_durationMs);
}

QString VLCPlayer::formatMilliseconds(qint64 ms) {
    if (ms <= 0) return "00:00";
    qint64 totalSec = ms / 1000;
    qint64 hours = totalSec / 3600;
    qint64 minutes = (totalSec % 3600) / 60;
    qint64 seconds = totalSec % 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
    return QString("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

} // namespace Flux
