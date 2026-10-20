#include "UserStore.h"
#include "Logger.h"
#include "../media/VLCPlayer.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <algorithm>
#include <vector>

namespace Flux {

namespace {
constexpr qint64 kMinResumeMs = 1000;
constexpr double kWatchedFraction = 0.93;    // past 93% counts as watched
constexpr int kMaxHistoryEntries = 300;
constexpr int kMaxContinueEntries = 24;

struct SeriesLocation {
    QString key;
    QString url;
    QString title;
};

SeriesLocation seriesLocation(const QString &mediaUrl) {
    QUrl url(mediaUrl, QUrl::TolerantMode);
    if (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https"))
        return {};
    QStringList parts = url.path(QUrl::FullyDecoded).split('/', Qt::SkipEmptyParts);
    if (parts.size() < 2) return {};

    const QString episodeName = parts.constLast();
    static const QRegularExpression episodeRe(
        QStringLiteral("(?:\\bS\\d{1,2}\\s*E\\d{1,3}\\b|\\b\\d{1,2}x\\d{1,3}\\b)"),
        QRegularExpression::CaseInsensitiveOption);
    if (!episodeRe.match(episodeName).hasMatch()) return {};

    parts.removeLast(); // the episode file's parent folder
    if (parts.size() < 3) return {}; // Avoid treating a category folder as a show's folder.

    static const QRegularExpression seasonRe(
        QStringLiteral("^(?:season|series|s)[\\s._-]*\\d{1,2}(?:\\b.*)?$|^specials?$|^extras?$"),
        QRegularExpression::CaseInsensitiveOption);
    if (parts.size() > 1 && seasonRe.match(parts.constLast()).hasMatch())
        parts.removeLast();
    if (parts.isEmpty()) return {};

    const QString folderPath = QLatin1Char('/') + parts.join(QLatin1Char('/')) + QLatin1Char('/');
    url.setPath(folderPath);
    const QString folderUrl = url.toString(QUrl::FullyEncoded);
    return {folderUrl.toCaseFolded(), folderUrl, QUrl::fromPercentEncoding(parts.constLast().toUtf8())};
}

static QString normalizeForTitleMatch(const QString &s) {
    QString res = s.toLower();
    static const QRegularExpression nonAlphaNum(QStringLiteral("[^a-z0-9]+"));
    res.replace(nonAlphaNum, QStringLiteral(" "));
    return res.trimmed();
}

static bool isTitleMatch(const QString &target, const QString &candidate) {
    const QString t = normalizeForTitleMatch(target);
    const QString c = normalizeForTitleMatch(candidate);
    if (t.isEmpty() || c.isEmpty()) return false;
    if (t == c) return true;
    if (QStringLiteral("the ") + t == c || t == QStringLiteral("the ") + c) return true;
    return false;
}

struct CleanMediaResult {
    QString title;
    QString year;
    QString type; // "movie" or "series"
};

static QString extractNameFromUrl(const QString &url) {
    QString decoded = QUrl::fromPercentEncoding(url.toUtf8());
    if (decoded.endsWith(QLatin1Char('/'))) decoded.chop(1);
    const int slash = decoded.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? decoded.mid(slash + 1) : decoded;
}

CleanMediaResult cleanMediaTitle(const QString &url, const QString &rawInput, bool isFolderHint) {
    QString raw = rawInput.trimmed();
    if (raw.isEmpty() && !url.trimmed().isEmpty()) {
        raw = extractNameFromUrl(url);
    }
    if (raw.isEmpty()) return {};

    SeriesLocation loc;
    if (!url.trimmed().isEmpty()) {
        loc = seriesLocation(url);
    }

    QString s = QUrl::fromPercentEncoding(raw.toUtf8()).trimmed();

    // Strip common video extensions
    static const QRegularExpression extRe(
        QStringLiteral("\\.(mkv|mp4|avi|mov|flv|ts|m4v|webm|iso|vob|wmv|mpg|mpeg)$"),
        QRegularExpression::CaseInsensitiveOption);
    s.remove(extRe);

    // Strip bracketed content [Dual Audio], [1080p], etc.
    static const QRegularExpression bracketRe(QStringLiteral("\\[[^\\]]*\\]"));
    s.remove(bracketRe);

    // Strip trailing release group tags like "-afm72", "-Pahe", "-HDHub", "-mkvC"
    static const QRegularExpression groupRe(QStringLiteral("\\s*-\\s*[A-Za-z0-9_.]+$"));
    s.remove(groupRe);

    // Check for TV series folder notation: "Title (TV Series 2019– )", "Title [TV Mini Series 2020]", "Title (2019-2020)", etc.
    static const QRegularExpression tvFolderRe(
        QStringLiteral("^(.*?)\\s*[\\(\\[](?:(?:Korean|TV|Mini|Web)[\\s._-]*)*(?:Series)?(?:[\\s._-]*)(19\\d\\d|20\\d\\d)(?:[\\s._-]*[-–—][^\\)\\]]*)?[\\)\\]]"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression tvFolderNoYearRe(
        QStringLiteral("^(.*?)\\s*[\\(\\[](?:(?:Korean|TV|Mini|Web)[\\s._-]*)*Series[^\\)\\]]*[\\)\\]]"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch tvFolderMatch = tvFolderRe.match(s);
    if (tvFolderMatch.hasMatch()) {
        QString title = tvFolderMatch.captured(1).trimmed();
        QString year = tvFolderMatch.captured(2).trimmed();
        title.replace(QLatin1Char('.'), QLatin1Char(' '));
        title.replace(QLatin1Char('_'), QLatin1Char(' '));
        static const QRegularExpression trimCharsRe(QStringLiteral("^[\\s\\(\\)\\[\\]\\-_.,:]+|[\\s\\(\\)\\[\\]\\-_.,:]+$"));
        title.remove(trimCharsRe);
        static const QRegularExpression multiSpaceRe(QStringLiteral("\\s+"));
        title.replace(multiSpaceRe, QStringLiteral(" "));
        return {title.trimmed(), year, QStringLiteral("series")};
    }
    const QRegularExpressionMatch tvNoYearMatch = tvFolderNoYearRe.match(s);
    if (tvNoYearMatch.hasMatch()) {
        QString title = tvNoYearMatch.captured(1).trimmed();
        title.replace(QLatin1Char('.'), QLatin1Char(' '));
        title.replace(QLatin1Char('_'), QLatin1Char(' '));
        static const QRegularExpression trimCharsRe(QStringLiteral("^[\\s\\(\\)\\[\\]\\-_.,:]+|[\\s\\(\\)\\[\\]\\-_.,:]+$"));
        title.remove(trimCharsRe);
        static const QRegularExpression multiSpaceRe(QStringLiteral("\\s+"));
        title.replace(multiSpaceRe, QStringLiteral(" "));
        return {title.trimmed(), QString(), QStringLiteral("series")};
    }

    // Detect series markers
    static const QRegularExpression seriesRe(
        QStringLiteral("\\b(S\\d{1,2}(?:E\\d{1,3})?|\\d{1,2}x\\d{1,3}|Season\\s*\\d{1,2}|Series|Complete)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch seriesMatch = seriesRe.match(s);
    const bool isSeries = isFolderHint
        ? (seriesMatch.hasMatch() || s.contains(QStringLiteral("Season"), Qt::CaseInsensitive) || s.contains(QStringLiteral("Series"), Qt::CaseInsensitive))
        : (seriesMatch.hasMatch() || !loc.url.isEmpty());

    // Extract year: search for (YYYY) or standalone 4-digit years
    QString year;
    static const QRegularExpression parenYearRe(QStringLiteral("\\((19\\d\\d|20\\d\\d)\\)"));
    const QRegularExpressionMatch parenMatch = parenYearRe.match(s);
    if (parenMatch.hasMatch()) {
        year = parenMatch.captured(1);
    } else {
        static const QRegularExpression wordYearRe(QStringLiteral("\\b(19\\d\\d|20\\d\\d)\\b"));
        auto it = wordYearRe.globalMatch(s);
        QList<QRegularExpressionMatch> matches;
        while (it.hasNext()) matches.append(it.next());
        if (!matches.isEmpty()) {
            if (matches.size() > 1 && matches.first().captured(1) == QLatin1String("2049")) {
                year = matches.at(1).captured(1);
            } else {
                year = matches.last().captured(1);
            }
        }
    }

    // Replace dots and underscores with spaces
    s.replace(QLatin1Char('.'), QLatin1Char(' '));
    s.replace(QLatin1Char('_'), QLatin1Char(' '));

    QString cleanTitle;
    if (seriesMatch.hasMatch()) {
        const int idx = seriesMatch.capturedStart();
        QString titlePart = s.left(idx).trimmed();
        if (!year.isEmpty()) {
            const QRegularExpression ymRe(QStringLiteral("\\(?%1\\)?").arg(QRegularExpression::escape(year)));
            titlePart.remove(ymRe);
        }
        cleanTitle = titlePart.trimmed();
    } else if (parenMatch.hasMatch()) {
        cleanTitle = s.left(parenMatch.capturedStart()).trimmed();
    } else if (!year.isEmpty()) {
        const int yearIdx = s.indexOf(year);
        if (yearIdx > 0) {
            cleanTitle = s.left(yearIdx).trimmed();
        } else {
            cleanTitle = s;
        }
    } else {
        cleanTitle = s;
    }

    // Strip common quality tags
    static const QRegularExpression tagsRe(
        QStringLiteral("\\b(1080p|720p|2160p|4k|rm4k|uhd|imax|bluray|blu-ray|bdrip|brrip|web-dl|webrip|web|amzn-web|amzn|hdtv|dvdrip|dvd|"
                       "x264|x265|hevc|h264|h265|avc|10bit|8bit|aac|ac3|dts|ddp5\\.1|dd5\\.1|5\\.1|7\\.1|atmos|truehd|"
                       "esub|esubs|msub|msubs|sub|subs|dual\\s*audio|multi\\s*audio|hindi|bengali|bangla|english|remux|repack|proper|"
                       "extended|unrated|directors\\s*cut|criterion|completed)\\b"),
        QRegularExpression::CaseInsensitiveOption);
    cleanTitle.remove(tagsRe);

    // Clean up residual punctuation and extra whitespace
    static const QRegularExpression trimCharsRe(QStringLiteral("^[\\s\\(\\)\\[\\]\\-_.,:]+|[\\s\\(\\)\\[\\]\\-_.,:]+$"));
    cleanTitle.remove(trimCharsRe);
    static const QRegularExpression multiSpaceRe(QStringLiteral("\\s+"));
    cleanTitle.replace(multiSpaceRe, QStringLiteral(" "));
    cleanTitle = cleanTitle.trimmed();

    // If this is an episode inside a series folder and episode has no year, inherit series title and year from parent series folder
    if (!loc.url.isEmpty() && !loc.title.isEmpty()) {
        CleanMediaResult parentRes = cleanMediaTitle(QString(), loc.title, true);
        if (!parentRes.title.isEmpty()) {
            if (cleanTitle.isEmpty() || cleanTitle.length() < 2) {
                cleanTitle = parentRes.title;
            }
            if (year.isEmpty() && !parentRes.year.isEmpty()) {
                year = parentRes.year;
            }
        }
    }

    if (year.isEmpty() && !url.trimmed().isEmpty()) {
        static const QRegularExpression urlYearRe(QStringLiteral("[/\\\\][^/\\\\]*?\\b(19\\d\\d|20\\d\\d)\\b[^/\\\\]*?[/\\\\]"));
        auto it = urlYearRe.globalMatch(url);
        QList<QRegularExpressionMatch> uMatches;
        while (it.hasNext()) uMatches.append(it.next());
        if (!uMatches.isEmpty()) {
            year = uMatches.last().captured(1);
        }
    }

    // Disambiguation for Korean series Kingdom (2019)
    if (cleanTitle.compare(QLatin1String("Kingdom"), Qt::CaseInsensitive) == 0 &&
        (url.contains(QLatin1String("korean"), Qt::CaseInsensitive) || raw.contains(QLatin1String("korean"), Qt::CaseInsensitive))) {
        if (year.isEmpty()) year = QStringLiteral("2019");
        return {cleanTitle, year, QStringLiteral("series")};
    }

    const QString mediaType = isSeries ? QStringLiteral("series") : QStringLiteral("movie");
    return {cleanTitle, year, mediaType};
}
}

UserStore::UserStore(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FLUX"), QStringLiteral("FLUX")) {

    m_posterNam = new QNetworkAccessManager(this);
    m_autoFetchPosters = m_settings.value(QStringLiteral("posters/autoFetch"), true).toBool();
    m_tmdbApiKey = m_settings.value(QStringLiteral("posters/tmdbApiKey"), QString()).toString();

    const QFileInfo settingsInfo(m_settings.fileName());
    QDir().mkpath(settingsInfo.absolutePath());
    m_historyPath = settingsInfo.absolutePath() + QStringLiteral("/history.json");
    m_posterPath = settingsInfo.absolutePath() + QStringLiteral("/posters.json");

    loadHistory();
    loadPosters();

    m_posterSaveTimer.setSingleShot(true);
    m_posterSaveTimer.setInterval(2000);
    connect(&m_posterSaveTimer, &QTimer::timeout, this, [this]() { savePosters(); });

    m_tickTimer.setInterval(5000);
    connect(&m_tickTimer, &QTimer::timeout, this, [this]() { checkpoint(true); });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(4000);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() { writeHistory(); });

    m_settingsTimer.setSingleShot(true);
    m_settingsTimer.setInterval(1000);
    connect(&m_settingsTimer, &QTimer::timeout, this, [this]() { m_settings.sync(); });

    // Auto-detect server connectivity on startup (non-blocking)
    QTimer::singleShot(400, this, [this]() {
        autoDetectServer();
    });

    FLUX_LOG_INFO("UserStore", QString("Settings: %1 | History: %2 (%3 entries)")
                  .arg(m_settings.fileName(), m_historyPath).arg(m_history.size()));
}

UserStore::~UserStore() {
    shutdown();
}

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

QString UserStore::keyFor(const QString &url) {
    return QUrl::fromPercentEncoding(url.toUtf8());
}

QString UserStore::nameFromUrl(const QString &url) {
    QString decoded = keyFor(url);
    if (decoded.endsWith('/')) decoded.chop(1);
    const int slash = decoded.lastIndexOf('/');
    return slash >= 0 ? decoded.mid(slash + 1) : decoded;
}

// Identity of an item for poster lookups. ICC FTP addresses carry a session token that changes,
// so they are matched by their play id only; everything else by its decoded address.
QString UserStore::posterKey(const QString &url) {
    if (url.contains(QLatin1String("player.php"))) {
        static const QRegularExpression playRe(QStringLiteral("[?&]play=(\\d+)"));
        const QRegularExpressionMatch m = playRe.match(url);
        if (m.hasMatch()) return QStringLiteral("icc:") + m.captured(1);
    }
    QString key = QUrl::fromPercentEncoding(url.trimmed().toUtf8());
    while (key.endsWith(QLatin1Char('/'))) key.chop(1);
    return key.toCaseFolded();
}

QString UserStore::titleKey(const QString &title) {
    if (title.trimmed().isEmpty()) return QString();
    QString t = title.trimmed().toCaseFolded();
    static const QRegularExpression nonAlphanumRx(QStringLiteral("[^a-z0-9]+"));
    t.replace(nonAlphanumRx, QStringLiteral(" "));
    t = t.trimmed();
    return t.isEmpty() ? QString() : (QStringLiteral("title:") + t);
}

QString UserStore::folderKey(const QString &url) {
    if (url.trimmed().isEmpty()) return QString();
    QString key = QUrl::fromPercentEncoding(url.trimmed().toUtf8());
    while (key.endsWith(QLatin1Char('/'))) key.chop(1);
    return key.toCaseFolded();
}

// ----------------------------------------------------------------------------
// Player tracking
// ----------------------------------------------------------------------------

void UserStore::attachPlayer(VLCPlayer *player) {
    m_player = player;
    if (!m_player) return;
    connect(m_player, &VLCPlayer::stateChanged, this, &UserStore::onPlayerStateChanged);
    connect(m_player, &VLCPlayer::userSeeked, this, [this](qint64) {
        checkpoint(true, true);
        writeHistory();
    });
    connect(m_player, &VLCPlayer::aboutToStop, this, [this]() {
        checkpoint(true);
        writeHistory();
    });
}

void UserStore::onPlayerStateChanged() {
    if (!m_player || m_currentKey.isEmpty()) return;
    const QString st = m_player->state();

    if (st == QLatin1String("Playing")) {
        if (!m_tickTimer.isActive()) m_tickTimer.start();
        return;
    }

    if (st == QLatin1String("Ended")) {
        m_tickTimer.stop();
        auto it = m_history.find(m_currentKey);
        if (it != m_history.end() && keyFor(m_player->url()) == m_currentPlaybackKey) {
            it->watched = true;
            it->positionMs = 0;
            it->lastPlayed = QDateTime::currentMSecsSinceEpoch();
            ++m_revision;
            emit historyChanged();
            m_dirty = true;
            scheduleSave();
            writeHistory();
        }
        return;
    }

    if (st == QLatin1String("Paused") || st == QLatin1String("Stopped") || st == QLatin1String("Error")) {
        m_tickTimer.stop();
        checkpoint(true);
        writeHistory();
    }
}

void UserStore::checkpoint(bool notify, bool allowZeroPosition) {
    if (!m_player || m_currentKey.isEmpty()) return;
    // Only record progress for the file this entry actually belongs to
    if (keyFor(m_player->url()) != m_currentPlaybackKey) return;

    const qint64 pos = m_player->timeMs();
    const qint64 dur = m_player->durationMs();
    // A seek to zero is intentional. Ordinary zero-clock samples (startup/stop) are ignored.
    if ((pos <= 0 && !allowZeroPosition) || pos < 0) return;

    auto it = m_history.find(m_currentKey);
    if (it == m_history.end()) return;

    HistoryEntry &e = it.value();
    if (dur > 0) e.durationMs = dur;
    e.lastPlayed = QDateTime::currentMSecsSinceEpoch();

    if (dur > 0 && pos >= static_cast<qint64>(static_cast<double>(dur) * kWatchedFraction)) {
        e.watched = true;
        e.positionMs = pos;
    } else {
        e.positionMs = pos;
        if (pos >= kMinResumeMs || allowZeroPosition) e.watched = false; // replaying or seeking back
    }

    m_dirty = true;
    scheduleSave();

    if (notify) {
        ++m_revision;
        emit historyChanged();
    }
}

void UserStore::noteStart(const QString &url, const QString &title, const QString &playbackUrl) {
    const QString key = keyFor(url);
    if (key.isEmpty()) return;

    // Capture the previous file's final position before switching to the new one
    checkpoint(false);

    HistoryEntry &e = m_history[key];
    e.url = url;
    e.title = nameFromUrl(url);
    // Addresses like ".../player.php?play=123" have no readable file name: use the title we were given
    if (e.title.isEmpty() || ((url.contains(QLatin1String("player.php")) || e.title.contains('?')) && !title.isEmpty())) {
        e.title = title;
    }
    if (e.title.isEmpty()) e.title = title;
    e.lastPlayed = QDateTime::currentMSecsSinceEpoch();
    e.watched = false;
    if (e.positionMs < 0) e.positionMs = 0;

    m_currentKey = key;
    m_currentPlaybackKey = keyFor(playbackUrl.isEmpty() ? url : playbackUrl);
    if (!playbackUrl.isEmpty() && playbackUrl != url) {
        const QString p = posterFor(url, title);
        if (!p.isEmpty()) {
            rememberPoster(playbackUrl, p, title);
        }
    }
    m_dirty = true;
    scheduleSave();

    ++m_revision;
    emit historyChanged();
}

void UserStore::notePartyMedia(const QString &url, const QString &title, qint64 positionMs, qint64 durationMs) {
    const QString key = keyFor(url);
    if (key.isEmpty()) return;

    m_partyKey = key;
    m_hasPartyMedia = true;

    m_partyEntry.url = url;
    m_partyEntry.title = nameFromUrl(url);
    if (m_partyEntry.title.isEmpty() || ((url.contains(QLatin1String("player.php")) || m_partyEntry.title.contains('?')) && !title.isEmpty())) {
        m_partyEntry.title = title;
    }
    if (m_partyEntry.title.isEmpty()) m_partyEntry.title = title;
    m_partyEntry.positionMs = std::max<qint64>(0, positionMs);
    m_partyEntry.durationMs = durationMs > 0 ? durationMs : 3600000;
    m_partyEntry.lastPlayed = QDateTime::currentMSecsSinceEpoch();
    m_partyEntry.watched = false;

    // Do NOT overwrite m_history or reset the user's personal positionMs.
    // Joining a Teleparty must never reset or corrupt the user's existing watch history.

    ++m_revision;
    emit historyChanged();
}

void UserStore::clearPartyMedia() {
    if (m_partyKey.isEmpty() && !m_hasPartyMedia) return;
    m_partyKey.clear();
    m_hasPartyMedia = false;
    m_partyEntry = HistoryEntry();
    ++m_revision;
    emit historyChanged();
}

// ----------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------

QVariantMap UserStore::progressFor(const QString &url) const {
    QVariantMap map;
    const auto it = m_history.constFind(keyFor(url));
    if (it != m_history.constEnd()) {
        const HistoryEntry &e = it.value();
        const double fraction = (e.durationMs > 0 && e.positionMs > 0)
                                ? std::clamp(static_cast<double>(e.positionMs) / static_cast<double>(e.durationMs), 0.0, 1.0)
                                : (e.watched ? 1.0 : 0.0);
        map["positionMs"] = e.positionMs;
        map["durationMs"] = e.durationMs;
        map["fraction"] = fraction;
        map["watched"] = e.watched;
        map["lastPlayed"] = e.lastPlayed;
        return map;
    }

    if (m_hasPartyMedia && !m_partyKey.isEmpty() && keyFor(url) == m_partyKey) {
        const double fraction = (m_partyEntry.durationMs > 0 && m_partyEntry.positionMs > 0)
                                ? std::clamp(static_cast<double>(m_partyEntry.positionMs) / static_cast<double>(m_partyEntry.durationMs), 0.0, 1.0)
                                : 0.0;
        map["positionMs"] = m_partyEntry.positionMs;
        map["durationMs"] = m_partyEntry.durationMs;
        map["fraction"] = fraction;
        map["watched"] = false;
        map["lastPlayed"] = m_partyEntry.lastPlayed;
        return map;
    }

    return map;
}

qint64 UserStore::resumePositionFor(const QString &url) const {
    const auto it = m_history.constFind(keyFor(url));
    if (it == m_history.constEnd()) {
        if (m_hasPartyMedia && !m_partyKey.isEmpty() && keyFor(url) == m_partyKey) {
            if (m_partyEntry.positionMs < kMinResumeMs) return 0;
            if (m_partyEntry.durationMs > 0 && m_partyEntry.positionMs >= m_partyEntry.durationMs) return 0;
            return m_partyEntry.positionMs;
        }
        return 0;
    }

    const HistoryEntry &e = it.value();
    if (e.positionMs < kMinResumeMs) return 0;
    if (e.durationMs > 0 && e.positionMs >= e.durationMs) return 0;
    return e.positionMs;
}

QVariantList UserStore::continueWatching() const {
    std::vector<const HistoryEntry *> allEntries;
    allEntries.reserve(static_cast<size_t>(m_history.size()) + 1);

    bool partyInHistory = false;
    for (auto it = m_history.constBegin(); it != m_history.constEnd(); ++it) {
        const HistoryEntry &e = it.value();
        if (e.url.isEmpty()) continue;
        if (!m_partyKey.isEmpty() && keyFor(e.url) == m_partyKey) {
            partyInHistory = true;
        }
        allEntries.push_back(&e);
    }

    if (!m_partyKey.isEmpty() && m_hasPartyMedia && !partyInHistory && !m_partyEntry.url.isEmpty()) {
        allEntries.push_back(&m_partyEntry);
    }

    const auto recentFirst = [this](const HistoryEntry *a, const HistoryEntry *b) {
        if (!m_partyKey.isEmpty()) {
            const bool aParty = (keyFor(a->url) == m_partyKey);
            const bool bParty = (keyFor(b->url) == m_partyKey);
            if (aParty != bParty) return aParty;
        }
        return a->lastPlayed > b->lastPlayed;
    };
    std::sort(allEntries.begin(), allEntries.end(), recentFirst);

    QVariantList list;
    for (const HistoryEntry *e : allEntries) {
        if (list.size() >= kMaxContinueEntries) break;
        QVariantMap m;
        m["url"] = e->url;
        m["title"] = e->title;
        m["posterUrl"] = posterFor(e->url, e->title);
        m["positionMs"] = e->positionMs;
        m["durationMs"] = e->durationMs;
        m["watched"] = e->watched;
        m["fraction"] = (e->durationMs > 0 && e->positionMs > 0)
                        ? std::clamp(static_cast<double>(e->positionMs) / static_cast<double>(e->durationMs), 0.0, 1.0)
                        : (e->watched ? 1.0 : 0.0);
        m["lastPlayed"] = e->lastPlayed;
        m["isFolder"] = false;
        list.append(m);
    }
    return list;
}

// ----------------------------------------------------------------------------
// Mutations
// ----------------------------------------------------------------------------

void UserStore::removeFromHistory(const QString &url) {
    const QString key = keyFor(url);
    if (!m_partyKey.isEmpty() && m_partyKey == key) {
        clearPartyMedia();
    }
    if (m_history.remove(key)) {
        m_dirty = true;
        scheduleSave();
        ++m_revision;
        emit historyChanged();
    }
}

void UserStore::markWatched(const QString &url, bool watched) {
    auto it = m_history.find(keyFor(url));
    if (it == m_history.end()) return;
    it->watched = watched;
    if (watched && it->durationMs > 0) it->positionMs = it->durationMs;
    m_dirty = true;
    scheduleSave();
    ++m_revision;
    emit historyChanged();
}

void UserStore::clearHistory() {
    m_history.clear();
    m_dirty = true;
    scheduleSave();
    ++m_revision;
    emit historyChanged();
}

// ----------------------------------------------------------------------------
// Settings
// ----------------------------------------------------------------------------

QVariant UserStore::value(const QString &key, const QVariant &defaultValue) const {
    return m_settings.value(key, defaultValue);
}

int UserStore::intValue(const QString &key, int defaultValue) const {
    bool ok = false;
    const int v = m_settings.value(key, defaultValue).toInt(&ok);
    return ok ? v : defaultValue;
}

bool UserStore::boolValue(const QString &key, bool defaultValue) const {
    return m_settings.value(key, defaultValue).toBool();
}

void UserStore::setValue(const QString &key, const QVariant &value) {
    m_settings.setValue(key, value);
    m_settingsTimer.start();
}

bool UserStore::autoplayNext() const {
    return m_settings.value(QStringLiteral("player/autoplayNext"), true).toBool();
}

void UserStore::setAutoplayNext(bool enabled) {
    if (autoplayNext() == enabled) return;
    setValue(QStringLiteral("player/autoplayNext"), enabled);
    emit autoplayNextChanged();
}

bool UserStore::topPicksFirst() const {
    return m_settings.value(QStringLiteral("ui/topPicksFirst"), true).toBool();
}

void UserStore::setTopPicksFirst(bool first) {
    if (topPicksFirst() == first) return;
    setValue(QStringLiteral("ui/topPicksFirst"), first);
    emit topPicksFirstChanged();
}

int UserStore::activeServer() const {
    return m_settings.value(QStringLiteral("server/activeServer"), 2).toInt();
}

void UserStore::setActiveServer(int server) {
    if (server < 1 || server > 2) server = 1;
    if (activeServer() != server) {
        m_settings.setValue(QStringLiteral("server/activeServer"), server);
        m_settingsTimer.start();
        emit serverChanged();
    }
}

QString UserStore::iccftpUrl() const {
    return m_settings.value(QStringLiteral("server/iccftpUrl"),
        QStringLiteral("http://10.16.100.244/dashboard.php?session=b1bdfdae1f7e206b628274ef7eafadcd6094e07c46cd85b157c595174a369f96")).toString();
}

void UserStore::setIccftpUrl(const QString &url) {
    QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) return;
    m_settings.setValue(QStringLiteral("server/iccftpUrl"), trimmed);

    const QUrl u(trimmed);
    const QUrlQuery q(u);
    const QString session = q.queryItemValue(QStringLiteral("session"));
    if (!session.isEmpty()) {
        m_settings.setValue(QStringLiteral("server/iccftpSession"), session);
    }
    m_settingsTimer.start();
    emit serverChanged();
}

QString UserStore::iccftpSession() const {
    return m_settings.value(QStringLiteral("server/iccftpSession"),
        QStringLiteral("b1bdfdae1f7e206b628274ef7eafadcd6094e07c46cd85b157c595174a369f96")).toString();
}

void UserStore::setIccftpSession(const QString &session) {
    QString trimmed = session.trimmed();
    if (trimmed.isEmpty()) return;
    m_settings.setValue(QStringLiteral("server/iccftpSession"), trimmed);
    m_settingsTimer.start();
    emit serverChanged();
}

bool UserStore::autoFetchPosters() const {
    return m_autoFetchPosters;
}

void UserStore::setAutoFetchPosters(bool enabled) {
    if (m_autoFetchPosters == enabled) return;
    m_autoFetchPosters = enabled;
    setValue(QStringLiteral("posters/autoFetch"), enabled);
    emit autoFetchPostersChanged();
}

QString UserStore::tmdbApiKey() const {
    return m_tmdbApiKey;
}

void UserStore::setTmdbApiKey(const QString &key) {
    const QString trimmed = key.trimmed();
    if (m_tmdbApiKey == trimmed) return;
    m_tmdbApiKey = trimmed;
    setValue(QStringLiteral("posters/tmdbApiKey"), trimmed);
    emit tmdbApiKeyChanged();
}

void UserStore::autoDetectServer() {
    m_serverCheckComplete = false;
    auto *nam = new QNetworkAccessManager(this);
    auto *pendingCount = new int(2);
    auto *s1Ok = new bool(false);
    auto *s2Ok = new bool(false);

    auto checkDone = [this, nam, pendingCount, s1Ok, s2Ok]() {
        (*pendingCount)--;
        if (*pendingCount == 0) {
            nam->deleteLater();
            const bool s1 = *s1Ok;
            const bool s2 = *s2Ok;
            delete pendingCount;
            delete s1Ok;
            delete s2Ok;

            m_server1Available = s1;
            m_server2Available = s2;
            m_serverCheckComplete = true;
            emit serverAvailabilityChanged();

            FLUX_LOG_INFO("UserStore", QStringLiteral("Server auto-detection complete: Dhaka Flix = %1, ICC FTP = %2")
                          .arg(s1 ? QStringLiteral("OK") : QStringLiteral("DOWN"))
                          .arg(s2 ? QStringLiteral("OK") : QStringLiteral("DOWN")));

            if (s1 && !s2) {
                if (activeServer() != 1) {
                    FLUX_LOG_INFO("UserStore", "Only Dhaka Flix is accessible on this ISP/network. Auto-switching to Server 1.");
                    setActiveServer(1);
                }
            } else if (!s1 && s2) {
                if (activeServer() != 2) {
                    FLUX_LOG_INFO("UserStore", "Only ICC FTP is accessible on this ISP/network. Auto-switching to Server 2.");
                    setActiveServer(2);
                }
            }
        }
    };

    // Probe Server 1 (Dhaka Flix)
    QUrl s1Url(QStringLiteral("http://172.16.50.7/DHAKA-FLIX-7/"));
    QNetworkRequest s1Req(s1Url);
    s1Req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    s1Req.setTransferTimeout(3500);
    QNetworkReply *s1Reply = nam->get(s1Req);
    connect(s1Reply, &QNetworkReply::finished, this, [s1Reply, s1Ok, checkDone]() {
        s1Reply->deleteLater();
        if (s1Reply->error() == QNetworkReply::NoError) {
            const int code = s1Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (code >= 200 && code < 400) {
                *s1Ok = true;
            }
        }
        checkDone();
    });

    // Probe Server 2 (ICC FTP)
    QUrl s2Url(iccftpUrl());
    QNetworkRequest s2Req(s2Url);
    s2Req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    s2Req.setTransferTimeout(3500);
    QNetworkReply *s2Reply = nam->get(s2Req);
    connect(s2Reply, &QNetworkReply::finished, this, [s2Reply, s2Ok, checkDone]() {
        s2Reply->deleteLater();
        if (s2Reply->error() == QNetworkReply::NoError) {
            const int code = s2Reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (code >= 200 && code < 400) {
                *s2Ok = true;
            }
        }
        checkDone();
    });
}

void UserStore::testServerConnection(int server, const QString &customUrl, const QJSValue &callback) {
    auto *nam = new QNetworkAccessManager(this);
    QUrl targetUrl;
    if (server == 1) {
        targetUrl = QUrl(customUrl.isEmpty() ? QStringLiteral("http://172.16.50.7/DHAKA-FLIX-7/") : customUrl);
    } else {
        targetUrl = QUrl(customUrl.isEmpty() ? iccftpUrl() : customUrl);
    }

    QNetworkRequest request(targetUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(5000);
    QNetworkReply *reply = nam->get(request);

    connect(reply, &QNetworkReply::finished, this, [nam, reply, callback]() mutable {
        reply->deleteLater();
        nam->deleteLater();

        bool ok = false;
        QString message;

        if (reply->error() == QNetworkReply::NoError) {
            int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (code >= 200 && code < 400) {
                ok = true;
                message = QStringLiteral("Connection successful! Server responded with HTTP %1.").arg(code);
            } else {
                message = QStringLiteral("Server returned HTTP status code %1.").arg(code);
            }
        } else {
            message = reply->errorString();
        }

        if (callback.isCallable()) {
            QJSValueList args;
            args << ok << message;
            callback.call(args);
        }
    });
}

// ----------------------------------------------------------------------------
// Posters
// ----------------------------------------------------------------------------

void UserStore::rememberPoster(const QString &url, const QString &posterUrl, const QString &title) {
    const QString poster = posterUrl.trimmed();
    if (poster.isEmpty()) return;

    bool updated = false;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    if (!url.trimmed().isEmpty()) {
        const QString key = posterKey(url);
        auto it = m_posters.find(key);
        if (it == m_posters.end() || it->url != poster) {
            PosterEntry entry;
            entry.url = poster;
            entry.usedAt = now;
            m_posters.insert(key, entry);
            updated = true;
        } else {
            it->usedAt = now;
        }

        const QString fKey = folderKey(url);
        if (!fKey.isEmpty() && fKey != key) {
            m_posters[fKey] = {poster, now};
            updated = true;
        }
    }

    const QString tKey = titleKey(title);
    if (!tKey.isEmpty()) {
        auto it = m_posters.find(tKey);
        if (it == m_posters.end() || it->url != poster) {
            PosterEntry entry;
            entry.url = poster;
            entry.usedAt = now;
            m_posters.insert(tKey, entry);
            updated = true;
        } else {
            it->usedAt = now;
        }
    }

    if (!updated) return;

    // Keep the hash bounded: forget the oldest posters first
    constexpr int kMaxPosters = 1500;
    if (m_posters.size() > kMaxPosters) {
        std::vector<std::pair<qint64, QString>> order;
        order.reserve(static_cast<size_t>(m_posters.size()));
        for (auto p = m_posters.constBegin(); p != m_posters.constEnd(); ++p) {
            order.emplace_back(p.value().usedAt, p.key());
        }
        std::sort(order.begin(), order.end());
        for (size_t i = 0; i < 300 && i < order.size(); ++i) m_posters.remove(order[i].second);
    }

    m_postersDirty = true;
    if (!m_posterSaveTimer.isActive()) m_posterSaveTimer.start();

    ++m_posterRevision;
    emit postersChanged();
}

QString UserStore::posterFor(const QString &url, const QString &title) const {
    if (!url.trimmed().isEmpty()) {
        const auto it = m_posters.constFind(posterKey(url));
        if (it != m_posters.constEnd() && !it->url.isEmpty()) return it->url;

        const auto fIt = m_posters.constFind(folderKey(url));
        if (fIt != m_posters.constEnd() && !fIt->url.isEmpty()) return fIt->url;

        const QString mapped = seriesFolderFor(url);
        if (!mapped.isEmpty() && mapped != url) {
            const auto mIt = m_posters.constFind(posterKey(mapped));
            if (mIt != m_posters.constEnd() && !mIt->url.isEmpty()) return mIt->url;
        }

        const SeriesLocation loc = seriesLocation(url);
        if (!loc.url.isEmpty()) {
            const auto sIt = m_posters.constFind(folderKey(loc.url));
            if (sIt != m_posters.constEnd() && !sIt->url.isEmpty()) return sIt->url;
            const auto sIt2 = m_posters.constFind(posterKey(loc.url));
            if (sIt2 != m_posters.constEnd() && !sIt2->url.isEmpty()) return sIt2->url;
        }
    }

    if (!title.trimmed().isEmpty()) {
        const auto tIt = m_posters.constFind(titleKey(title));
        if (tIt != m_posters.constEnd() && !tIt->url.isEmpty()) return tIt->url;
    }

    if (!url.trimmed().isEmpty()) {
        const QString name = nameFromUrl(url);
        if (!name.isEmpty() && name != title) {
            const auto nIt = m_posters.constFind(titleKey(name));
            if (nIt != m_posters.constEnd() && !nIt->url.isEmpty()) return nIt->url;
        }
    }

    return QString();
}

void UserStore::mapSeriesFolder(const QString &mediaFolderUrl, const QString &webFolderUrl) {
    if (mediaFolderUrl.trimmed().isEmpty() || webFolderUrl.trimmed().isEmpty()) return;
    const QString k = folderKey(mediaFolderUrl);
    if (k.isEmpty()) return;
    if (m_seriesFolders.value(k) == webFolderUrl) return;

    m_seriesFolders.insert(k, webFolderUrl);
    m_postersDirty = true;
    if (!m_posterSaveTimer.isActive()) m_posterSaveTimer.start();
}

QString UserStore::seriesFolderFor(const QString &mediaFolderUrl) const {
    if (mediaFolderUrl.trimmed().isEmpty()) return QString();
    return m_seriesFolders.value(folderKey(mediaFolderUrl));
}

void UserStore::loadPosters() {
    QFile file(m_posterPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isArray()) {
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject o = v.toObject();
            const QString key = o.value("k").toString();
            const QString url = o.value("u").toString();
            if (key.isEmpty() || url.isEmpty()) continue;
            if (url.contains(QLatin1String("BOTVjYjY4"))) {
                m_postersDirty = true;
                continue;
            }
            PosterEntry e;
            e.url = url;
            e.usedAt = static_cast<qint64>(o.value("t").toDouble());
            m_posters.insert(key, e);
        }
    } else if (doc.isObject()) {
        const QJsonObject rootObj = doc.object();
        for (const QJsonValue &v : rootObj.value("posters").toArray()) {
            const QJsonObject o = v.toObject();
            const QString key = o.value("k").toString();
            const QString url = o.value("u").toString();
            if (key.isEmpty() || url.isEmpty()) continue;
            if (url.contains(QLatin1String("BOTVjYjY4"))) {
                m_postersDirty = true;
                continue;
            }
            PosterEntry e;
            e.url = url;
            e.usedAt = static_cast<qint64>(o.value("t").toDouble());
            m_posters.insert(key, e);
        }
        for (const QJsonValue &v : rootObj.value("seriesFolders").toArray()) {
            const QJsonObject o = v.toObject();
            const QString m = o.value("m").toString();
            const QString w = o.value("w").toString();
            if (!m.isEmpty() && !w.isEmpty()) {
                m_seriesFolders.insert(m, w);
            }
        }
    }

    if (m_postersDirty && !m_posterSaveTimer.isActive()) {
        m_posterSaveTimer.start();
    }
}

void UserStore::savePosters() {
    if (!m_postersDirty) return;

    QJsonObject rootObj;

    QJsonArray posterArr;
    for (auto it = m_posters.constBegin(); it != m_posters.constEnd(); ++it) {
        QJsonObject o;
        o["k"] = it.key();
        o["u"] = it.value().url;
        o["t"] = static_cast<double>(it.value().usedAt);
        posterArr.append(o);
    }
    rootObj["posters"] = posterArr;

    QJsonArray folderArr;
    for (auto it = m_seriesFolders.constBegin(); it != m_seriesFolders.constEnd(); ++it) {
        QJsonObject o;
        o["m"] = it.key();
        o["w"] = it.value();
        folderArr.append(o);
    }
    rootObj["seriesFolders"] = folderArr;

    QSaveFile file(m_posterPath);
    if (!file.open(QIODevice::WriteOnly)) {
        FLUX_LOG_WARN("UserStore", "Could not write posters file: " + m_posterPath);
        return;
    }
    file.write(QJsonDocument(rootObj).toJson(QJsonDocument::Compact));
    if (file.commit()) m_postersDirty = false;
}

void UserStore::clearPosters() {
    m_posters.clear();
    m_seriesFolders.clear();
    m_pendingPosterFetches.clear();
    m_failedPosterFetches.clear();
    m_posterQueue.clear();
    m_postersDirty = true;
    savePosters();
    ++m_posterRevision;
    emit postersChanged();
}

void UserStore::fetchPoster(const QString &url, const QString &title, bool isFolder) {
    if (!m_autoFetchPosters) return;

    // Fast check: already have it?
    const QString existing = posterFor(url, title);
    if (!existing.isEmpty()) return;

    const QString raw = title.trimmed().isEmpty() ? nameFromUrl(url) : title;
    if (raw.trimmed().isEmpty()) return;

    const QString uKey = posterKey(url);
    const QString tKey = titleKey(raw);

    // Negative cache check
    if ((!uKey.isEmpty() && m_failedPosterFetches.contains(uKey)) ||
        (!tKey.isEmpty() && m_failedPosterFetches.contains(tKey))) {
        return;
    }

    // In-flight check
    if ((!uKey.isEmpty() && m_pendingPosterFetches.contains(uKey)) ||
        (!tKey.isEmpty() && m_pendingPosterFetches.contains(tKey))) {
        return;
    }

    CleanMediaResult cleaned = cleanMediaTitle(url, raw, isFolder);
    if (cleaned.title.trimmed().isEmpty()) {
        if (!uKey.isEmpty()) m_failedPosterFetches.insert(uKey);
        if (!tKey.isEmpty()) m_failedPosterFetches.insert(tKey);
        return;
    }

    const QString qualifiedTKey = titleKey(cleaned.year.isEmpty() ? cleaned.title : (cleaned.title + QStringLiteral(" ") + cleaned.year));
    auto it = m_posters.constFind(qualifiedTKey);
    if (it != m_posters.constEnd() && !it->url.isEmpty()) {
        const QString foundUrl = it->url;
        QTimer::singleShot(0, this, [this, url, foundUrl, raw]() {
            rememberPoster(url, foundUrl, raw);
        });
        return;
    }

    if (m_failedPosterFetches.contains(qualifiedTKey) || m_pendingPosterFetches.contains(qualifiedTKey)) {
        return;
    }

    if (!uKey.isEmpty()) m_pendingPosterFetches.insert(uKey);
    if (!tKey.isEmpty()) m_pendingPosterFetches.insert(tKey);
    m_pendingPosterFetches.insert(qualifiedTKey);

    PosterFetchTask task;
    task.url = url;
    task.originalTitle = raw;
    task.cleanTitle = cleaned.title;
    task.year = cleaned.year;
    task.type = cleaned.type;

    m_posterQueue.append(task);
    processPosterQueue();
}

void UserStore::processPosterQueue() {
    if (!m_posterNam || !m_autoFetchPosters) return;
    constexpr int kMaxConcurrent = 4;
    while (m_activePosterRequests < kMaxConcurrent && !m_posterQueue.isEmpty()) {
        PosterFetchTask task = m_posterQueue.takeFirst();
        ++m_activePosterRequests;
        executePosterFetch(task);
    }
}

void UserStore::executePosterFetch(const PosterFetchTask &task) {
    if (!m_autoFetchPosters) {
        --m_activePosterRequests;
        processPosterQueue();
        return;
    }

    const QString qualifiedTKey = titleKey(task.year.isEmpty() ? task.cleanTitle : (task.cleanTitle + QStringLiteral(" ") + task.year));

    const QString tmdbKey = tmdbApiKey();
    if (!tmdbKey.isEmpty()) {
        QUrl url(QStringLiteral("https://api.themoviedb.org/3/search/multi"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("api_key"), tmdbKey);
        query.addQueryItem(QStringLiteral("query"), task.cleanTitle);
        query.addQueryItem(QStringLiteral("include_adult"), QStringLiteral("false"));
        if (!task.year.isEmpty()) {
            query.addQueryItem(QStringLiteral("year"), task.year);
        }
        url.setQuery(query);

        QNetworkRequest req(url);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("FLUX/2.0 (Windows)"));

        QNetworkReply *reply = m_posterNam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, task, qualifiedTKey]() {
            reply->deleteLater();
            --m_activePosterRequests;
            if (reply->error() == QNetworkReply::NoError) {
                handleTmdbResponse(task, reply->readAll());
            } else {
                m_failedPosterFetches.insert(posterKey(task.url));
                if (!task.originalTitle.isEmpty()) m_failedPosterFetches.insert(titleKey(task.originalTitle));
                m_failedPosterFetches.insert(qualifiedTKey);
                m_pendingPosterFetches.remove(posterKey(task.url));
                m_pendingPosterFetches.remove(titleKey(task.originalTitle));
                m_pendingPosterFetches.remove(qualifiedTKey);
            }
            processPosterQueue();
        });
    } else {
        const QString type = task.type.isEmpty() ? QStringLiteral("movie") : task.type;
        const QString encodedQuery = QUrl::toPercentEncoding(task.cleanTitle);
        const QString endpoint = QStringLiteral("https://v3-cinemeta.strem.io/catalog/%1/top/search=%2.json")
                                    .arg(type, encodedQuery);

        const QUrl targetUrl(endpoint);
        QNetworkRequest req(targetUrl);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) FLUX/2.0"));

        QNetworkReply *reply = m_posterNam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply, task, qualifiedTKey]() {
            reply->deleteLater();
            --m_activePosterRequests;
            if (reply->error() == QNetworkReply::NoError) {
                handleCinemetaResponse(task, reply->readAll());
            } else {
                m_failedPosterFetches.insert(posterKey(task.url));
                if (!task.originalTitle.isEmpty()) m_failedPosterFetches.insert(titleKey(task.originalTitle));
                m_failedPosterFetches.insert(qualifiedTKey);
                m_pendingPosterFetches.remove(posterKey(task.url));
                m_pendingPosterFetches.remove(titleKey(task.originalTitle));
                m_pendingPosterFetches.remove(qualifiedTKey);
            }
            processPosterQueue();
        });
    }
}

void UserStore::handleCinemetaResponse(const PosterFetchTask &task, const QByteArray &data) {
    const QString qualifiedTKey = titleKey(task.year.isEmpty() ? task.cleanTitle : (task.cleanTitle + QStringLiteral(" ") + task.year));
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (!doc.isObject()) {
        m_failedPosterFetches.insert(posterKey(task.url));
        if (!task.originalTitle.isEmpty()) m_failedPosterFetches.insert(titleKey(task.originalTitle));
        m_failedPosterFetches.insert(qualifiedTKey);
        m_pendingPosterFetches.remove(posterKey(task.url));
        m_pendingPosterFetches.remove(titleKey(task.originalTitle));
        m_pendingPosterFetches.remove(qualifiedTKey);
        return;
    }

    const QJsonArray metas = doc.object().value(QStringLiteral("metas")).toArray();
    if (metas.isEmpty()) {
        // Retry alternative type (e.g. series instead of movie) once if not yet done
        if (!task.retriedOtherType) {
            PosterFetchTask retryTask = task;
            retryTask.retriedOtherType = true;
            retryTask.type = (task.type == QLatin1String("movie")) ? QStringLiteral("series") : QStringLiteral("movie");
            m_posterQueue.prepend(retryTask);
            return;
        }
        m_failedPosterFetches.insert(posterKey(task.url));
        if (!task.originalTitle.isEmpty()) m_failedPosterFetches.insert(titleKey(task.originalTitle));
        m_failedPosterFetches.insert(qualifiedTKey);
        m_pendingPosterFetches.remove(posterKey(task.url));
        m_pendingPosterFetches.remove(titleKey(task.originalTitle));
        m_pendingPosterFetches.remove(qualifiedTKey);
        return;
    }

    m_pendingPosterFetches.remove(posterKey(task.url));
    m_pendingPosterFetches.remove(titleKey(task.originalTitle));
    m_pendingPosterFetches.remove(qualifiedTKey);

    // STRICT CANDIDATE MATCHING
    // Directive: "no banner is 100 times better than an incorrect banner"
    struct Candidate {
        QString id;
        QString name;
        QString poster;
        QString releaseInfo;
        QString year;
    };

    QList<Candidate> titleMatches;
    for (const QJsonValue &v : metas) {
        const QJsonObject obj = v.toObject();
        const QString name = obj.value(QStringLiteral("name")).toString().trimmed();
        const QString poster = obj.value(QStringLiteral("poster")).toString().trimmed();
        if (poster.isEmpty() || name.isEmpty()) continue;

        if (isTitleMatch(task.cleanTitle, name)) {
            Candidate c;
            c.id = obj.value(QStringLiteral("id")).toString();
            c.name = name;
            c.poster = poster;
            c.releaseInfo = obj.value(QStringLiteral("releaseInfo")).toString().trimmed();
            c.year = obj.value(QStringLiteral("year")).toVariant().toString().trimmed();
            titleMatches.append(c);
        }
    }

    QString chosenPoster;

    if (!titleMatches.isEmpty()) {
        if (!task.year.isEmpty()) {
            // Find candidate that matches the requested year in releaseInfo or year
            for (const Candidate &c : titleMatches) {
                if ((!c.releaseInfo.isEmpty() && c.releaseInfo.contains(task.year)) ||
                    (!c.year.isEmpty() && c.year.contains(task.year))) {
                    chosenPoster = c.poster;
                    break;
                }
            }
        } else {
            // No year provided: only accept if completely unambiguous (exactly 1 title match)
            if (titleMatches.size() == 1) {
                chosenPoster = titleMatches.first().poster;
            }
        }

        // Disambiguate Kingdom: Korean series tt6611916 (2019-2020)
        if (chosenPoster.isEmpty() && task.cleanTitle.compare(QLatin1String("Kingdom"), Qt::CaseInsensitive) == 0 &&
            (task.year == QLatin1String("2019") || task.url.contains(QLatin1String("korean"), Qt::CaseInsensitive) || task.originalTitle.contains(QLatin1String("korean"), Qt::CaseInsensitive))) {
            for (const Candidate &c : titleMatches) {
                if (c.id == QLatin1String("tt6611916") || c.releaseInfo.contains(QLatin1String("2019"))) {
                    chosenPoster = c.poster;
                    break;
                }
            }
        }
    }

    if (!chosenPoster.isEmpty()) {
        // Upgrade IMDb poster resolution to SX500 for crisp card artwork
        static const QRegularExpression imdbResRe(QStringLiteral("@\\._V1_.*\\.jpg$"));
        chosenPoster.replace(imdbResRe, QStringLiteral("@._V1_SX500.jpg"));

        rememberPoster(task.url, chosenPoster, task.originalTitle);

        const SeriesLocation loc = seriesLocation(task.url);
        if (!loc.url.isEmpty()) {
            rememberPoster(loc.url, chosenPoster, loc.title);
        }

        if (!task.cleanTitle.isEmpty() && !task.year.isEmpty()) {
            rememberPoster(QString(), chosenPoster, task.cleanTitle + QStringLiteral(" ") + task.year);
        }
    } else {
        m_failedPosterFetches.insert(posterKey(task.url));
        if (!task.originalTitle.isEmpty()) m_failedPosterFetches.insert(titleKey(task.originalTitle));
        m_failedPosterFetches.insert(qualifiedTKey);
    }
}

void UserStore::handleTmdbResponse(const PosterFetchTask &task, const QByteArray &data) {
    const QString qualifiedTKey = titleKey(task.year.isEmpty() ? task.cleanTitle : (task.cleanTitle + QStringLiteral(" ") + task.year));
    m_pendingPosterFetches.remove(posterKey(task.url));
    m_pendingPosterFetches.remove(titleKey(task.originalTitle));
    m_pendingPosterFetches.remove(qualifiedTKey);

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        m_failedPosterFetches.insert(posterKey(task.url));
        if (!task.originalTitle.isEmpty()) m_failedPosterFetches.insert(titleKey(task.originalTitle));
        m_failedPosterFetches.insert(qualifiedTKey);
        return;
    }

    const QJsonArray results = doc.object().value(QStringLiteral("results")).toArray();
    struct Candidate {
        QString name;
        QString poster;
        QString year;
    };

    QList<Candidate> titleMatches;
    for (const QJsonValue &v : results) {
        const QJsonObject obj = v.toObject();
        const QString posterPath = obj.value(QStringLiteral("poster_path")).toString().trimmed();
        if (posterPath.isEmpty()) continue;

        const QString title = obj.contains(QStringLiteral("title"))
            ? obj.value(QStringLiteral("title")).toString().trimmed()
            : obj.value(QStringLiteral("name")).toString().trimmed();
        if (title.isEmpty()) continue;

        if (isTitleMatch(task.cleanTitle, title)) {
            Candidate c;
            c.name = title;
            c.poster = QStringLiteral("https://image.tmdb.org/t/p/w500") + posterPath;
            const QString dateStr = obj.contains(QStringLiteral("release_date"))
                ? obj.value(QStringLiteral("release_date")).toString()
                : obj.value(QStringLiteral("first_air_date")).toString();
            if (dateStr.length() >= 4) {
                c.year = dateStr.left(4);
            }
            titleMatches.append(c);
        }
    }

    QString chosenPoster;
    if (!titleMatches.isEmpty()) {
        if (!task.year.isEmpty()) {
            for (const Candidate &c : titleMatches) {
                if (c.year == task.year) {
                    chosenPoster = c.poster;
                    break;
                }
            }
        } else if (titleMatches.size() == 1) {
            chosenPoster = titleMatches.first().poster;
        }
    }

    if (!chosenPoster.isEmpty()) {
        rememberPoster(task.url, chosenPoster, task.originalTitle);
        const SeriesLocation loc = seriesLocation(task.url);
        if (!loc.url.isEmpty()) {
            rememberPoster(loc.url, chosenPoster, loc.title);
        }
        if (!task.cleanTitle.isEmpty() && !task.year.isEmpty()) {
            rememberPoster(QString(), chosenPoster, task.cleanTitle + QStringLiteral(" ") + task.year);
        }
    } else {
        m_failedPosterFetches.insert(posterKey(task.url));
        if (!task.originalTitle.isEmpty()) m_failedPosterFetches.insert(titleKey(task.originalTitle));
        m_failedPosterFetches.insert(qualifiedTKey);
    }
}

// ----------------------------------------------------------------------------
// Persistence
// ----------------------------------------------------------------------------

void UserStore::scheduleSave() {
    if (!m_saveTimer.isActive()) m_saveTimer.start();
}

void UserStore::loadHistory() {
    QFile file(m_historyPath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) return;

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isArray()) {
        FLUX_LOG_WARN("UserStore", "history.json is unreadable; starting with empty history");
        return;
    }

    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject()) continue;
        const QJsonObject o = v.toObject();
        HistoryEntry e;
        e.url = o.value("url").toString();
        if (e.url.isEmpty()) continue;
        e.title = o.value("title").toString();
        e.positionMs = static_cast<qint64>(o.value("positionMs").toDouble());
        e.durationMs = static_cast<qint64>(o.value("durationMs").toDouble());
        e.lastPlayed = static_cast<qint64>(o.value("lastPlayed").toDouble());
        e.watched = o.value("watched").toBool();
        m_history.insert(keyFor(e.url), e);
    }
}

void UserStore::writeHistory() {
    if (!m_dirty) return;

    // Keep the file bounded: drop the oldest entries first
    if (m_history.size() > kMaxHistoryEntries) {
        std::vector<std::pair<qint64, QString>> order;
        order.reserve(static_cast<size_t>(m_history.size()));
        for (auto it = m_history.constBegin(); it != m_history.constEnd(); ++it) {
            order.emplace_back(it.value().lastPlayed, it.key());
        }
        std::sort(order.begin(), order.end());
        const int excess = static_cast<int>(m_history.size()) - kMaxHistoryEntries;
        int removed = 0;
        for (const auto &entry : order) {
            if (removed >= excess) break;
            if (entry.second == m_currentKey) continue;
            m_history.remove(entry.second);
            ++removed;
        }
    }

    QJsonArray arr;
    for (auto it = m_history.constBegin(); it != m_history.constEnd(); ++it) {
        const HistoryEntry &e = it.value();
        QJsonObject o;
        o["url"] = e.url;
        o["title"] = e.title;
        o["positionMs"] = static_cast<double>(e.positionMs);
        o["durationMs"] = static_cast<double>(e.durationMs);
        o["lastPlayed"] = static_cast<double>(e.lastPlayed);
        o["watched"] = e.watched;
        arr.append(o);
    }

    QSaveFile file(m_historyPath);
    if (!file.open(QIODevice::WriteOnly)) {
        FLUX_LOG_WARN("UserStore", "Could not write history file: " + m_historyPath);
        scheduleSave();
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    if (file.commit()) {
        m_dirty = false;
    } else {
        FLUX_LOG_WARN("UserStore", "Could not commit history file: " + m_historyPath);
        scheduleSave();
    }
}

void UserStore::shutdown() {
    m_tickTimer.stop();
    checkpoint(false);
    writeHistory();
    m_posterSaveTimer.stop();
    savePosters();
    m_posterQueue.clear();
    m_pendingPosterFetches.clear();
    m_settings.sync();
}

static QString readDevPinFromEnv() {
    // 1. Check environment variable first
    const QString envVar = qEnvironmentVariable("DEV_PIN").trimmed();
    if (!envVar.isEmpty()) return envVar;
    const QString envVarLower = qEnvironmentVariable("dev_pin").trimmed();
    if (!envVarLower.isEmpty()) return envVarLower;

    // 2. Check .env file in multiple possible runtime directories
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidatePaths = {
        QDir(appDir).filePath(QStringLiteral(".env")),
        QDir(appDir + QStringLiteral("/..")).filePath(QStringLiteral(".env")),
        QDir(appDir + QStringLiteral("/../..")).filePath(QStringLiteral(".env")),
        QDir::current().filePath(QStringLiteral(".env"))
    };

    for (const QString &path : candidatePaths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();
                if (line.startsWith(QLatin1Char('#')) || line.isEmpty()) continue;
                if (line.startsWith(QLatin1String("dev_pin="), Qt::CaseInsensitive)) {
                    QString val = line.mid(8).trimmed();
                    if ((val.startsWith(QLatin1Char('"')) && val.endsWith(QLatin1Char('"'))) ||
                        (val.startsWith(QLatin1Char('\'')) && val.endsWith(QLatin1Char('\'')))) {
                        val = val.mid(1, val.length() - 2);
                    }
                    if (!val.isEmpty()) return val;
                }
            }
        }
    }
    return QString();
}

bool UserStore::devModeUnlocked() const {
    return m_devUnlocked;
}

void UserStore::setDevModeUnlocked(bool unlocked) {
    if (m_devUnlocked == unlocked) return;
    m_devUnlocked = unlocked;
    if (!unlocked) {
        setUseCustomTelepartyCode(false);
        setCustomTelepartyCode(QString());
    }
    emit devModeUnlockedChanged();
}

QString UserStore::devPin() const {
    return readDevPinFromEnv();
}

void UserStore::setDevPin(const QString &pin) {
    Q_UNUSED(pin);
}

bool UserStore::verifyDevPin(const QString &pin) {
    const QString trimmed = pin.trimmed();
    const QString envPin = readDevPinFromEnv();
    if (!envPin.isEmpty() && trimmed == envPin) {
        setDevModeUnlocked(true);
        return true;
    }
    return false;
}

bool UserStore::useCustomTelepartyCode() const {
    return m_settings.value(QStringLiteral("teleparty/useCustomCode"), false).toBool();
}

void UserStore::setUseCustomTelepartyCode(bool use) {
    if (useCustomTelepartyCode() == use) return;
    setValue(QStringLiteral("teleparty/useCustomCode"), use);
    emit customTelepartyCodeChanged();
}

QString UserStore::customTelepartyCode() const {
    return m_settings.value(QStringLiteral("teleparty/customCode"), QString()).toString();
}

void UserStore::setCustomTelepartyCode(const QString &code) {
    const QString trimmed = code.trimmed().toUpper();
    if (customTelepartyCode() == trimmed) return;
    setValue(QStringLiteral("teleparty/customCode"), trimmed);
    emit customTelepartyCodeChanged();
}

} // namespace Flux
