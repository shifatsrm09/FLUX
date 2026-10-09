#include "UserStore.h"
#include "Logger.h"
#include "../media/VLCPlayer.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUrl>
#include <algorithm>
#include <vector>

namespace Flux {

namespace {
constexpr qint64 kMinResumeMs = 30000;       // under 30s in: not worth resuming
constexpr double kWatchedFraction = 0.93;    // past 93% counts as watched
constexpr int kMaxHistoryEntries = 300;
constexpr int kMaxContinueEntries = 24;
}

UserStore::UserStore(QObject *parent)
    : QObject(parent)
    , m_settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FLUX"), QStringLiteral("FLUX")) {

    const QFileInfo settingsInfo(m_settings.fileName());
    QDir().mkpath(settingsInfo.absolutePath());
    m_historyPath = settingsInfo.absolutePath() + QStringLiteral("/history.json");

    loadHistory();

    m_tickTimer.setInterval(5000);
    connect(&m_tickTimer, &QTimer::timeout, this, [this]() { checkpoint(false); });

    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(4000);
    connect(&m_saveTimer, &QTimer::timeout, this, [this]() { writeHistory(); });

    m_settingsTimer.setSingleShot(true);
    m_settingsTimer.setInterval(1000);
    connect(&m_settingsTimer, &QTimer::timeout, this, [this]() { m_settings.sync(); });

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

// ----------------------------------------------------------------------------
// Player tracking
// ----------------------------------------------------------------------------

void UserStore::attachPlayer(VLCPlayer *player) {
    m_player = player;
    if (!m_player) return;
    connect(m_player, &VLCPlayer::stateChanged, this, &UserStore::onPlayerStateChanged);
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
        if (it != m_history.end() && keyFor(m_player->url()) == m_currentKey) {
            it->watched = true;
            it->positionMs = 0;
            it->lastPlayed = QDateTime::currentMSecsSinceEpoch();
            ++m_revision;
            emit historyChanged();
            m_dirty = true;
            scheduleSave();
        }
        return;
    }

    if (st == QLatin1String("Paused") || st == QLatin1String("Stopped") || st == QLatin1String("Error")) {
        m_tickTimer.stop();
        checkpoint(true);
    }
}

void UserStore::checkpoint(bool notify) {
    if (!m_player || m_currentKey.isEmpty()) return;
    // Only record progress for the file this entry actually belongs to
    if (keyFor(m_player->url()) != m_currentKey) return;

    const qint64 pos = m_player->timeMs();
    const qint64 dur = m_player->durationMs();
    // stop() zeroes the clock; ignore it so a restart never wipes saved progress
    if (pos <= 0 || dur <= 0) return;

    auto it = m_history.find(m_currentKey);
    if (it == m_history.end()) return;

    HistoryEntry &e = it.value();
    e.durationMs = dur;
    e.lastPlayed = QDateTime::currentMSecsSinceEpoch();

    if (pos >= static_cast<qint64>(static_cast<double>(dur) * kWatchedFraction)) {
        e.watched = true;
        e.positionMs = 0;
    } else {
        e.positionMs = pos;
        if (pos >= kMinResumeMs) e.watched = false;   // being watched again
    }

    m_dirty = true;
    scheduleSave();

    if (notify) {
        ++m_revision;
        emit historyChanged();
    }
}

void UserStore::noteStart(const QString &url, const QString &title) {
    const QString key = keyFor(url);
    if (key.isEmpty()) return;

    // Capture the previous file's final position before switching to the new one
    checkpoint(false);

    HistoryEntry &e = m_history[key];
    e.url = url;
    e.title = nameFromUrl(url);
    if (e.title.isEmpty()) e.title = title;
    e.lastPlayed = QDateTime::currentMSecsSinceEpoch();

    m_currentKey = key;
    m_dirty = true;
    scheduleSave();

    ++m_revision;
    emit historyChanged();
}

// ----------------------------------------------------------------------------
// Queries
// ----------------------------------------------------------------------------

QVariantMap UserStore::progressFor(const QString &url) const {
    QVariantMap map;
    const auto it = m_history.constFind(keyFor(url));
    if (it == m_history.constEnd()) return map;

    const HistoryEntry &e = it.value();
    const double fraction = (e.durationMs > 0 && e.positionMs > 0)
                            ? static_cast<double>(e.positionMs) / static_cast<double>(e.durationMs)
                            : 0.0;
    map["positionMs"] = e.positionMs;
    map["durationMs"] = e.durationMs;
    map["fraction"] = fraction;
    map["watched"] = e.watched;
    return map;
}

qint64 UserStore::resumePositionFor(const QString &url) const {
    const auto it = m_history.constFind(keyFor(url));
    if (it == m_history.constEnd()) return 0;

    const HistoryEntry &e = it.value();
    if (e.watched || e.durationMs <= 0) return 0;
    if (e.positionMs < kMinResumeMs) return 0;
    if (e.positionMs >= static_cast<qint64>(static_cast<double>(e.durationMs) * kWatchedFraction)) return 0;
    return e.positionMs;
}

QVariantList UserStore::continueWatching() const {
    std::vector<const HistoryEntry *> items;
    for (auto it = m_history.constBegin(); it != m_history.constEnd(); ++it) {
        const HistoryEntry &e = it.value();
        if (e.watched || e.durationMs <= 0) continue;
        if (e.positionMs < kMinResumeMs) continue;
        if (e.positionMs >= static_cast<qint64>(static_cast<double>(e.durationMs) * kWatchedFraction)) continue;
        items.push_back(&e);
    }

    std::sort(items.begin(), items.end(), [](const HistoryEntry *a, const HistoryEntry *b) {
        return a->lastPlayed > b->lastPlayed;
    });

    QVariantList list;
    for (const HistoryEntry *e : items) {
        if (list.size() >= kMaxContinueEntries) break;
        QVariantMap m;
        m["url"] = e->url;
        m["title"] = e->title;
        m["positionMs"] = e->positionMs;
        m["durationMs"] = e->durationMs;
        m["fraction"] = static_cast<double>(e->positionMs) / static_cast<double>(e->durationMs);
        m["lastPlayed"] = e->lastPlayed;
        list.append(m);
    }
    return list;
}

// ----------------------------------------------------------------------------
// Mutations
// ----------------------------------------------------------------------------

void UserStore::removeFromHistory(const QString &url) {
    if (m_history.remove(keyFor(url)) > 0) {
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
    if (watched) it->positionMs = 0;
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
        for (int i = 0; i < excess; ++i) {
            if (order[static_cast<size_t>(i)].second != m_currentKey) {
                m_history.remove(order[static_cast<size_t>(i)].second);
            }
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
        return;
    }
    file.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    if (file.commit()) {
        m_dirty = false;
    }
}

void UserStore::shutdown() {
    m_tickTimer.stop();
    checkpoint(false);
    writeHistory();
    m_settings.sync();
}

} // namespace Flux
