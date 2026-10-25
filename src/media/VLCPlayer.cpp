#include "VLCPlayer.h"
#include <QSettings>
#include <QSet>
#include "VLCInstance.h"
#include "../core/Logger.h"
#include <QVariantMap>
#include <QRegularExpression>
#include <QHash>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef min
#undef max
#endif


extern "C" {
    typedef union {
        int64_t i_int;
        bool b_bool;
        float f_float;
        char *psz_string;
        void *p_address;
    } vlc_value_t;

    __declspec(dllimport) void config_PutInt(void *p_this, const char *psz_name, int64_t i_val);
    __declspec(dllimport) int var_Create(void *p_this, const char *psz_name, int i_type);
    __declspec(dllimport) int var_SetChecked(void *p_this, const char *psz_name, int expected_type, vlc_value_t val);
    __declspec(dllimport) void *libvlc_get_input_thread(libvlc_media_player_t *p_mi);
    __declspec(dllimport) void vlc_object_release(void *p_obj);
}

namespace Flux {

// ============================================================================
// Track-name cleaning
// Release groups label tracks like "<www.SomeSite.com>English". The UI should only show
// the language ("English", "Hindi", "Spanish"...). If no language can be recognised the
// cleaned title is shown, and if that is empty too, the original name is kept.
// ============================================================================
namespace {

// Full language names (and unambiguous 3-letter codes) -> canonical display name
const QHash<QString, QString> &languageAliases() {
    static const QHash<QString, QString> aliases = []() {
        QHash<QString, QString> m;
        auto add = [&m](const QString &canonical, const QStringList &names) {
            m.insert(canonical.toLower(), canonical);
            for (const QString &n : names) m.insert(n.toLower(), canonical);
        };
        add("English",    {"eng"});
        add("Hindi",      {"hin"});
        add("Bengali",    {"bangla"});
        add("Urdu",       {"urd"});
        add("Tamil",      {"tam"});
        add("Telugu",     {"tel"});
        add("Malayalam",  {"mal"});
        add("Kannada",    {"kan"});
        add("Marathi",    {});
        add("Punjabi",    {"panjabi"});
        add("Gujarati",   {"guj"});
        add("Nepali",     {"nep"});
        add("Sinhala",    {"sinhalese"});
        add("Spanish",    {"espanol", QString::fromUtf8("espa\xC3\xB1ol"), "castilian", "spa"});
        add("French",     {"francais", QString::fromUtf8("fran\xC3\xA7" "ais"), "fre", "fra"});
        add("German",     {"deutsch", "ger", "deu"});
        add("Italian",    {"italiano", "ita"});
        add("Portuguese", {"portugues", QString::fromUtf8("portugu\xC3\xAA" "s")});
        add("Russian",    {"rus"});
        add("Japanese",   {"jpn"});
        add("Korean",     {"kor"});
        add("Chinese",    {"mandarin", "cantonese", "chi", "zho"});
        add("Arabic",     {"ara"});
        add("Turkish",    {"tur"});
        add("Thai",       {});
        add("Vietnamese", {"vie"});
        add("Indonesian", {});
        add("Malay",      {});
        add("Persian",    {"farsi"});
        add("Hebrew",     {"heb"});
        add("Greek",      {});
        add("Dutch",      {"nld", "dut"});
        add("Swedish",    {"swe"});
        add("Norwegian",  {});
        add("Danish",     {});
        add("Finnish",    {});
        add("Polish",     {"pol"});
        add("Czech",      {"ces", "cze"});
        add("Hungarian",  {"hun"});
        add("Romanian",   {"ron", "rum"});
        add("Ukrainian",  {"ukr"});
        add("Filipino",   {"tagalog"});
        return m;
    }();
    return aliases;
}

// 2-letter ISO codes are only trusted when they are the WHOLE name ("en"), never inside
// a sentence ("it", "no", "in" would be false positives).
const QHash<QString, QString> &twoLetterCodes() {
    static const QHash<QString, QString> codes = {
        {"en", "English"}, {"hi", "Hindi"}, {"es", "Spanish"}, {"fr", "French"},
        {"de", "German"}, {"it", "Italian"}, {"pt", "Portuguese"}, {"ru", "Russian"},
        {"ja", "Japanese"}, {"ko", "Korean"}, {"zh", "Chinese"}, {"ar", "Arabic"},
        {"tr", "Turkish"}, {"bn", "Bengali"}, {"ta", "Tamil"}, {"te", "Telugu"},
        {"ur", "Urdu"}, {"ml", "Malayalam"}, {"kn", "Kannada"}, {"mr", "Marathi"},
        {"nl", "Dutch"}, {"sv", "Swedish"}, {"pl", "Polish"}, {"th", "Thai"},
        {"vi", "Vietnamese"}, {"id", "Indonesian"}, {"fa", "Persian"}, {"he", "Hebrew"},
        {"el", "Greek"}, {"uk", "Ukrainian"}, {"cs", "Czech"}, {"hu", "Hungarian"},
        {"ro", "Romanian"}
    };
    return codes;
}

QString detectLanguage(const QString &text) {
    const QString t = text.simplified();
    if (t.isEmpty()) return QString();

    if (t.size() == 2) {
        const auto it = twoLetterCodes().constFind(t.toLower());
        if (it != twoLetterCodes().constEnd()) return it.value();
    }

    static const QRegularExpression splitter(QStringLiteral("[^\\p{L}]+"));
    const QStringList tokens = t.toLower().split(splitter, Qt::SkipEmptyParts);
    const auto &aliases = languageAliases();
    for (const QString &tok : tokens) {
        const auto it = aliases.constFind(tok);
        if (it != aliases.constEnd()) return it.value();
    }
    return QString();
}

struct CleanedTrack {
    QString display;   // what the user sees
    QString lang;      // canonical language, or empty if not recognised
};

CleanedTrack cleanTrackName(const QString &raw) {
    CleanedTrack out;
    out.display = raw;   // fallback: the original name

    static const QRegularExpression bracketRe(QStringLiteral("\\[([^\\]]*)\\]"));
    static const QRegularExpression angleRe(QStringLiteral("<[^>]*>"));
    static const QRegularExpression urlRe(QStringLiteral("(https?://\\S+|www\\.\\S+)"),
                                          QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression domainRe(
        QStringLiteral("\\b[\\w-]+(\\.[\\w-]+)*\\.(com|net|org|info|co|me|tv|to|ws|cc|xyz|pk|bd|io|club|site|online|top|vip|biz|in)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression genericRe(QStringLiteral("\\b(Track|Subtitle|Audio|Stream)\\s*\\d+\\b"),
                                              QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression camelRe(QStringLiteral("([a-z])([A-Z])"));
    static const QRegularExpression edgeRe(
        QStringLiteral("^[\\s\\-\\x{2013}\\x{2014}_:.|,/\\\\]+|[\\s\\-\\x{2013}\\x{2014}_:.|,/\\\\]+$"));

    QString s = raw;

    // The language libVLC appended itself: "... - [English]"
    QString bracketLang;
    QRegularExpressionMatchIterator bi = bracketRe.globalMatch(s);
    while (bi.hasNext()) bracketLang = bi.next().captured(1).trimmed();
    s.remove(bracketRe);

    // Site / release-group junk
    s.remove(angleRe);
    s.remove(urlRe);
    s.remove(domainRe);
    s.remove(genericRe);
    s.replace(camelRe, QStringLiteral("\\1 \\2"));   // "FooEnglish" -> "Foo English"
    s.remove(edgeRe);
    s = s.simplified();

    // Recognise the language: the cleaned title first (uploaders label tracks by hand),
    // then libVLC's own bracket language, then a looser pass over the raw text.
    QString lang = detectLanguage(s);
    if (lang.isEmpty()) lang = detectLanguage(bracketLang);
    if (lang.isEmpty()) {
        QString loose = raw;
        loose.remove(bracketRe);
        loose.replace(camelRe, QStringLiteral("\\1 \\2"));
        lang = detectLanguage(loose);
    }
    if (lang.isEmpty() && !bracketLang.isEmpty()) {
        const QString bl = bracketLang.toLower();
        if (bl != QLatin1String("undetermined") && bl != QLatin1String("unknown") && bl != QLatin1String("und")) {
            lang = bracketLang.left(1).toUpper() + bracketLang.mid(1);   // a language we have no alias for
        }
    }

    // Useful qualifiers worth keeping next to the language
    QStringList extras;
    static const QRegularExpression chRe(QStringLiteral("(?<![\\d.])(5\\.1|7\\.1)(?![\\d.])"));
    const QRegularExpressionMatch chm = chRe.match(s);
    if (chm.hasMatch()) extras << chm.captured(1);

    static const QRegularExpression tagRe(QStringLiteral("\\b(commentary|forced|sdh|cc|signs & songs|signs and songs|signs|songs)\\b"),
                                          QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatchIterator ti = tagRe.globalMatch(s);
    while (ti.hasNext()) {
        const QString tag = ti.next().captured(1).toLower();
        QString shown;
        if (tag == QLatin1String("sdh") || tag == QLatin1String("cc")) shown = tag.toUpper();
        else if (tag.contains(QLatin1String("sign")) && tag.contains(QLatin1String("song"))) shown = QStringLiteral("Signs & Songs");
        else shown = tag.left(1).toUpper() + tag.mid(1);
        if (!extras.contains(shown)) extras << shown;
    }

    if (!lang.isEmpty()) {
        out.lang = lang;
        out.display = lang;
        if (!extras.isEmpty()) {
            out.display += QString(" ") + QChar(0x00B7) + QString(" ") + extras.join(QLatin1Char(' '));
        }
    } else if (!s.isEmpty()) {
        out.display = s;      // no language recognised: show the cleaned title
    }                         // else: nothing usable left, keep the original name

    return out;
}

QVariantMap makeTrack(int id, const QString &rawName) {
    QVariantMap track;
    track["id"] = id;
    track["rawName"] = rawName;
    if (id < 0) {                       // libVLC's own "Disable" entry: keep verbatim
        track["name"] = rawName;
        track["lang"] = QString();
        return track;
    }
    const CleanedTrack c = cleanTrackName(rawName);
    track["name"] = c.display;
    track["lang"] = c.lang;
    return track;
}

// Two tracks that clean to the same name become "English" and "English (2)"
void disambiguateTracks(QVariantList &list) {
    QHash<QString, int> total;
    for (const QVariant &v : list) {
        const QVariantMap t = v.toMap();
        if (t.value("id").toInt() >= 0) total[t.value("name").toString()]++;
    }

    QHash<QString, int> seen;
    for (int i = 0; i < list.size(); ++i) {
        QVariantMap t = list.at(i).toMap();
        if (t.value("id").toInt() < 0) continue;
        const QString n = t.value("name").toString();
        if (total.value(n) > 1) {
            const int k = ++seen[n];
            if (k > 1) {
                t["name"] = n + QStringLiteral(" (") + QString::number(k) + QStringLiteral(")");
                list[i] = t;
            }
        }
    }
}

bool trackMatches(const QVariantMap &t, const QString &pref) {
    if (pref.trimmed().isEmpty()) return false;
    const QString tLang = t.value("lang").toString();
    const QString tName = t.value("name").toString();
    const QString tRaw = t.value("rawName").toString();

    if (tLang.compare(pref, Qt::CaseInsensitive) == 0
        || tName.compare(pref, Qt::CaseInsensitive) == 0
        || tName.contains(pref, Qt::CaseInsensitive)
        || tRaw.contains(pref, Qt::CaseInsensitive)) {
        return true;
    }

    const QString prefLang = detectLanguage(pref);
    if (!prefLang.isEmpty()) {
        if (!tLang.isEmpty() && tLang.compare(prefLang, Qt::CaseInsensitive) == 0) {
            return true;
        }
        if (detectLanguage(tName).compare(prefLang, Qt::CaseInsensitive) == 0 ||
            detectLanguage(tRaw).compare(prefLang, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

// Commentary / forced / SDH variants are a worse default than the plain track
int trackPenalty(const QVariantMap &t) {
    const QString n = (t.value("name").toString() + QLatin1Char(' ') + t.value("rawName").toString()).toLower();
    if (n.contains(QLatin1String("sign")) || n.contains(QLatin1String("song"))) {
        return 2;
    }
    if (n.contains(QLatin1String("commentary")) || n.contains(QLatin1String("forced"))
        || n.contains(QLatin1String("sdh")) || n.contains(QLatin1String("descript"))) {
        return 1;
    }
    return 0;
}

// Best track for a preferred language (empty map if none matches)
QVariantMap bestTrackFor(const QVariantList &tracks, const QString &pref) {
    QVariantMap best;
    int bestPenalty = 1000;
    for (const QVariant &v : tracks) {
        const QVariantMap t = v.toMap();
        if (!trackMatches(t, pref)) continue;
        const int p = trackPenalty(t);
        if (p < bestPenalty) {
            best = t;
            bestPenalty = p;
        }
    }
    return best;
}

} // namespace

libvlc_media_player_t* VLCPlayer::vlcMediaPlayer() {
    ensureMediaPlayer();
    return m_mediaPlayer;
}

bool VLCPlayer::ensureMediaPlayer() {
    if (m_mediaPlayer) {
        return true;
    }

    auto *vlc = VLCInstance::instance().get();
    if (!vlc) {
        return false;
    }

    m_mediaPlayer = libvlc_media_player_new(vlc);
    if (!m_mediaPlayer) {
        FLUX_LOG_ERROR("VLCPlayer", "Failed to create libVLC media player!");
        return false;
    }

    setupVlcEvents();

    if (m_volume >= 0) {
        libvlc_audio_set_volume(m_mediaPlayer, m_volume);
    }
    if (m_muted) {
        libvlc_audio_set_mute(m_mediaPlayer, 1);
    }

    emit mediaPlayerRecreated();
    return true;
}

VLCPlayer::VLCPlayer(QObject *parent)
    : QObject(parent) {

    // Fallback polling timer (250ms) to ensure UI timeline stays smooth even if VLC event frequency varies
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(250);
    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        // Seek mute: lift it as soon as playback has actually landed near the seek target
        // (checked before the isPlaying() bail-out because libVLC reports Buffering, not
        // Playing, while the new range request is in flight).
        if (m_seekMuted && m_mediaPlayer && !m_hasPendingSeek) {
            const libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
            if (st == libvlc_Paused || st == libvlc_Stopped || st == libvlc_Ended || st == libvlc_Error) {
                endSeekMute();
            } else if (st == libvlc_Playing && !m_isBuffering) {
                const qint64 t = libvlc_media_player_get_time(m_mediaPlayer);
                const bool landed = (m_seekMuteTargetMs >= 0)
                    ? (t >= 0 && std::llabs(t - m_seekMuteTargetMs) <= 3000)
                    : !seekGuardActive();
                if (landed) endSeekMute();
            }
        }

        if (!m_mediaPlayer || !isPlaying()) return;

        // While a seek is pending/settling, libVLC keeps reporting the OLD position for a
        // moment. Ignoring it stops the slider and clock jumping back and forth (the
        // brief "freeze" feel after seeking).
        const qint64 t = libvlc_media_player_get_time(m_mediaPlayer);
        const float reportedPosition = libvlc_media_player_get_position(m_mediaPlayer);
        if (m_resumePositionPending) {
            const qreal positionTolerance = m_durationMs > 0
                    ? 750.0 / static_cast<qreal>(m_durationMs) : 0.0001;
            const qreal minimumPosition = std::max<qreal>(0.0, m_position - positionTolerance);
            const bool timeReached = t >= 0 && t >= m_resumeTargetMs - 750;
            const bool positionReached = reportedPosition >= 0.0f
                    && static_cast<qreal>(reportedPosition) >= minimumPosition;
            const bool guardExpired = m_uptime.elapsed() >= m_resumeGuardDeadline;
            if ((timeReached && positionReached) || (guardExpired && t >= 0)) {
                m_resumePositionPending = false;
                m_seekGuardUntil = 0;
                if (t >= 0) {
                    m_timeMs = t;
                    if (m_durationMs > 0) {
                        m_position = std::clamp(static_cast<qreal>(t) / m_durationMs, 0.0, 1.0);
                    }
                    emit timeChanged();
                    emit positionChanged();
                }
            }
        }

        if (!m_resumePositionPending && !seekGuardActive()) {
            if (t >= 0 && t != m_timeMs) {
                m_timeMs = t;
                emit timeChanged();
            }

            const float p = reportedPosition;
            // 0.001 is several seconds on a feature-length movie, which makes
            // the progress bar visibly trail playback after a resume.
            if (p >= 0.0f && std::abs(p - static_cast<float>(m_position)) > 0.00005f) {
                m_position = p;
                emit positionChanged();
            }
        }

        qint64 len = libvlc_media_player_get_length(m_mediaPlayer);
        if (len > 0 && len != m_durationMs) {
            m_durationMs = len;
            emit durationChanged();
            if (m_timeMs > 0) {
                m_position = std::clamp(static_cast<qreal>(m_timeMs) / static_cast<qreal>(m_durationMs), 0.0, 1.0);
                emit positionChanged();
            }
            updateTracks();
        }

        // Periodically refresh display sleep prevention while actively playing
        static int sleepHeartbeatTicks = 0;
        if (++sleepHeartbeatTicks % 120 == 0) { // every 30s (120 * 250ms)
            updateSleepInhibition(true);
        }
    });

    // Automatically prevent display sleep during video playback, restore on pause/stop
    connect(this, &VLCPlayer::stateChanged, this, [this]() {
        updateSleepInhibition(false);
    });

    // Seek coalescing timer: at most one libVLC seek every 120ms, always with the latest target
    m_uptime.start();
    m_seekTimer = new QTimer(this);
    m_seekTimer->setSingleShot(true);
    m_seekTimer->setInterval(120);
    connect(m_seekTimer, &QTimer::timeout, this, [this]() { applyPendingSeek(); });

    // Failsafe: never stay seek-muted for more than a few seconds, whatever happens
    m_seekMuteTimer = new QTimer(this);
    m_seekMuteTimer->setSingleShot(true);
    m_seekMuteTimer->setInterval(6000);
    connect(m_seekMuteTimer, &QTimer::timeout, this, [this]() { endSeekMute(); });

    if (VLCInstance::instance().isInitialized()) {
        ensureMediaPlayer();
    }

    FLUX_LOG_INFO("VLCPlayer", "VLCPlayer created successfully");
}

VLCPlayer::~VLCPlayer() {
    stop();
    detachVlcEvents();

    if (m_displaySleepInhibited) {
#ifdef Q_OS_WIN
        SetThreadExecutionState(ES_CONTINUOUS);
#endif
        m_displaySleepInhibited = false;
    }

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
            player->applyMute();
            emit player->stateChanged();
            emit player->bufferingChanged();
            player->updateTracks();
            FLUX_LOG_INFO("VLCPlayer", "Playback started");
        }, Qt::QueuedConnection);
        break;

    case libvlc_MediaPlayerPaused:
        QMetaObject::invokeMethod(player, [player]() {
            player->m_state = "Paused";
            player->endSeekMute();   // pause is now real; safe to restore audio for resume
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
            if (player->m_timeMs > 0 && len > 0) {
                player->m_position = std::clamp(static_cast<qreal>(player->m_timeMs) / len, 0.0, 1.0);
                emit player->positionChanged();
            }
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
    // Stop the previous source before changing m_url so progress tracking can still
    // associate the player's final clock position with the media that is ending.
    const bool stoppedBeforeUrlChange = !m_simulatedMode
            && !mediaUrl.isEmpty() && mediaUrl != m_url;
    if (stoppedBeforeUrlChange) stop();

    if (!mediaUrl.isEmpty()) {
        setUrl(mediaUrl);
    }

    if (m_url.isEmpty()) {
        FLUX_LOG_WARN("VLCPlayer", "play() requested but URL is empty");
        return;
    }

    if (std::abs(m_playbackRate - 1.0) > 0.0005) {
        m_playbackRate = 1.0;
        emit playbackRateChanged();
    }

    if (m_simulatedMode) {
        const qint64 startMs = m_startTimeMs;
        const qreal startPosition = m_startPosition;
        m_startTimeMs = 0;
        m_startPosition = -1.0;
        m_hasPendingSeek = false;
        m_simulatedSubMsRemainder = 0.0;
        m_timeMs = std::max<qint64>(0, startMs);
        if (m_durationMs <= 0) m_durationMs = 7200000;
        m_position = startPosition >= 0.0
                ? startPosition
                : static_cast<qreal>(m_timeMs) / static_cast<qreal>(m_durationMs);
        m_simulatedSeekGuardUntilMs = (startMs >= 1000) ? (m_simulatedClockMs + 700) : 0;
        m_state = QStringLiteral("Opening");
        m_isBuffering = true;
        m_bufferingPercent = 0.0f;
        m_errorMessage.clear();
        emit timeChanged();
        emit positionChanged();
        emit stateChanged();
        emit bufferingChanged();
        return;
    }

    ensureMediaPlayer();
    auto *vlc = VLCInstance::instance().get();
    if (!vlc || !m_mediaPlayer) {
        FLUX_LOG_ERROR("VLCPlayer", "Cannot play: libVLC not initialized");
        return;
    }

    FLUX_LOG_INFO("VLCPlayer", QString("Starting direct stream: %1").arg(m_url));

    if (!stoppedBeforeUrlChange) stop();

    if (m_currentMedia) {
        libvlc_media_release(m_currentMedia);
        m_currentMedia = nullptr;
    }

    // Direct HTTP Range streaming from URL
    QString playLocation = m_url;
    if (playLocation.contains(QLatin1String("index.circleftp.net"))) {
        playLocation.replace(QLatin1String("index.circleftp.net"), QLatin1String("15.1.4.2"));
    }
    m_currentMedia = libvlc_media_new_location(vlc, playLocation.toUtf8().constData());
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
    libvlc_media_add_option(m_currentMedia, ":network-caching=600");
    libvlc_media_add_option(m_currentMedia, ":http-reconnect");
    libvlc_media_add_option(m_currentMedia, ":prefetch-buffer-size=131072");
    libvlc_media_add_option(m_currentMedia, ":prefetch-read-size=262144");

    const QByteArray subScaleOpt = QString(":sub-text-scale=%1").arg(m_subtitleScale).toUtf8();

    libvlc_media_add_option(m_currentMedia, subScaleOpt.constData());

    // Resume: begin directly at the saved position instead of starting at 0 and seeking
    // (avoids a wasted buffer at the start of the file). One-shot.
    const qint64 startMs = m_startTimeMs;
    const qreal startPosition = m_startPosition;
    m_startTimeMs = 0;
    m_startPosition = -1.0;
    if (startMs >= 1000) {
        const QByteArray startOpt = QString(":start-time=%1").arg(static_cast<double>(startMs) / 1000.0, 0, 'f', 2).toUtf8();
        libvlc_media_add_option(m_currentMedia, startOpt.constData());
        FLUX_LOG_INFO("VLCPlayer", QString("Resuming from %1 ms").arg(startMs));
    }

    libvlc_media_player_set_media(m_mediaPlayer, m_currentMedia);
    libvlc_media_player_set_rate(m_mediaPlayer, 1.0f);

    // A new stream starts fresh: drop any seek still queued for the previous one
    m_hasPendingSeek = false;
    m_seekGuardUntil = 0;
    if (m_seekTimer) m_seekTimer->stop();

    // New stream: language auto-selection starts over
    m_audioPrefApplied = false;
    m_subPrefApplied = false;
    m_autoTrackAttempts = 0;

    // Show the resume position immediately instead of flashing 00:00
    if (startMs >= 1000) {
        m_timeMs = startMs;
        if (startPosition >= 0.0) {
            m_position = std::clamp(startPosition, 0.0, 1.0);
        } else if (m_durationMs > 0) {
            m_position = std::clamp(static_cast<qreal>(startMs) / static_cast<qreal>(m_durationMs), 0.0, 1.0);
        }
        emit timeChanged();
        emit positionChanged();
        m_resumeTargetMs = startMs;
        m_resumeGuardDeadline = m_uptime.elapsed() + 15000;
        m_resumePositionPending = true;
        m_seekGuardUntil = m_uptime.elapsed() + 2000;
    } else {
        m_resumePositionPending = false;
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
    if (std::abs(m_playbackRate - 1.0) > 0.0005) {
        remoteSetRate(1.0);
    }
    if (m_simulatedMode) {
        if (m_state == QLatin1String("Playing") || m_state.startsWith(QLatin1String("Buffering")) || m_state == QLatin1String("Opening")) {
            m_state = QStringLiteral("Paused");
            m_isBuffering = false;
            emit stateChanged();
        }
        return;
    }
    if (!m_mediaPlayer) return;
    libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
    if (st == libvlc_Playing || st == libvlc_Buffering || st == libvlc_Opening) {
        // Silence first: libVLC's audio queue keeps draining for a moment after pause is
        // requested, which is the "lag" you hear. Muting is instant; the mute is lifted when
        // libVLC reports Paused (see the Paused event handler) so resume is unaffected.
        beginSeekMute(-1);
        libvlc_media_player_set_pause(m_mediaPlayer, 1);   // idempotent, unlike the toggle
        m_state = "Paused";
        emit stateChanged();
        FLUX_LOG_INFO("VLCPlayer", "Playback paused immediately");
    }
}

void VLCPlayer::resume() {
    if (m_simulatedMode) {
        if (m_state == QLatin1String("Paused")) {
            m_state = QStringLiteral("Playing");
            m_isBuffering = false;
            emit stateChanged();
        }
        return;
    }
    if (m_mediaPlayer && isPaused()) {
        endSeekMute();
        libvlc_media_player_set_pause(m_mediaPlayer, 0);
        m_state = "Playing";
        emit stateChanged();
    }
}

void VLCPlayer::togglePlay() {
    if (m_simulatedMode) {
        if (m_state == QLatin1String("Playing") || m_state.startsWith(QLatin1String("Buffering")) || m_state == QLatin1String("Opening")) {
            pause();
            if (m_quiet == 0) emit userToggledPlay(false, m_timeMs);
        } else if (m_state == QLatin1String("Paused")) {
            resume();
            if (m_quiet == 0) emit userToggledPlay(true, m_timeMs);
        } else if (!m_url.isEmpty()) {
            play();
            if (m_quiet == 0) emit userToggledPlay(true, 0);
        }
        return;
    }
    if (!m_mediaPlayer) return;

    // Decide from libVLC's real state. The old check (isPlaying() == false => play())
    // treated "Buffering"/"Opening" as stopped and RESTARTED the stream from the
    // beginning whenever you clicked during a buffer.
    libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
    if (st == libvlc_Playing || st == libvlc_Buffering || st == libvlc_Opening) {
        pause();
        if (m_quiet == 0) emit userToggledPlay(false, m_timeMs);
    } else if (st == libvlc_Paused) {
        resume();
        if (m_quiet == 0) emit userToggledPlay(true, m_timeMs);
    } else if (!m_url.isEmpty()) {
        play();
        if (m_quiet == 0) emit userToggledPlay(true, 0);   // restarts from the beginning
    }
}

void VLCPlayer::stop() {
    emit aboutToStop();
    m_resumePositionPending = false;
    endSeekMute();
    if (std::abs(m_playbackRate - 1.0) > 0.0005) {
        m_playbackRate = 1.0;
        emit playbackRateChanged();
    }
    if (m_simulatedMode) {
        m_state = QStringLiteral("Stopped");
        m_isBuffering = false;
        m_timeMs = 0;
        m_position = 0.0;
        m_simulatedSubMsRemainder = 0.0;
        emit stateChanged();
        emit bufferingChanged();
        emit timeChanged();
        emit positionChanged();
        return;
    }
    if (m_mediaPlayer) {
        libvlc_media_player_set_rate(m_mediaPlayer, 1.0f);
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
    if (m_simulatedMode) {
        return m_simulatedClockMs < m_simulatedSeekGuardUntilMs;
    }
    return m_uptime.isValid() && m_uptime.elapsed() < m_seekGuardUntil;
}

bool VLCPlayer::isSeekable() const {
    if (m_simulatedMode) {
        return m_simulatedSeekable;
    }
    return m_mediaPlayer && (libvlc_media_player_is_seekable(m_mediaPlayer) != 0);
}

bool VLCPlayer::isSeekSettling() const {
    return seekGuardActive() || m_hasPendingSeek || m_seekMuted;
}

void VLCPlayer::seek(qreal pos) {
    if (!m_mediaPlayer && !m_simulatedMode) return;
    m_resumePositionPending = false;
    pos = std::clamp(pos, 0.0, 1.0);

    m_pendingIsTime = false;
    m_pendingPos = pos;
    m_hasPendingSeek = true;
    m_pendingNotify = (m_quiet == 0);   // the latest request decides whether it is broadcast

    // Optimistic UI update: the slider and clock move instantly while the actual
    // (network) seek is coalesced and issued a moment later.
    m_position = pos;
    if (m_durationMs > 0) {
        m_timeMs = static_cast<qint64>(pos * static_cast<qreal>(m_durationMs));
    }
    m_seekGuardUntil = m_uptime.elapsed() + 900;
    emit positionChanged();
    emit timeChanged();

    // Silence the stale audio immediately; it is restored once playback lands on the target
    beginSeekMute(m_durationMs > 0 ? m_timeMs : -1);

    if (m_seekTimer && !m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}

void VLCPlayer::seekRelative(qint64 deltaMs) {
    if (!m_mediaPlayer && !m_simulatedMode) return;
    m_resumePositionPending = false;

    // m_timeMs already includes any seek that is still pending, so repeated presses
    // accumulate (e.g. 6 presses of +10s become a single +60s seek).
    qint64 newTime = std::max<qint64>(0, m_timeMs + deltaMs);
    if (m_durationMs > 1000) {
        newTime = std::min(newTime, m_durationMs - 1000);   // never seek past the very end
    }

    m_pendingIsTime = true;
    m_pendingTimeMs = newTime;
    m_hasPendingSeek = true;
    m_pendingNotify = (m_quiet == 0);

    m_timeMs = newTime;
    if (m_durationMs > 0) {
        m_position = static_cast<qreal>(newTime) / static_cast<qreal>(m_durationMs);
    }
    m_seekGuardUntil = m_uptime.elapsed() + 900;
    emit timeChanged();
    emit positionChanged();

    beginSeekMute(newTime);

    if (m_seekTimer && !m_seekTimer->isActive()) {
        m_seekTimer->start();
    }
}

void VLCPlayer::applyPendingSeek() {
    if (!m_hasPendingSeek || (!m_mediaPlayer && !m_simulatedMode)) {
        m_hasPendingSeek = false;
        return;
    }
    m_hasPendingSeek = false;

    // Tell Teleparty about seeks the local user made (after coalescing, so a slider drag
    // produces a handful of updates rather than one per mouse move)
    const bool notify = m_pendingNotify;
    m_pendingNotify = false;
    if (notify) {
        const qint64 target = m_pendingIsTime
            ? m_pendingTimeMs
            : static_cast<qint64>(m_pendingPos * static_cast<qreal>(m_durationMs));
        if (m_pendingIsTime || m_durationMs > 0) emit userSeeked(target);
    }

    if (m_simulatedMode) {
        m_simulatedSeekGuardUntilMs = m_simulatedClockMs + 700;
        return;
    }

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

void VLCPlayer::applyMute() {
    if (!m_mediaPlayer) return;
    libvlc_audio_set_mute(m_mediaPlayer, (m_muted || m_seekMuted) ? 1 : 0);
}

void VLCPlayer::beginSeekMute(qint64 targetMs) {
    if (!m_mediaPlayer) return;
    // Only needed while audio is actually flowing; a paused/stopped player has nothing stale
    const libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
    if (st != libvlc_Playing && st != libvlc_Buffering) return;

    m_seekMuteTargetMs = targetMs;
    if (!m_seekMuted) {
        m_seekMuted = true;
        applyMute();
    }
    if (m_seekMuteTimer) m_seekMuteTimer->start();   // (re)arm failsafe
}

void VLCPlayer::endSeekMute() {
    if (m_seekMuteTimer) m_seekMuteTimer->stop();
    m_seekMuteTargetMs = -1;
    if (!m_seekMuted) return;
    m_seekMuted = false;
    applyMute();
}

bool VLCPlayer::isPlaying() const {
    if (m_simulatedMode) {
        return m_state == QLatin1String("Playing");
    }
    return m_mediaPlayer && libvlc_media_player_is_playing(m_mediaPlayer);
}

bool VLCPlayer::isPaused() const {
    if (m_simulatedMode) {
        return m_state == QLatin1String("Paused");
    }
    if (!m_mediaPlayer) return false;
    libvlc_state_t st = libvlc_media_player_get_state(m_mediaPlayer);
    return (st == libvlc_Paused || m_state == "Paused");
}

qreal VLCPlayer::position() const {
    // The poll loop owns timeline updates. Reading libVLC directly here can
    // expose its stale pre-resume position between polls and rewind the slider.
    return m_position;
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
    emit volumeChanged();   // update the UI readout first so it never waits on libVLC
    if (m_mediaPlayer) {
        libvlc_audio_set_volume(m_mediaPlayer, vol);
    }
}

bool VLCPlayer::isMuted() const {
    return m_muted;
}

void VLCPlayer::setMuted(bool mute) {
    m_muted = mute;
    applyMute();   // combines the user's mute with any active seek mute
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
        const QString rawName = cur->psz_name ? QString::fromUtf8(cur->psz_name) : QString("Track %1").arg(cur->i_id);
        audioList.append(makeTrack(cur->i_id, rawName));
        cur = cur->p_next;
    }
    if (audioDesc) {
        libvlc_track_description_list_release(audioDesc);
    }
    disambiguateTracks(audioList);
    m_audioTracks = audioList;
    emit audioTracksChanged();

    // 2. Subtitle (SPU) Tracks
    QHash<int, QString> subTypeMap;
    if (m_currentMedia) {
        libvlc_media_track_t **tracks = nullptr;
        unsigned count = libvlc_media_tracks_get(m_currentMedia, &tracks);
        for (unsigned i = 0; i < count; ++i) {
            if (tracks[i]->i_type == libvlc_track_text) {
                uint32_t c = tracks[i]->i_codec;
                // Bitmap formats: PGS (bdpg: 0x67706462), VobSub (spu : 0x20757073), DVB (dvbs: 0x73627664), XSub (xsub: 0x62757378)
                if (c == 0x67706462 || c == 0x20757073 || c == 0x73627664 || c == 0x62757378) {
                    subTypeMap[tracks[i]->i_id] = QStringLiteral("bitmap");
                } else if (c == 0x20617373 || c == 0x20737361) {
                    // SSA (0x20617373), ASS (0x20737361)
                    subTypeMap[tracks[i]->i_id] = QStringLiteral("ass");
                } else {
                    subTypeMap[tracks[i]->i_id] = QStringLiteral("text");
                }
            }
        }
        if (tracks) {
            libvlc_media_tracks_release(tracks, count);
        }
    }

    QVariantList subList;
    libvlc_track_description_t *spuDesc = libvlc_video_get_spu_description(m_mediaPlayer);
    cur = spuDesc;
    while (cur) {
        const QString rawName = cur->psz_name ? QString::fromUtf8(cur->psz_name) : QString("Subtitle %1").arg(cur->i_id);
        QVariantMap tr = makeTrack(cur->i_id, rawName);
        const QString st = subTypeMap.value(cur->i_id, QStringLiteral("text"));
        tr["subType"] = st;
        tr["isBitmap"] = (st == QLatin1String("bitmap"));
        tr["isAss"] = (st == QLatin1String("ass"));
        subList.append(tr);
        cur = cur->p_next;
    }
    if (spuDesc) {
        libvlc_track_description_list_release(spuDesc);
    }
    disambiguateTracks(subList);
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

void VLCPlayer::playFrom(const QString &url, qint64 startMs, qreal startPosition) {
    m_startTimeMs = std::max<qint64>(0, startMs);
    m_startPosition = startPosition >= 0.0 ? std::clamp(startPosition, 0.0, 1.0) : -1.0;
    play(url);
}

// ---- Teleparty: actions applied on behalf of another member (never broadcast back) ----

void VLCPlayer::remotePause() {
    ++m_quiet;
    pause();
    --m_quiet;
}

void VLCPlayer::remoteResume() {
    ++m_quiet;
    resume();
    --m_quiet;
}

void VLCPlayer::remoteSeekTo(qint64 timeMs) {
    if (!m_mediaPlayer && !m_simulatedMode) return;
    m_resumePositionPending = false;

    qint64 newTime = std::max<qint64>(0, timeMs);
    if (m_durationMs > 1000) {
        newTime = std::min(newTime, m_durationMs - 1000);
    }

    if (m_seekTimer) m_seekTimer->stop();
    m_hasPendingSeek = false;
    m_pendingNotify = false;

    if (m_simulatedMode) {
        if (!m_simulatedSeekable) return;
        if (m_simulatedKeyframeStepMs > 0) {
            newTime = ((newTime + m_simulatedKeyframeStepMs / 2) / m_simulatedKeyframeStepMs) * m_simulatedKeyframeStepMs;
        }
        m_timeMs = newTime;
        m_simulatedSubMsRemainder = 0.0;
        if (m_durationMs > 0) {
            m_position = static_cast<qreal>(newTime) / static_cast<qreal>(m_durationMs);
        }
        m_simulatedSeekGuardUntilMs = m_simulatedClockMs + 700;
        emit timeChanged();
        emit positionChanged();
        return;
    }

    m_timeMs = newTime;
    if (m_durationMs > 0) {
        m_position = static_cast<qreal>(newTime) / static_cast<qreal>(m_durationMs);
    }
    m_seekGuardUntil = m_uptime.elapsed() + 700;
    emit timeChanged();
    emit positionChanged();

    beginSeekMute(newTime);
    FLUX_LOG_INFO("VLCPlayer", QString("Remote seeking immediately to %1 ms").arg(newTime));
    libvlc_media_player_set_time(m_mediaPlayer, newTime);
}

void VLCPlayer::remoteRestart(qint64 startMs) {
    if (m_url.isEmpty()) return;
    ++m_quiet;
    playFrom(m_url, startMs);
    --m_quiet;
}

bool VLCPlayer::remoteSetRate(double rate) {
    if (!std::isfinite(rate)) return false;
    const double clamped = std::clamp(rate, 0.5, 2.0);

    if (m_simulatedMode) {
        if (!m_simulatedRateSupported) {
            return false;
        }
        if (std::abs(m_playbackRate - clamped) > 0.0005) {
            m_playbackRate = clamped;
            emit playbackRateChanged();
        }
        return true;
    }

    if (!m_mediaPlayer) return false;
    if (std::abs(m_playbackRate - clamped) < 0.0005) {
        return true;
    }

    const int rc = libvlc_media_player_set_rate(m_mediaPlayer, static_cast<float>(clamped));
    if (rc != 0) {
        FLUX_LOG_WARN("VLCPlayer", QString("libVLC rejected playback rate %1").arg(clamped, 0, 'f', 3));
        return false;
    }

    m_playbackRate = clamped;
    emit playbackRateChanged();
    return true;
}

void VLCPlayer::setSimulatedMode(bool enabled) {
    m_simulatedMode = enabled;
    if (enabled && m_pollTimer) {
        m_pollTimer->stop();
    }
}

void VLCPlayer::setSimulatedState(const QString &state, qint64 timeMs, qint64 durationMs,
                                  bool buffering, float bufferPct) {
    m_simulatedMode = true;
    const bool stateDiff = (m_state != state);
    const bool bufDiff = (m_isBuffering != buffering || std::abs(m_bufferingPercent - bufferPct) > 0.1f);
    const bool timeDiff = (m_timeMs != timeMs);
    const bool durDiff = (m_durationMs != durationMs);

    m_state = state;
    m_isBuffering = buffering;
    m_bufferingPercent = bufferPct;
    m_timeMs = std::max<qint64>(0, timeMs);
    m_durationMs = std::max<qint64>(0, durationMs);
    m_simulatedSubMsRemainder = 0.0;
    if (m_durationMs > 0) {
        m_position = static_cast<qreal>(m_timeMs) / static_cast<qreal>(m_durationMs);
    }

    if (durDiff) emit durationChanged();
    if (timeDiff) {
        emit timeChanged();
        emit positionChanged();
    }
    if (bufDiff) emit bufferingChanged();
    if (stateDiff) emit stateChanged();
}

void VLCPlayer::advanceSimulatedTime(qint64 elapsedRealMs) {
    if (!m_simulatedMode || elapsedRealMs <= 0) return;
    m_simulatedClockMs += elapsedRealMs;

    if (m_state == QLatin1String("Playing") && !m_isBuffering) {
        const double exactDelta = static_cast<double>(elapsedRealMs) * m_playbackRate + m_simulatedSubMsRemainder;
        const qint64 wholeMs = static_cast<qint64>(std::floor(exactDelta));
        m_simulatedSubMsRemainder = exactDelta - static_cast<double>(wholeMs);
        m_timeMs += wholeMs;
        if (m_durationMs > 0 && m_timeMs >= m_durationMs) {
            m_timeMs = m_durationMs;
            m_state = QStringLiteral("Ended");
            emit timeChanged();
            emit stateChanged();
            return;
        }
        if (m_durationMs > 0) {
            m_position = static_cast<qreal>(m_timeMs) / static_cast<qreal>(m_durationMs);
        }
        emit timeChanged();
    }
}

void VLCPlayer::simulateUserTogglePlay(bool playing) {
    if (!m_simulatedMode) return;
    if (playing) {
        m_state = QStringLiteral("Playing");
        m_isBuffering = false;
    } else {
        m_state = QStringLiteral("Paused");
        m_isBuffering = false;
        m_playbackRate = 1.0;
    }
    emit stateChanged();
    if (m_quiet == 0) {
        emit userToggledPlay(playing, m_timeMs);
    }
}

void VLCPlayer::simulateUserSeek(qint64 targetMs) {
    if (!m_simulatedMode) return;
    m_timeMs = std::max<qint64>(0, targetMs);
    m_simulatedSubMsRemainder = 0.0;
    if (m_durationMs > 0) {
        m_position = static_cast<qreal>(m_timeMs) / static_cast<qreal>(m_durationMs);
    }
    m_simulatedSeekGuardUntilMs = m_simulatedClockMs + 700;
    emit timeChanged();
    emit positionChanged();
    if (m_quiet == 0) {
        emit userSeeked(m_timeMs);
    }
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
                    const QVariantMap best = bestTrackFor(real, pref);
                    if (best.isEmpty()) continue;

                    const int id = best.value("id").toInt();
                    if (id != current) {
                        FLUX_LOG_INFO("VLCPlayer", QString("Auto-selecting preferred audio '%1' (track %2)").arg(pref).arg(id));
                        libvlc_audio_set_track(m_mediaPlayer, id);
                        emit selectedAudioTrackChanged();
                    }
                    done = true;
                    break;
                }
            }
        }
    }

    // ---- Subtitles (Latched Preference Support) ----
    if (!m_subPrefApplied) {
        const QVariantList real = realTracks(m_subtitleTracks);
        if (m_latchedSubtitleDisabled || m_preferredSubtitle.compare(QStringLiteral("off"), Qt::CaseInsensitive) == 0) {
            m_subPrefApplied = true;
            if (libvlc_video_get_spu(m_mediaPlayer) != -1) {
                libvlc_video_set_spu(m_mediaPlayer, -1);
                emit selectedSubtitleTrackChanged();
            }
        } else if (!real.isEmpty()) {
            QVariantMap best;

            // 1. Exact display name match (e.g. "English (2)", "English 2", "Bangla", "Hindi")
            if (!m_latchedSubtitleName.isEmpty()) {
                for (const QVariant &v : real) {
                    const QVariantMap t = v.toMap();
                    if (t.value("name").toString().compare(m_latchedSubtitleName, Qt::CaseInsensitive) == 0) {
                        best = t;
                        break;
                    }
                }
            }

            // 2. Exact raw name match (e.g. "Subtitle 2", "Track 2 - [Bangla]")
            if (best.isEmpty() && !m_latchedSubtitleRawName.isEmpty()) {
                for (const QVariant &v : real) {
                    const QVariantMap t = v.toMap();
                    if (t.value("rawName").toString().compare(m_latchedSubtitleRawName, Qt::CaseInsensitive) == 0) {
                        best = t;
                        break;
                    }
                }
            }

            // 3. Match by relative index if the track at that index matches the language or name
            if (best.isEmpty() && m_latchedSubtitleIndex >= 0 && m_latchedSubtitleIndex < real.size()) {
                const QVariantMap candidate = real.at(m_latchedSubtitleIndex).toMap();
                if (trackMatches(candidate, m_latchedSubtitleName) ||
                    (!m_latchedSubtitleLang.isEmpty() && trackMatches(candidate, m_latchedSubtitleLang))) {
                    best = candidate;
                }
            }

            // 4. Match by language across all tracks
            if (best.isEmpty() && !m_latchedSubtitleLang.isEmpty()) {
                best = bestTrackFor(real, m_latchedSubtitleLang);
            }

            // 5. Match by display name substring / alias
            if (best.isEmpty() && !m_latchedSubtitleName.isEmpty()) {
                best = bestTrackFor(real, m_latchedSubtitleName);
            }

            // 6. Match by exact relative track index (e.g. identical MKV batch with generic names)
            if (best.isEmpty() && m_latchedSubtitleIndex >= 0 && m_latchedSubtitleIndex < real.size()) {
                best = real.at(m_latchedSubtitleIndex).toMap();
            }

            // 7. General user preference fallback (e.g. "English")
            if (best.isEmpty() && !m_preferredSubtitle.isEmpty() && m_preferredSubtitle != QLatin1String("off")) {
                best = bestTrackFor(real, m_preferredSubtitle);
            }

            if (!best.isEmpty()) {
                m_subPrefApplied = true;
                const int id = best.value("id").toInt();
                if (id != libvlc_video_get_spu(m_mediaPlayer)) {
                    FLUX_LOG_INFO("VLCPlayer", QString("Auto-selecting latched subtitle '%1' (track ID %2)").arg(best.value("name").toString()).arg(id));
                    libvlc_video_set_spu(m_mediaPlayer, id);
                    emit selectedSubtitleTrackChanged();
                }
            } else if (m_autoTrackAttempts > 8) {
                m_subPrefApplied = true;
            }
        } else if (m_autoTrackAttempts > 8) {
            m_subPrefApplied = true;
        }
    }
}

void VLCPlayer::learnAudioChoice(int trackId) {
    QString name;
    QString lang;
    for (const QVariant &v : m_audioTracks) {
        const QVariantMap t = v.toMap();
        if (t.value("id").toInt() == trackId) {
            name = t.value("name").toString();
            lang = t.value("lang").toString();
            break;
        }
    }
    // Prefer the clean language ("English") over the full label ("English · 5.1")
    const QString key = lang.isEmpty() ? languageKey(name) : lang;
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

void VLCPlayer::setLatchedSubtitle(const QString &name, const QString &lang, int index, bool disabled) {
    m_latchedSubtitleName = name;
    m_latchedSubtitleLang = lang;
    m_latchedSubtitleIndex = index;
    m_latchedSubtitleDisabled = disabled;
    if (disabled) {
        m_preferredSubtitle = QStringLiteral("off");
    } else if (!lang.isEmpty()) {
        m_preferredSubtitle = lang;
    } else if (!name.isEmpty()) {
        m_preferredSubtitle = name;
    }
}

void VLCPlayer::learnSubtitleChoice(int spuId) {
    if (spuId < 0) {
        m_latchedSubtitleDisabled = true;
        m_latchedSubtitleName = QStringLiteral("Disable");
        m_latchedSubtitleLang.clear();
        m_latchedSubtitleRawName.clear();
        m_latchedSubtitleIndex = -1;
        m_preferredSubtitle = QStringLiteral("off");
        FLUX_LOG_INFO("VLCPlayer", "Latched subtitle choice: Disabled");
        emit languagePreferencesChanged();
        return;
    }

    m_latchedSubtitleDisabled = false;
    QString name;
    QString lang;
    QString rawName;
    int realIndex = 0;
    int foundIndex = -1;

    for (const QVariant &v : m_subtitleTracks) {
        const QVariantMap t = v.toMap();
        const int id = t.value("id").toInt();
        if (id < 0) continue;
        if (id == spuId) {
            name = t.value("name").toString();
            lang = t.value("lang").toString();
            rawName = t.value("rawName").toString();
            foundIndex = realIndex;
            break;
        }
        ++realIndex;
    }

    if (foundIndex < 0) return;

    m_latchedSubtitleName = name;
    m_latchedSubtitleLang = lang;
    m_latchedSubtitleRawName = rawName;
    m_latchedSubtitleIndex = foundIndex;

    const QString key = !lang.isEmpty() ? lang : (!name.isEmpty() ? name : rawName);
    if (!key.isEmpty()) {
        m_preferredSubtitle = key;
    }

    FLUX_LOG_INFO("VLCPlayer", QString("Latched subtitle preference: name='%1', lang='%2', raw='%3', index=%4")
                                   .arg(m_latchedSubtitleName, m_latchedSubtitleLang, m_latchedSubtitleRawName)
                                   .arg(m_latchedSubtitleIndex));
    emit languagePreferencesChanged();
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

QString VLCPlayer::cycleSubtitleTrack() {
    if (!m_mediaPlayer || m_subtitleTracks.isEmpty()) {
        return QStringLiteral("None available");
    }

    if (m_subtitleTracks.size() == 1 && m_subtitleTracks.first().toMap().value(QStringLiteral("id")).toInt() < 0) {
        return QStringLiteral("None available");
    }

    const int curId = selectedSubtitleTrack();
    int curIdx = -1;
    for (int i = 0; i < m_subtitleTracks.size(); ++i) {
        if (m_subtitleTracks.at(i).toMap().value(QStringLiteral("id")).toInt() == curId) {
            curIdx = i;
            break;
        }
    }

    const int nextIdx = (curIdx + 1) % m_subtitleTracks.size();
    const QVariantMap nextTrack = m_subtitleTracks.at(nextIdx).toMap();
    const int nextId = nextTrack.value(QStringLiteral("id")).toInt();
    const QString name = nextTrack.value(QStringLiteral("name")).toString();

    selectSubtitleTrack(nextId);

    if (nextId < 0 || name.compare(QStringLiteral("Disable"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("Disabled");
    }
    return name.isEmpty() ? QString("Track %1").arg(nextId) : name;
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

bool VLCPlayer::isCurrentSubtitleScalable() const {
    return currentSubtitleType() == QLatin1String("text");
}

QString VLCPlayer::currentSubtitleType() const {
    if (!m_mediaPlayer) return QString();
    int curSpu = selectedSubtitleTrack();
    if (curSpu < 0) return QString();
    for (const auto &v : m_subtitleTracks) {
        QVariantMap m = v.toMap();
        if (m.value("id").toInt() == curSpu) {
            return m.value("subType", QStringLiteral("text")).toString();
        }
    }
    return QString();
}

void VLCPlayer::setSubtitleScale(int scale) {
    scale = std::clamp(scale, 50, 200);
    if (m_subtitleScale == scale) return;

    m_subtitleScale = scale;
    emit subtitleScaleChanged();
    FLUX_LOG_INFO("VLCPlayer", QString("Subtitle scale set to %1%").arg(m_subtitleScale));

    applySubtitleScaleLive();
}

void VLCPlayer::increaseSubtitleScale() {

    setSubtitleScale(std::min(200, m_subtitleScale + 10));

}



void VLCPlayer::decreaseSubtitleScale() {

    setSubtitleScale(std::max(50, m_subtitleScale - 10));

}



void VLCPlayer::applySubtitleScaleLive() {
    // 1. Save scale to persistent user settings
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FLUX"), QStringLiteral("FLUX"));
    settings.setValue("subtitles/scale", m_subtitleScale);

    if (!m_mediaPlayer) return;

    // 2. Update global libVLC configuration and root instance variable
    libvlc_instance_t *vlc = VLCInstance::instance().get();
    void *coreObj = vlc ? *(void**)vlc : nullptr;
    vlc_value_t val;
    val.i_int = m_subtitleScale;

    if (coreObj) {
        config_PutInt(coreObj, "sub-text-scale", m_subtitleScale);
        var_Create(coreObj, "sub-text-scale", 0x0020 | 0x8000 /* VLC_VAR_INTEGER | VLC_VAR_DOINHERIT */);
        var_SetChecked(coreObj, "sub-text-scale", 0x0020, val);
    }

    // 3. Update active media player object variable
    var_Create(m_mediaPlayer, "sub-text-scale", 0x0020 | 0x8000);
    var_SetChecked(m_mediaPlayer, "sub-text-scale", 0x0020, val);

    // 4. Update active input thread variable if streaming
    void *input = libvlc_get_input_thread(m_mediaPlayer);
    if (input) {
        var_Create(input, "sub-text-scale", 0x0020 | 0x8000);
        var_SetChecked(input, "sub-text-scale", 0x0020, val);
        vlc_object_release(input);
    }

    FLUX_LOG_INFO("VLCPlayer", QString("Applied subtitle scale %1% dynamically without interrupting playback").arg(m_subtitleScale));
}

void VLCPlayer::updateSleepInhibition(bool force) {
#ifdef Q_OS_WIN
    if (m_simulatedMode) return;

    const bool shouldInhibit = isPlaying() || m_state == QLatin1String("Playing")
                               || (m_isBuffering && m_state.startsWith(QLatin1String("Buffering")));

    if (!force && shouldInhibit == m_displaySleepInhibited) return;
    m_displaySleepInhibited = shouldInhibit;

    if (shouldInhibit) {
        SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);
        if (!force) {
            FLUX_LOG_INFO("VLCPlayer", "Display sleep prevention active (video playing)");
        }
    } else {
        SetThreadExecutionState(ES_CONTINUOUS);
        FLUX_LOG_INFO("VLCPlayer", "Display sleep prevention released");
    }
#else
    Q_UNUSED(force);
#endif
}

} // namespace Flux
