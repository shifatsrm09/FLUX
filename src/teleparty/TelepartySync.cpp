#include "TelepartySync.h"
#include "TelepartySession.h"
#include "../media/VLCPlayer.h"
#include "../core/Logger.h"
#include "../core/UserStore.h"

#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Flux {

namespace {

constexpr int kMaxUrlLength = 2048;
constexpr int kMaxTitleLength = 300;
constexpr qint64 kOpenDedupeMs = 8000;       // same video opened twice within this = one open
constexpr int kSeekMergeMs = 40;             // VLCPlayer already coalesces local seeks; keep this minimal
constexpr int kStateWaitMs = 5000;           // how long we accept a "state" answer after asking
// Drift correction is a safety net, not a frame-by-frame synchronizer. A slower
// cadence avoids repeatedly touching VLC while network streams are buffering.
constexpr int kDriftTickIntervalMs = 30000;
constexpr qint64 kCatchUpDriftMs = 650;      // initial join catch-up seek threshold
constexpr qint64 kSoftLogCooldownMs = 2000;  // rate-limit soft drift logs
constexpr qint64 kReadinessMinIntervalMs = 800;

struct BuiltinCatalogEntry {
    const char *id;
    const char *title;
    const char *url;
};

constexpr BuiltinCatalogEntry kBuiltinCatalog[] = {
    {
        "ant-man",
        "Ant-Man and the Wasp: Quantumania",
        "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/Ant-Man%20and%20the%20Wasp-Quantumania%20%282023%29%201080p%20DSNP/Ant-Man%20and%20the%20Wasp%20Quantumania%20%282023%29%201080p%20DSNP-WEB%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-PSA.mkv"
    },
    {
        "civil-war",
        "Civil War",
        "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282024%29%201080p/Civil%20War%20%282024%29%201080p%20%5BDual%20Audio%5D/Civil%20War%20%282024%29%201080p%20BluRay%20x265%20HEVC%20ESub%20%5BDual%20Audio%5D%5BHindi%205.1%2BEnglish%205.1%5D%20-mkvC.mkv"
    },
    {
        "kingdom-s1e1",
        "Kingdom S1E1",
        "http://172.16.50.14/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/Kingdom%20%28TV%20Series%202019%E2%80%93%20%29%201080p%20%5BDual%20Audio%5D/Season%201/Kingdom%20S01E01%20%201080p%20NF%20WEBRip%20x265%20HEVC%20MSubs%20%5BDual%20Audio%5D%5BEnglish%205.1%2BKorean%205.1%5D%20-OlaM.mkv"
    }
};

qint64 cleanTime(const QVariant &v) {
    // Positions come from other machines: never trust them blindly
    const qint64 t = v.toLongLong();
    return AuthoritativeTimeline::sanitizePositionMs(t);
}

bool hasUnsafeCharactersOrTraversal(const QString &str) {
    if (str.contains(QLatin1Char('\\'))) return true;
    for (const QChar &ch : str) {
        if (ch.unicode() < 0x20 || ch.unicode() == 0x7F) return true;
    }
    const QString decoded = QUrl::fromPercentEncoding(str.toUtf8());
    if (decoded.contains(QLatin1Char('\\'))) return true;
    const QStringList segments = decoded.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    for (const QString &seg : segments) {
        if (seg == QLatin1String(".") || seg == QLatin1String("..")) {
            return true;
        }
    }
    return false;
}

bool isBuiltinCatalogId(const QString &id) {
    for (const auto &entry : kBuiltinCatalog) {
        if (id == QLatin1String(entry.id)) {
            return true;
        }
    }
    return false;
}

QString deriveTitleFromPath(const QString &relPath) {
    const QString decoded = QUrl::fromPercentEncoding(relPath.toUtf8());
    const int slash = decoded.lastIndexOf(QLatin1Char('/'));
    QString name = (slash >= 0) ? decoded.mid(slash + 1) : decoded;
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot > 0) {
        name = name.left(dot);
    }
    return name.trimmed();
}

} // namespace

TelepartySync::TelepartySync(TelepartySession *session, VLCPlayer *player, QObject *parent)
    : QObject(parent)
    , m_session(session)
    , m_player(player)
    , m_catalog(this)
    , m_clock(std::make_shared<SystemMonotonicClock>()) {

    // Prefer uninterrupted playback. Only make a corrective seek after a clear,
    // sustained offset; never rate-nudge streams, which can trigger rebuffering.
    AdaptiveDriftController::Config driftConfig = m_driftController.config();
    driftConfig.deadbandMs = 10000;
    driftConfig.softSeekThresholdMs = 10000;
    driftConfig.hardSeekThresholdMs = 10000;
    driftConfig.pausedSeekThresholdMs = 10000;
    driftConfig.fallbackSeekThresholdMs = 10000;
    m_driftController.setConfig(driftConfig);

    m_stateTimer.setSingleShot(true);
    connect(&m_stateTimer, &QTimer::timeout, this, [this]() { m_needState = false; });

    m_seekTimer.setSingleShot(true);
    m_seekTimer.setInterval(kSeekMergeMs);
    connect(&m_seekTimer, &QTimer::timeout, this, &TelepartySync::flushSeek);

    m_driftTimer.setInterval(kDriftTickIntervalMs);
    connect(&m_driftTimer, &QTimer::timeout, this, &TelepartySync::evaluateDriftTick);

    m_clockSyncTimer.setInterval(1500);
    connect(&m_clockSyncTimer, &QTimer::timeout, this, &TelepartySync::sendClockPing);

    m_scheduleTimer.setSingleShot(true);
    connect(&m_scheduleTimer, &QTimer::timeout, this, &TelepartySync::processScheduledActions);

    m_localReadinessTimeoutTimer.setSingleShot(true);
    connect(&m_localReadinessTimeoutTimer, &QTimer::timeout, this, &TelepartySync::onLocalReadinessTimeout);

    // Session -> player
    if (m_session) {
        connect(m_session, &TelepartySession::eventReceived, this, &TelepartySync::onEvent);
        connect(m_session, &TelepartySession::stateChanged, this, &TelepartySync::onSessionStateChanged);
        connect(m_session, &TelepartySession::authorityChanged, this, &TelepartySync::onAuthorityChanged);
        connect(m_session, &TelepartySession::memberLeft, this, [this](const QString &peerId) {
            m_peerReadiness.remove(peerId);
            if (m_peerLocalStatus.remove(peerId)) {
                emit localVerificationChanged();
            }
            if (m_mediaMode == TelepartyMediaMode::LocalFile && peerId == m_localMediaAuthorityPeerId) {
                // Media authority departed; failover to session host
                m_localMediaAuthorityPeerId = m_session->hostMemberId();
                if (m_session->isHost()) {
                    m_localMediaAuthorityPeerId = m_session->memberId();
                    sendState();
                    tryCommitLocalPlaybackStart();
                }
                emit localVerificationChanged();
            } else if (m_localMediaAuthorityPeerId == m_session->memberId()) {
                tryCommitLocalPlaybackStart();
            }
        });
        connect(m_session, &TelepartySession::memberJoined, this, [this](const QString &peerId) {
            if (m_mediaMode == TelepartyMediaMode::LocalFile && m_hostLocalDescriptor.isValidForSession()) {
                if (!m_peerLocalStatus.contains(peerId)) {
                    ParticipantLocalStatus st;
                    st.peerId = peerId;
                    st.mediaId = m_hostLocalDescriptor.mediaId;
                    st.revision = m_hostLocalDescriptor.revision;
                    st.state = LocalVerificationState::WaitingForLocalSelection;
                    st.reason = LocalCompatibilityReason::None;
                    st.updatedAtLocalMs = localNowMs();
                    m_peerLocalStatus.insert(peerId, st);
                    emit localVerificationChanged();
                }
            }
            // Someone (re)joined: tell them what we are watching
            sendState();
        });
        connect(m_session, &TelepartySession::joinedSession, this, [this]() {
            resetParty();
            m_clockEstimator.reset(m_session->isHost(), m_session->hostMemberId());

            if (m_player) {
                m_driftTimer.start();
                if (!m_session->isHost()) {
                    m_clockSyncTimer.start(m_clockEstimator.recommendedPingIntervalMs());
                    sendClockPing();
                }
            }

            // A newcomer learns what the party is watching from the members' answers
            if (!m_session->isHost()) {
                armNeedState();
                m_session->sendEvent(QStringLiteral("syncme"));
            }
        });
    }

    // Player -> session
    if (m_player) {
        connect(m_player, &VLCPlayer::userToggledPlay, this, &TelepartySync::onUserToggledPlay);
        connect(m_player, &VLCPlayer::userSeeked, this, &TelepartySync::onUserSeeked);
        connect(m_player, &VLCPlayer::stateChanged, this, &TelepartySync::onPlayerStateChanged);
        connect(m_player, &VLCPlayer::bufferingChanged, this, &TelepartySync::onPlayerBufferingChanged);
    }
}

TelepartySync::~TelepartySync() {
    cancelActiveInspection();
}

void TelepartySync::setMonotonicClock(std::shared_ptr<MonotonicClock> clock) {
    if (clock) {
        m_clock = std::move(clock);
    } else {
        m_clock = std::make_shared<SystemMonotonicClock>();
    }
}

qint64 TelepartySync::localNowMs() const {
    return m_clock ? m_clock->nowMs() : 0;
}

qint64 TelepartySync::estimatedHostNowMs() const {
    return m_clockEstimator.estimatedHostTimeMs(localNowMs());
}

void TelepartySync::setUserStore(UserStore *store) {
    m_userStore = store;
}

QString TelepartySync::iccftpBaseOrigin() const {
    if (m_userStore) {
        QUrl u(m_userStore->iccftpUrl(), QUrl::TolerantMode);
        if (u.isValid() && !u.host().isEmpty()) {
            const int port = u.port();
            QString s = u.scheme() + QStringLiteral("://") + u.host();
            if (port > 0 && port != 80 && port != 443) s += QStringLiteral(":") + QString::number(port);
            return s;
        }
    }
    return QStringLiteral("http://10.16.100.244");
}

QString TelepartySync::iccftpSessionToken() const {
    if (m_userStore) {
        return m_userStore->iccftpSession();
    }
    return QStringLiteral("b1bdfdae1f7e206b628274ef7eafadcd6094e07c46cd85b157c595174a369f96");
}

QString TelepartySync::iccftpHost() const {
    if (m_userStore) {
        QUrl u(m_userStore->iccftpUrl(), QUrl::TolerantMode);
        if (u.isValid() && !u.host().isEmpty()) {
            return u.host();
        }
    }
    return QStringLiteral("10.16.100.244");
}

bool TelepartySync::isSameOrMatchingMedia(const QString &playerUrl, const QString &currentUrl) const {
    if (playerUrl.isEmpty() || currentUrl.isEmpty()) return false;
    if (playerUrl == currentUrl) return true;

    const QString decPlayer = QUrl::fromPercentEncoding(playerUrl.trimmed().toUtf8());
    const QString decCurr = QUrl::fromPercentEncoding(currentUrl.trimmed().toUtf8());
    if (decPlayer.compare(decCurr, Qt::CaseInsensitive) == 0) return true;

    // Both contain player.php with the same play ID
    if (playerUrl.contains(QLatin1String("player.php")) && currentUrl.contains(QLatin1String("player.php"))) {
        static const QRegularExpression playRx(QStringLiteral("[?&]play=(\\d+)"));
        const auto mp = playRx.match(playerUrl);
        const auto mc = playRx.match(currentUrl);
        if (mp.hasMatch() && mc.hasMatch() && mp.captured(1) == mc.captured(1)) {
            return true;
        }
    }

    // Direct node url vs player.php URL
    if (m_userStore) {
        if (currentUrl.contains(QLatin1String("player.php"))) {
            static const QRegularExpression playRx(QStringLiteral("[?&]play=(\\d+)"));
            const auto mc = playRx.match(currentUrl);
            if (mc.hasMatch()) {
                const QString mapped = m_userStore->seriesFolderFor(playerUrl);
                if (mapped.contains(QStringLiteral("play=") + mc.captured(1))) return true;
            }
        }
        if (playerUrl.contains(QLatin1String("player.php"))) {
            static const QRegularExpression playRx(QStringLiteral("[?&]play=(\\d+)"));
            const auto mp = playRx.match(playerUrl);
            if (mp.hasMatch()) {
                const QString mapped = m_userStore->seriesFolderFor(currentUrl);
                if (mapped.contains(QStringLiteral("play=") + mp.captured(1))) return true;
            }
        }
    }

    auto isLocalUrl = [](const QString &u) -> bool {
        return u.startsWith(QLatin1String("file:"), Qt::CaseInsensitive) ||
               (!u.startsWith(QLatin1String("http:"), Qt::CaseInsensitive) &&
                !u.startsWith(QLatin1String("https:"), Qt::CaseInsensitive) &&
                !u.startsWith(QLatin1String("ftp:"), Qt::CaseInsensitive) &&
                !u.contains(QLatin1String("://")));
    };

    auto toCleanLocalPath = [](const QString &u) -> QString {
        QString path = u.trimmed();
        if (path.startsWith(QLatin1String("file:"), Qt::CaseInsensitive)) {
            path = QUrl(path).toLocalFile();
        }
        return QFileInfo(path).absoluteFilePath();
    };

    if (isLocalUrl(playerUrl) && isLocalUrl(currentUrl)) {
        if (toCleanLocalPath(playerUrl).compare(toCleanLocalPath(currentUrl), Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    QString localCandidate;
    QString remoteCandidate;
    if (isLocalUrl(playerUrl) && !isLocalUrl(currentUrl)) {
        localCandidate = toCleanLocalPath(playerUrl);
        remoteCandidate = currentUrl;
    } else if (isLocalUrl(currentUrl) && !isLocalUrl(playerUrl)) {
        localCandidate = toCleanLocalPath(currentUrl);
        remoteCandidate = playerUrl;
    }

    if (!localCandidate.isEmpty() && !remoteCandidate.isEmpty()) {
        QFileInfo localFi(localCandidate);

        // 1. Check QSettings for recorded completed download
        QSettings settings;
        const QString dlKey = QString::fromLatin1(QCryptographicHash::hash(remoteCandidate.toUtf8(), QCryptographicHash::Sha256).toHex());
        const QString recordedPath = settings.value(QStringLiteral("downloads/completed/") + dlKey).toString();
        if (!recordedPath.isEmpty() && QFileInfo(recordedPath).exists()) {
            if (QFileInfo(recordedPath).absoluteFilePath().compare(localCandidate, Qt::CaseInsensitive) == 0) {
                return true;
            }
        }

        // 2. Check MediaCache hash naming
        const QByteArray remoteHash = QCryptographicHash::hash(QUrl(remoteCandidate).toString(QUrl::FullyEncoded).toUtf8(),
                                                               QCryptographicHash::Sha256).toHex();
        if (localFi.fileName().startsWith(QString::fromLatin1(remoteHash), Qt::CaseInsensitive)) {
            return true;
        }

        // 3. Filename matching for video streams
        QString remoteFileName = QUrl(remoteCandidate).fileName();
        if (remoteFileName.isEmpty()) {
            QString decoded = QUrl::fromPercentEncoding(remoteCandidate.toUtf8());
            const int slash = decoded.lastIndexOf(QLatin1Char('/'));
            if (slash >= 0) remoteFileName = decoded.mid(slash + 1);
        }
        const int qIdx = remoteFileName.indexOf(QLatin1Char('?'));
        if (qIdx >= 0) remoteFileName = remoteFileName.left(qIdx);

        static const QSet<QString> kVideoExts = {
            QStringLiteral("mkv"), QStringLiteral("mp4"), QStringLiteral("avi"),
            QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("m4v"),
            QStringLiteral("ts"), QStringLiteral("m2ts"), QStringLiteral("webm"),
            QStringLiteral("flv"), QStringLiteral("mpg"), QStringLiteral("mpeg")
        };

        const QString remoteSuffix = QFileInfo(remoteFileName).suffix().toLower();
        const QString localSuffix = localFi.suffix().toLower();
        if (kVideoExts.contains(remoteSuffix) && kVideoExts.contains(localSuffix)) {
            if (remoteFileName.compare(localFi.fileName(), Qt::CaseInsensitive) == 0) {
                return true;
            }
        }
    }

    return false;
}

// ============================================================================
// Catalog validation & compact movie IDs
// ============================================================================

QString TelepartySync::encodeCatalogMovieId(const QString &url) const {
    if (url.isEmpty() || url.size() > kMaxUrlLength) return QString();
    if (hasUnsafeCharactersOrTraversal(url)) return QString();

    const QUrl u(url);
    const QString scheme = u.scheme().toLower();
    if (!u.isValid() || (scheme != QLatin1String("http") && scheme != QLatin1String("https")) || u.host().isEmpty()) {
        return QString();
    }

    const QString host = u.host();
    const QString decodedUrl = QUrl::fromPercentEncoding(url.toUtf8());

    // 1. Check built-in catalog items for ultra-short IDs ("ant-man", "civil-war", "kingdom-s1e1")
    for (const auto &entry : kBuiltinCatalog) {
        const QString entryUrl = QString::fromLatin1(entry.url);
        if (url == entryUrl || decodedUrl == QUrl::fromPercentEncoding(entryUrl.toUtf8())) {
            return QString::fromLatin1(entry.id);
        }
    }

    // 2. ICC FTP: player.php?play=<id>
    if (url.contains(QLatin1String("player.php"))) {
        static const QRegularExpression playRx(QStringLiteral("[?&]play=(\\d+)"));
        const auto match = playRx.match(url);
        if (match.hasMatch()) {
            return QStringLiteral("icc:play:") + match.captured(1);
        }
    }

    // 3. ICC FTP: direct storage nodes (10.16.100.x or user-configured ICC FTP host)
    const QString configuredIccHost = iccftpHost();
    if (host.startsWith(QStringLiteral("10.16.100.")) ||
        (!configuredIccHost.isEmpty() && host.compare(configuredIccHost, Qt::CaseInsensitive) == 0)) {
        return QStringLiteral("icc:node:") + u.authority() + u.path(QUrl::FullyEncoded);
    }

    // 4. Match against the official BDIX catalog roots in MediaLibraryModel
    const MediaRoot *bestRoot = nullptr;
    QString bestRelPath;
    int bestRootLen = -1;

    for (const MediaRoot &root : m_catalog.roots()) {
        QString rootBase = root.url;
        if (!rootBase.endsWith(QLatin1Char('/'))) {
            rootBase += QLatin1Char('/');
        }

        if (url.startsWith(rootBase, Qt::CaseInsensitive)) {
            const QString rel = url.mid(rootBase.size());
            if (!rel.isEmpty() && static_cast<int>(rootBase.size()) > bestRootLen) {
                bestRoot = &root;
                bestRelPath = rel;
                bestRootLen = static_cast<int>(rootBase.size());
            }
            continue;
        }

        // Percent-decoded prefix match (handles e.g. "(1080p)" vs "%281080p%29" in server responses)
        const QString decodedRoot = QUrl::fromPercentEncoding(rootBase.toUtf8());
        if (decodedUrl.startsWith(decodedRoot, Qt::CaseInsensitive)) {
            const int slashCount = static_cast<int>(rootBase.count(QLatin1Char('/')));
            int slashesSeen = 0;
            int cutPos = -1;
            for (int i = 0; i < url.size(); ++i) {
                if (url.at(i) == QLatin1Char('/')) {
                    if (++slashesSeen == slashCount) {
                        cutPos = i + 1;
                        break;
                    }
                }
            }
            if (cutPos > 0 && cutPos < url.size()) {
                const QString rel = url.mid(cutPos);
                if (!rel.isEmpty() && static_cast<int>(decodedRoot.size()) > bestRootLen) {
                    bestRoot = &root;
                    bestRelPath = rel;
                    bestRootLen = static_cast<int>(decodedRoot.size());
                }
            }
        }
    }

    if (bestRoot && !bestRelPath.isEmpty()) {
        return bestRoot->id + QLatin1Char(':') + bestRelPath;
    }

    // 5. Dhaka Flix direct node fallback (e.g. 172.16.50.x)
    if (host.startsWith(QStringLiteral("172.16.50."))) {
        return QStringLiteral("df:node:") + u.authority() + u.path(QUrl::FullyEncoded);
    }

    // 6. Generic BDIX stream URL fallback (for any video file on BDIX / LAN storage)
    const bool isBdixHost = host.startsWith(QStringLiteral("10.")) ||
                            host.startsWith(QStringLiteral("172.")) ||
                            host.startsWith(QStringLiteral("192.168.")) ||
                            host.contains(QStringLiteral("bdix"), Qt::CaseInsensitive) ||
                            host.contains(QStringLiteral("dhaka"), Qt::CaseInsensitive) ||
                            host.contains(QStringLiteral("icc"), Qt::CaseInsensitive);

    if (isBdixHost || url.contains(QLatin1String("/ftps")) || url.contains(QLatin1String("DHAKA-FLIX"))) {
        if (url.endsWith(QStringLiteral(".mp4"), Qt::CaseInsensitive) ||
            url.endsWith(QStringLiteral(".mkv"), Qt::CaseInsensitive) ||
            url.endsWith(QStringLiteral(".avi"), Qt::CaseInsensitive) ||
            url.endsWith(QStringLiteral(".webm"), Qt::CaseInsensitive) ||
            url.endsWith(QStringLiteral(".ts"), Qt::CaseInsensitive) ||
            url.endsWith(QStringLiteral(".m4v"), Qt::CaseInsensitive)) {
            return QStringLiteral("bdix:") + u.authority() + u.path(QUrl::FullyEncoded);
        }
    }

    return QString();
}

bool TelepartySync::resolveCatalogMovie(const QVariantMap &data, QString &outUrl, QString &outTitle) const {
    QString id = data.value(QStringLiteral("id")).toString().trimmed();
    QString providedTitle = data.value(QStringLiteral("n")).toString().trimmed();
    if (providedTitle.isEmpty()) {
        providedTitle = data.value(QStringLiteral("title")).toString().trimmed();
    }

    if (id.isEmpty()) {
        const QString rawUrl = data.value(QStringLiteral("url")).toString().trimmed();
        if (!rawUrl.isEmpty()) {
            id = encodeCatalogMovieId(rawUrl);
        }
    }

    if (id.isEmpty() || id.size() > kMaxUrlLength) {
        return false;
    }

    // 1. Check built-in short movie IDs
    for (const auto &entry : kBuiltinCatalog) {
        if (id == QLatin1String(entry.id)) {
            outUrl = QString::fromLatin1(entry.url);
            outTitle = (providedTitle.isEmpty() ? QString::fromLatin1(entry.title) : providedTitle)
                           .left(kMaxTitleLength);
            return true;
        }
    }

    // 2. ICC FTP player.php IDs: "icc:play:<id>" or legacy "icc:<id>"
    if (id.startsWith(QLatin1String("icc:play:")) ||
        (id.startsWith(QLatin1String("icc:")) && !id.startsWith(QLatin1String("icc:node:")))) {
        const QString playId = id.startsWith(QLatin1String("icc:play:")) ? id.mid(9) : id.mid(4);
        if (!playId.isEmpty()) {
            const QString base = iccftpBaseOrigin();
            const QString session = iccftpSessionToken();
            outUrl = base + QStringLiteral("/player.php?play=") + playId;
            if (!session.isEmpty()) {
                outUrl += QStringLiteral("&session=") + session;
            }
            outTitle = (providedTitle.isEmpty() ? QStringLiteral("ICC FTP Video") : providedTitle).left(kMaxTitleLength);
            return true;
        }
    }

    // 3. ICC FTP direct node: "icc:node:<authority><path>"
    if (id.startsWith(QLatin1String("icc:node:"))) {
        const QString remainder = id.mid(9);
        if (!remainder.isEmpty() && !hasUnsafeCharactersOrTraversal(remainder)) {
            outUrl = QStringLiteral("http://") + remainder;
            const QString fallbackTitle = deriveTitleFromPath(QUrl(outUrl).path());
            outTitle = (providedTitle.isEmpty() ? fallbackTitle : providedTitle).left(kMaxTitleLength);
            return true;
        }
    }

    // 4. Dhaka Flix direct node: "df:node:<authority><path>"
    if (id.startsWith(QLatin1String("df:node:"))) {
        const QString remainder = id.mid(8);
        if (!remainder.isEmpty() && !hasUnsafeCharactersOrTraversal(remainder)) {
            outUrl = QStringLiteral("http://") + remainder;
            const QString fallbackTitle = deriveTitleFromPath(QUrl(outUrl).path());
            outTitle = (providedTitle.isEmpty() ? fallbackTitle : providedTitle).left(kMaxTitleLength);
            return true;
        }
    }

    // 5. Generic BDIX direct stream: "bdix:<authority><path>"
    if (id.startsWith(QLatin1String("bdix:"))) {
        const QString remainder = id.mid(5);
        if (!remainder.isEmpty() && !hasUnsafeCharactersOrTraversal(remainder)) {
            outUrl = QStringLiteral("http://") + remainder;
            const QString fallbackTitle = deriveTitleFromPath(QUrl(outUrl).path());
            outTitle = (providedTitle.isEmpty() ? fallbackTitle : providedTitle).left(kMaxTitleLength);
            return true;
        }
    }

    // 6. Validate "<rootId>:<relativePath>" against MediaLibraryModel
    const int colon = id.indexOf(QLatin1Char(':'));
    if (colon <= 0 || colon >= id.size() - 1) {
        return false;
    }

    const QString rootId = id.left(colon).trimmed();
    const QString relPath = id.mid(colon + 1).trimmed();

    const MediaRoot *root = m_catalog.findById(rootId);
    if (!root) {
        return false;
    }

    if (relPath.isEmpty() ||
        relPath.startsWith(QLatin1Char('/')) ||
        relPath.contains(QLatin1Char('?')) ||
        relPath.contains(QLatin1Char('#')) ||
        hasUnsafeCharactersOrTraversal(relPath)) {
        return false;
    }

    const QString decodedRel = QUrl::fromPercentEncoding(relPath.toUtf8());
    if (decodedRel.contains(QLatin1Char(':'))) {
        return false; // reject embedded drive letters or schemes
    }

    QString rootBase = root->url;
    if (!rootBase.endsWith(QLatin1Char('/'))) {
        rootBase += QLatin1Char('/');
    }

    const QString fullUrl = rootBase + relPath;
    const QUrl parsed(fullUrl);
    if (!parsed.isValid() || parsed.host().isEmpty()) {
        return false;
    }

    outUrl = fullUrl;
    const QString fallbackTitle = deriveTitleFromPath(relPath);
    outTitle = (providedTitle.isEmpty() ? fallbackTitle : providedTitle).left(kMaxTitleLength);
    return true;
}

// ============================================================================
// UI hooks
// ============================================================================

void TelepartySync::setWatching(bool watching) {
    if (m_watching == watching) return;
    m_watching = watching;
    emit watchingChanged();

    // Coming back to a video the party is on (e.g. via "Now Playing"): catch up with it
    if (watching && m_session && m_session->active() && !m_currentUrl.isEmpty()
        && m_player && isSameOrMatchingMedia(m_player->url(), m_currentUrl)) {
        if (m_mediaMode == TelepartyMediaMode::LocalFile) {
            updateLocalVerificationReadiness(true);
            if (isLocalParticipantSynchronized() && m_timeline.isValid()) {
                align(m_timeline.expectedPositionMs(estimatedHostNowMs()),
                      m_timeline.snapshot().playing,
                      QString(),
                      ActionOrigin::JoinSync);
            }
            return;
        }
        const qint64 targetPos = m_timeline.isValid()
                                     ? m_timeline.expectedPositionMs(estimatedHostNowMs())
                                     : partyTimeMs();
        const bool shouldPlay = m_timeline.isValid() ? m_timeline.snapshot().playing : m_partyPlaying;
        align(targetPos, shouldPlay, QString(), ActionOrigin::JoinSync);

        armNeedState();
        m_session->sendEvent(QStringLiteral("syncme"));
    }
}

bool TelepartySync::isStreamUrl(const QString &url) const {
    return !encodeCatalogMovieId(url).isEmpty();
}

void TelepartySync::broadcastOpen(const QString &url, const QString &title, qint64 startMs) {
    if (!m_session || !m_session->active() || !isStreamUrl(url)) return;

    const QString movieId = encodeCatalogMovieId(url);
    if (movieId.isEmpty()) return;

    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        clearLocalSessionState();
    }

    flushSeek();
    const qint64 rev = m_timeline.nextRevision();
    const qint64 hostNow = estimatedHostNowMs();
    const qint64 cleanStartMs = AuthoritativeTimeline::sanitizePositionMs(startMs);
    noteOpen(url, title.left(kMaxTitleLength), rev, hostNow, m_session->memberId(), cleanStartMs);
    m_announceOnPlay = true;
    m_needState = false;
    m_stateTimer.stop();

    QVariantMap d;
    d.insert(QStringLiteral("id"), movieId);
    d.insert(QStringLiteral("rev"), rev);
    d.insert(QStringLiteral("ht"), hostNow);
    if (cleanStartMs > 0) {
        d.insert(QStringLiteral("t"), cleanStartMs);
    }
    if (!isBuiltinCatalogId(movieId) && !m_currentTitle.isEmpty()) {
        d.insert(QStringLiteral("n"), m_currentTitle);
    }
    m_session->sendEvent(QStringLiteral("open"), d);
}

void TelepartySync::adoptCurrent(const QString &url, const QString &title) {
    if (!m_session || !m_session->active()) return;

    if (!isStreamUrl(url)) return;
    const QString movieId = encodeCatalogMovieId(url);
    if (movieId.isEmpty()) return;

    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        clearLocalSessionState();
    }

    m_currentUrl = url;
    m_currentTitle = title.left(kMaxTitleLength);
    m_partyTimeMs = m_player ? m_player->timeMs() : 0;
    m_partyDurationMs = m_player ? m_player->durationMs() : 0;
    m_partyPlaying = playerRunning();
    m_partyUpdatedLocalMs = localNowMs();

    TimelineSnapshot snap;
    snap.sessionId = m_session->code();
    snap.revision = m_timeline.nextRevision();
    snap.authorId = m_session->memberId();
    snap.mediaId = movieId;
    snap.mediaUrl = m_currentUrl;
    snap.mediaTitle = m_currentTitle;
    snap.durationMs = m_partyDurationMs;
    snap.basePositionMs = m_partyTimeMs;
    snap.hostTimestampMs = estimatedHostNowMs();
    snap.playing = m_partyPlaying;
    snap.playbackRate = m_player ? m_player->playbackRate() : 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);

    emit currentMediaChanged();
    emit diagnosticsChanged();
}

void TelepartySync::joinPartyPlayback() {
    if (!m_session || !m_session->active()) return;

    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        if (m_currentUrl.isEmpty()) {
            if (m_hostLocalDescriptor.isValidForSession()) {
                emit localMediaPromptRequested(m_hostLocalDescriptor.displayTitle,
                                               m_hostLocalDescriptor.durationMs,
                                               m_hostLocalDescriptor.containerFormat);
                emit activity(QStringLiteral("Select '%1' from your Library to verify and join Local Teleparty")
                                  .arg(m_hostLocalDescriptor.displayTitle));
            }
            return;
        }
        const qint64 startMs = partyTimeMs();
        emit remoteOpenRequested(m_currentUrl, m_currentTitle, startMs);
        return;
    }

    if (m_currentUrl.isEmpty()) return;

    const qint64 startMs = partyTimeMs();
    m_pendingSync = true;
    m_pendingTimeMs = startMs;
    m_pendingPlaying = m_timeline.isValid() ? m_timeline.snapshot().playing : m_partyPlaying;
    m_pendingStartedLocalMs = localNowMs();

    armNeedState();
    m_session->sendEvent(QStringLiteral("syncme"));

    FLUX_LOG_INFO("Teleparty", QString("Joining party playback: %1 at %2")
                                   .arg(m_currentTitle.isEmpty() ? m_currentUrl : m_currentTitle, formatTime(startMs)));
    emit remoteOpenRequested(m_currentUrl, m_currentTitle, startMs);
}

// ============================================================================
// Local user -> everyone else (ActionOrigin::LocalUser only)
// ============================================================================

void TelepartySync::onUserToggledPlay(bool playing, qint64 timeMs) {
    // Prevent feedback loops: remote scheduled actions, drift corrections, and buffer
    // recoveries never broadcast outbound events.
    if (m_internalActionDepth > 0) return;
    if (!m_session || !m_session->active()) return;

    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        updateLocalVerificationReadiness(true);
        if (!isLocalParticipantSynchronized()) {
            // A participant without a verified local copy cannot control this media.
            if (playing && m_player && !m_player->isPaused()) {
                ScopedInternalAction guard(m_internalActionDepth);
                m_player->remotePause();
            }
            emit activity(localStatusText());
            return;
        }
    } else {
        if (m_currentUrl.isEmpty() && m_player && isStreamUrl(m_player->url())) {
            m_currentUrl = m_player->url();
            emit currentMediaChanged();
        }
    }
    if (m_currentUrl.isEmpty()) return;

    flushSeek(); // keep the order the user performed things in

    const qint64 posMs = AuthoritativeTimeline::sanitizePositionMs(timeMs, m_partyDurationMs);
    const qint64 localNow = localNowMs();
    const qint64 hostNow = estimatedHostNowMs();
    const qint64 leadMs = m_useScheduledLeadTime ? scheduledLeadTimeMs() : 0;
    const qint64 targetHostMs = hostNow + leadMs;
    const qint64 rev = m_timeline.nextRevision();

    m_partyTimeMs = posMs;
    m_partyPlaying = playing;
    m_partyUpdatedLocalMs = localNow;
    emit partyPlayingChanged();

    TimelineSnapshot snap = m_timeline.snapshot();
    snap.sessionId = m_session->code();
    snap.revision = rev;
    snap.authorId = m_session->memberId();
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        snap.mediaId = m_hostLocalDescriptor.mediaId;
        snap.mediaUrl = m_currentUrl;
        snap.mediaTitle = m_currentTitle;
        snap.durationMs = m_hostLocalDescriptor.durationMs;
    } else {
        if (snap.mediaId.isEmpty()) snap.mediaId = encodeCatalogMovieId(m_currentUrl);
        snap.mediaUrl = m_currentUrl;
        snap.mediaTitle = m_currentTitle;
        if (m_player && m_player->durationMs() > 0) snap.durationMs = m_player->durationMs();
    }
    snap.basePositionMs = posMs;
    snap.hostTimestampMs = targetHostMs;
    snap.playing = playing;
    snap.playbackRate = 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);
    m_scheduler.cancelSuperseded(rev);

    QVariantMap d;
    d.insert(QStringLiteral("t"), posMs);
    d.insert(QStringLiteral("rev"), rev);
    d.insert(QStringLiteral("ht"), targetHostMs);
    if (m_mediaMode == TelepartyMediaMode::LocalFile && !m_hostLocalDescriptor.mediaId.isEmpty()) {
        d.insert(QStringLiteral("mid"), m_hostLocalDescriptor.mediaId);
    }
    m_session->sendEvent(playing ? QStringLiteral("play") : QStringLiteral("pause"), d);
    emit diagnosticsChanged();
}

void TelepartySync::onUserSeeked(qint64 timeMs) {
    if (m_internalActionDepth > 0) return;
    if (!m_session || !m_session->active()) return;

    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        updateLocalVerificationReadiness(true);
        if (!isLocalParticipantSynchronized()) {
            return;
        }
    } else {
        if (m_currentUrl.isEmpty() && m_player && isStreamUrl(m_player->url())) {
            m_currentUrl = m_player->url();
            emit currentMediaChanged();
        }
    }
    if (m_currentUrl.isEmpty()) return;

    m_partyTimeMs = AuthoritativeTimeline::sanitizePositionMs(timeMs, m_partyDurationMs);
    m_partyUpdatedLocalMs = localNowMs();
    m_driftController.notifyExternalSeek(m_partyUpdatedLocalMs, m_partyTimeMs);

    m_pendingSeekMs = m_partyTimeMs;
    m_hasPendingSeek = true;
    m_seekTimer.start();
}

void TelepartySync::flushSeek() {
    m_seekTimer.stop();
    if (!m_hasPendingSeek) return;
    m_hasPendingSeek = false;

    if (!m_session || !m_session->active() || m_currentUrl.isEmpty()) return;
    if (m_mediaMode == TelepartyMediaMode::LocalFile && !isLocalParticipantSynchronized()) {
        return;
    }

    const qint64 posMs = AuthoritativeTimeline::sanitizePositionMs(m_pendingSeekMs, m_partyDurationMs);
    const qint64 hostNow = estimatedHostNowMs();
    const qint64 leadMs = m_useScheduledLeadTime ? scheduledLeadTimeMs() : 0;
    const qint64 targetHostMs = hostNow + leadMs;
    const qint64 rev = m_timeline.nextRevision();

    TimelineSnapshot snap = m_timeline.snapshot();
    snap.sessionId = m_session->code();
    snap.revision = rev;
    snap.authorId = m_session->memberId();
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        snap.mediaId = m_hostLocalDescriptor.mediaId;
        snap.mediaUrl = m_currentUrl;
        snap.mediaTitle = m_currentTitle;
        snap.durationMs = m_hostLocalDescriptor.durationMs;
    } else {
        if (snap.mediaId.isEmpty()) snap.mediaId = encodeCatalogMovieId(m_currentUrl);
        snap.mediaUrl = m_currentUrl;
        snap.mediaTitle = m_currentTitle;
        if (m_player && m_player->durationMs() > 0) snap.durationMs = m_player->durationMs();
    }
    snap.basePositionMs = posMs;
    snap.hostTimestampMs = targetHostMs;
    snap.playing = m_timeline.isValid() ? m_timeline.snapshot().playing : m_partyPlaying;
    snap.playbackRate = 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);
    m_scheduler.cancelSuperseded(rev);

    QVariantMap d;
    d.insert(QStringLiteral("t"), posMs);
    d.insert(QStringLiteral("rev"), rev);
    d.insert(QStringLiteral("ht"), targetHostMs);
    if (m_mediaMode == TelepartyMediaMode::LocalFile && !m_hostLocalDescriptor.mediaId.isEmpty()) {
        d.insert(QStringLiteral("mid"), m_hostLocalDescriptor.mediaId);
    }
    m_session->sendEvent(QStringLiteral("seek"), d);
    emit diagnosticsChanged();
}

void TelepartySync::sendState() {
    if (!m_session || !m_session->active()) return;
    if (m_hasPendingSeek) {
        flushSeek();
    }

    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        // In Local Teleparty mode, only the participant who selected the media announces the shared
        // LocalMediaDescriptor ("lopen") so newcomers/reconnectors can verify their local copy.
        if (m_localMediaAuthorityPeerId != m_session->memberId() ||
            !m_hostLocalDescriptor.isValidForSession()) {
            return;
        }
        const qint64 hostNow = estimatedHostNowMs();
        const qint64 posMs = m_timeline.isValid()
                                 ? m_timeline.expectedPositionMs(hostNow)
                                 : (m_player ? m_player->timeMs() : m_partyTimeMs);
        const bool playing = m_localPlaybackCommitted &&
                             (m_timeline.isValid() ? m_timeline.snapshot().playing : playerRunning());
        QVariantMap d = m_hostLocalDescriptor.toNetworkJson().toVariantMap();
        d.insert(QStringLiteral("pos"), posMs);
        d.insert(QStringLiteral("ht"), hostNow);
        d.insert(QStringLiteral("p"), playing ? 1 : 0);
        d.insert(QStringLiteral("committed"), m_localPlaybackCommitted ? 1 : 0);
        d.insert(QStringLiteral("trev"), std::max<qint64>(m_hostLocalDescriptor.revision, m_timeline.revision()));
        if (!LocalMediaDescriptor::containsPrivatePathLeakInJson(QJsonObject::fromVariantMap(d))) {
            m_session->sendEvent(QStringLiteral("lopen"), d);
        }
        return;
    }

    if (!m_player) return;
    if (m_currentUrl.isEmpty() && isStreamUrl(m_player->url())
        && m_player->state() != QLatin1String("Idle") && m_player->state() != QLatin1String("Stopped")) {
        m_currentUrl = m_player->url();
        emit currentMediaChanged();
    }
    if (m_currentUrl.isEmpty()) return;
    if (m_needState || !isSameOrMatchingMedia(m_player->url(), m_currentUrl)) return; // not in a position to answer
    const QString st = m_player->state();
    if (st == QLatin1String("Opening") || st == QLatin1String("Idle") || st == QLatin1String("Stopped")) return;

    const QString movieId = encodeCatalogMovieId(m_currentUrl);
    if (movieId.isEmpty()) return;

    const qint64 posMs = m_player->timeMs();
    const qint64 durMs = m_player->durationMs();
    const bool playing = playerRunning();
    const qint64 hostNow = estimatedHostNowMs();
    const qint64 rev = std::max<qint64>(1, m_timeline.revision());

    TimelineSnapshot snap = m_timeline.snapshot();
    snap.sessionId = m_session->code();
    snap.revision = rev;
    snap.authorId = m_session->memberId();
    snap.mediaId = movieId;
    snap.mediaUrl = m_currentUrl;
    snap.mediaTitle = m_currentTitle;
    snap.durationMs = durMs;
    snap.basePositionMs = posMs;
    snap.hostTimestampMs = hostNow;
    snap.playing = playing;
    snap.playbackRate = 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);

    QVariantMap d;
    d.insert(QStringLiteral("id"), movieId);
    if (!isBuiltinCatalogId(movieId) && !m_currentTitle.isEmpty()) {
        d.insert(QStringLiteral("n"), m_currentTitle);
    }
    d.insert(QStringLiteral("t"), posMs);
    d.insert(QStringLiteral("d"), durMs);
    d.insert(QStringLiteral("p"), playing ? 1 : 0);
    d.insert(QStringLiteral("rev"), m_timeline.revision());
    d.insert(QStringLiteral("ht"), hostNow);
    m_session->sendEvent(QStringLiteral("state"), d);
}

// ============================================================================
// Clock Synchronization ("ts") & Participant Readiness ("rd")
// ============================================================================

void TelepartySync::sendClockPing() {
    if (!m_session || !m_session->active() || m_session->isHost()) return;
    if (m_session->memberCount() <= 1) return;

    const qint64 t1 = localNowMs();
    m_lastClockPingLocalMs = t1;
    m_clockEstimator.registerOutgoingPing(t1);

    QVariantMap d;
    d.insert(QStringLiteral("q"), t1);
    if (m_clockEstimator.medianRttMs() > 0) {
        d.insert(QStringLiteral("l"), m_clockEstimator.medianRttMs());
    }
    m_session->sendEvent(QStringLiteral("ts"), d);
}

void TelepartySync::handleTimeSync(const QVariantMap &data, const QString &from) {
    if (!m_session || !m_session->active()) return;

    // 1. Incoming Pong (contains "r" = t2 and "s" = t3)
    if (data.contains(QStringLiteral("r")) && data.contains(QStringLiteral("s"))) {
        const QString targetPeer = data.value(QStringLiteral("p")).toString();
        if (!targetPeer.isEmpty() && targetPeer != m_session->memberId()) {
            return; // Pong addressed to a different participant
        }
        // Only accept time reference responses from the host / authority peer
        const QString authPeer = m_session->hostMemberId();
        if (!authPeer.isEmpty() && from != authPeer) {
            return;
        }

        const qint64 t1 = data.value(QStringLiteral("q")).toLongLong();
        const qint64 t2 = data.value(QStringLiteral("r")).toLongLong();
        const qint64 t3 = data.value(QStringLiteral("s")).toLongLong();
        const qint64 t4 = localNowMs();

        const bool hadEstimateBefore = m_clockEstimator.hasEstimate();
        if (m_clockEstimator.processSample(t1, t2, t3, t4)) {
            // If a "state" snapshot arrived before our first clock sample locked,
            // re-anchor the timeline to the exact host timestamp now!
            if (!hadEstimateBefore && m_reanchorStateOnFirstClockLock && m_timeline.isValid()) {
                m_reanchorStateOnFirstClockLock = false;
                TimelineSnapshot snap = m_timeline.snapshot();
                snap.basePositionMs = m_rawStateBasePositionMs;
                snap.hostTimestampMs = m_rawStateHostTimestampMs;
                m_timeline.forceUpdate(snap);
            }

            // Recalculate any pending scheduled transition timers with the refined clock offset
            processScheduledActions();
            updateScheduleTimer();

            // Adapt ping interval (fast initial burst -> slower steady state)
            if (m_clockSyncTimer.isActive()) {
                m_clockSyncTimer.setInterval(m_clockEstimator.recommendedPingIntervalMs());
            }
            emit diagnosticsChanged();
        }
        return;
    }

    // 2. Incoming Ping (contains "q" = t1): Host / authority responds with (t1, t2, t3)
    if (data.contains(QStringLiteral("q"))) {
        const bool isAuthority = m_session->isHost() ||
                                 (m_session->hostMemberId() == m_session->memberId());
        if (!isAuthority) {
            return;
        }

        const qint64 t2 = localNowMs();
        const qint64 t1 = data.value(QStringLiteral("q")).toLongLong();
        const qint64 reportedRtt = data.value(QStringLiteral("l"), 0).toLongLong();
        if (reportedRtt > 0) {
            m_clockEstimator.recordPeerRttSample(reportedRtt);
        }

        const qint64 t3 = localNowMs();
        QVariantMap pong;
        pong.insert(QStringLiteral("p"), from);
        pong.insert(QStringLiteral("q"), t1);
        pong.insert(QStringLiteral("r"), t2);
        pong.insert(QStringLiteral("s"), t3);
        m_session->sendEvent(QStringLiteral("ts"), pong);
        emit diagnosticsChanged();
    }
}

void TelepartySync::updateLocalReadiness(bool allowBroadcast) {
    ParticipantReadiness next = ParticipantReadiness::Idle;
    if (m_player && !m_currentUrl.isEmpty() && isSameOrMatchingMedia(m_player->url(), m_currentUrl)) {
        const QString st = m_player->state();
        if (st == QLatin1String("Error")) {
            next = ParticipantReadiness::Error;
        } else if (st == QLatin1String("Opening")) {
            next = ParticipantReadiness::Opening;
        } else if (m_player->isBuffering() || st.startsWith(QLatin1String("Buffering"))) {
            next = ParticipantReadiness::Buffering;
        } else if (m_player->isSeekSettling()) {
            next = ParticipantReadiness::Seeking;
        } else if (st == QLatin1String("Playing") || st == QLatin1String("Paused")) {
            next = ParticipantReadiness::Ready;
        }
    }

    if (next == m_localReadiness) {
        return;
    }
    m_localReadiness = next;

    if (!allowBroadcast || !m_session || !m_session->active() || m_session->memberCount() <= 1) {
        return;
    }

    const qint64 now = localNowMs();
    if (m_lastReadinessSentLocalMs > 0 && (now - m_lastReadinessSentLocalMs) < kReadinessMinIntervalMs
        && next != ParticipantReadiness::Ready) {
        return;
    }
    m_lastReadinessSentLocalMs = now;

    QVariantMap d;
    d.insert(QStringLiteral("s"), readinessToCode(m_localReadiness));
    d.insert(QStringLiteral("t"), m_player ? m_player->timeMs() : 0);
    d.insert(QStringLiteral("rev"), m_timeline.revision());
    m_session->sendEvent(QStringLiteral("rd"), d);
}

void TelepartySync::handleReadiness(const QVariantMap &data, const QString &from) {
    const QString code = data.value(QStringLiteral("s")).toString();
    m_peerReadiness.insert(from, readinessFromCode(code));
}

// ============================================================================
// Everyone else -> local player
// ============================================================================

void TelepartySync::onEvent(const QString &type, const QVariantMap &data, const QString &from) {
    if (type == QLatin1String("open")) {
        handleOpen(data, from);
    } else if (type == QLatin1String("state")) {
        handleState(data, from);
    } else if (type == QLatin1String("syncme")) {
        sendState();
    } else if (type == QLatin1String("ts")) {
        handleTimeSync(data, from);
    } else if (type == QLatin1String("rd")) {
        handleReadiness(data, from);
    } else if (type == QLatin1String("lopen")) {
        handleLocalOpen(data, from);
    } else if (type == QLatin1String("lver")) {
        handleLocalVerifyReport(data, from);
    } else if (type == QLatin1String("lstart")) {
        handleLocalStartCommit(data, from);
    } else if (type == QLatin1String("play") || type == QLatin1String("pause") || type == QLatin1String("seek")) {
        handlePlaybackTransition(type, data, from);
    }
}

void TelepartySync::handleOpen(const QVariantMap &data, const QString &from) {
    QString url;
    QString title;
    if (!resolveCatalogMovie(data, url, title)) {
        FLUX_LOG_WARN("Teleparty", "Rejected remote 'open' event: movie ID / URL is not in the local catalog");
        return;
    }

    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        clearLocalSessionState();
    }

    const qint64 now = localNowMs();
    // Autoplay can make two members open the same next episode at the same moment
    if (url == m_lastOpenUrl && m_lastOpenAt > 0 && (now - m_lastOpenAt) < kOpenDedupeMs) return;

    const qint64 rev = data.contains(QStringLiteral("rev"))
                           ? data.value(QStringLiteral("rev")).toLongLong()
                           : m_timeline.nextRevision();
    const qint64 ht = data.contains(QStringLiteral("ht"))
                          ? data.value(QStringLiteral("ht")).toLongLong()
                          : estimatedHostNowMs();

    const qint64 startMs = cleanTime(data.value(QStringLiteral("t")));

    noteOpen(url, title, rev, ht, from, startMs);
    m_syncOnPlay = true;
    m_needState = false;
    m_stateTimer.stop();

    FLUX_LOG_INFO("Teleparty", QString("A member opened: %1 at %2 (rev=%3)")
                                   .arg(title.isEmpty() ? url : title, formatTime(startMs))
                                   .arg(rev));
    emit remoteOpenRequested(url, title, startMs);
}

void TelepartySync::handleState(const QVariantMap &data, const QString &from) {
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        return;
    }

    const qint64 incomingRev = data.value(QStringLiteral("rev"), 0).toLongLong();
    const bool isHigherRevision = (incomingRev > 0 && incomingRev > m_timeline.revision());

    if (!m_needState && !m_currentUrl.isEmpty() && !isHigherRevision) {
        return; // we did not ask (or already have an up-to-date revision)
    }

    QString url;
    QString title;
    if (!resolveCatalogMovie(data, url, title)) {
        FLUX_LOG_WARN("Teleparty", "Rejected remote 'state' event: movie ID / URL is not in the local catalog");
        return;
    }

    m_needState = false;
    m_stateTimer.stop();

    const qint64 t = cleanTime(data.value(QStringLiteral("t")));
    const qint64 dur = cleanTime(data.value(QStringLiteral("d")));
    const bool playing = data.contains(QStringLiteral("p"))
                             ? (data.value(QStringLiteral("p")).toInt() != 0)
                             : data.value(QStringLiteral("playing"), true).toBool();
    const double rate = AuthoritativeTimeline::sanitizeRate(data.value(QStringLiteral("r"), 1.0).toDouble());
    const qint64 rawHt = data.value(QStringLiteral("ht"), 0).toLongLong();
    const qint64 rev = (incomingRev > 0) ? incomingRev : std::max<qint64>(1, m_timeline.nextRevision());

    const bool changed = (m_currentUrl != url || m_currentTitle != title);
    m_currentUrl = url;
    m_currentTitle = title;
    m_partyTimeMs = t;
    if (dur > 0) m_partyDurationMs = dur;
    m_partyPlaying = playing;
    m_partyUpdatedLocalMs = localNowMs();
    emit partyPlayingChanged();

    TimelineSnapshot snap;
    snap.sessionId = m_session ? m_session->code() : QString();
    snap.revision = rev;
    snap.authorId = from;
    snap.mediaId = encodeCatalogMovieId(url);
    snap.mediaUrl = url;
    snap.mediaTitle = title;
    snap.durationMs = m_partyDurationMs;
    snap.playing = playing;
    snap.playbackRate = rate;
    snap.valid = true;

    if (rawHt > 0 && m_clockEstimator.hasEstimate()) {
        snap.basePositionMs = t;
        snap.hostTimestampMs = rawHt;
        m_reanchorStateOnFirstClockLock = false;
    } else {
        // Clock estimator hasn't locked its first sample yet; compensate with estimated one-way delay
        // and store rawHt so the timeline re-anchors as soon as the first "ts" pong arrives.
        const qint64 oneWayCompMs = playing ? ClockEstimator::kDefaultOneWayMs : 0;
        snap.basePositionMs = AuthoritativeTimeline::sanitizePositionMs(t + oneWayCompMs, m_partyDurationMs);
        snap.hostTimestampMs = estimatedHostNowMs();
        if (rawHt > 0) {
            m_reanchorStateOnFirstClockLock = true;
            m_rawStateHostTimestampMs = rawHt;
            m_rawStateBasePositionMs = t;
        }
    }
    m_timeline.forceUpdate(snap);

    const qint64 liveExpectedPos = m_timeline.expectedPositionMs(estimatedHostNowMs());

    if (changed) emit currentMediaChanged();
    emit partyMediaAvailable(url, title, liveExpectedPos, m_partyDurationMs);
    emit diagnosticsChanged();

    // Already on that video on the player page: line up with the others
    if (m_watching && m_player && isSameOrMatchingMedia(m_player->url(), url)) {
        if (m_player->state() == QLatin1String("Opening") || m_player->isBuffering() || m_pendingSync) {
            m_pendingSync = true;
            m_pendingTimeMs = liveExpectedPos;
            m_pendingPlaying = playing;
            m_pendingStartedLocalMs = localNowMs();
        } else {
            align(liveExpectedPos, playing, QString(), ActionOrigin::JoinSync);
        }
        return;
    }

    // Late joiner on the Home / Search page: do NOT force-open or reset the party;
    // the video is now placed first in Continue Watching and the Now Playing pill is active.
    FLUX_LOG_INFO("Teleparty", QString("Party is watching: %1 at %2 (rev=%3)")
                                   .arg(title.isEmpty() ? url : title, formatTime(liveExpectedPos))
                                   .arg(rev));
}

void TelepartySync::handlePlaybackTransition(const QString &type, const QVariantMap &data, const QString &from) {
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        if (!m_hostLocalDescriptor.isValidForSession()) {
            return;
        }
        const QString incomingMid = data.value(QStringLiteral("mid")).toString();
        if (!incomingMid.isEmpty() && incomingMid != m_hostLocalDescriptor.mediaId) {
            FLUX_LOG_WARN("Teleparty",
                          QString("Rejected '%1' event for mismatched local mediaId '%2' (expected '%3')")
                              .arg(type, incomingMid, m_hostLocalDescriptor.mediaId));
            return;
        }
        // The room membership is the control authority. Local decoder readiness
        // must not block play/pause/seek events while another member is opening.
    }

    const qint64 t = cleanTime(data.value(QStringLiteral("t")));
    const qint64 rev = data.contains(QStringLiteral("rev"))
                           ? data.value(QStringLiteral("rev")).toLongLong()
                           : m_timeline.nextRevision();
    const qint64 rawHt = data.value(QStringLiteral("ht"), 0).toLongLong();
    const double rate = AuthoritativeTimeline::sanitizeRate(data.value(QStringLiteral("r"), 1.0).toDouble());
    const qint64 seq = data.value(QStringLiteral("_seq"), 0).toLongLong();

    const QString eventId = (seq > 0)
                                ? QStringLiteral("%1:%2").arg(from).arg(seq)
                                : data.value(QStringLiteral("eid")).toString();

    const qint64 estHostNow = estimatedHostNowMs();
    qint64 targetHostMs = (rawHt > 0 && m_clockEstimator.hasEstimate()) ? rawHt : estHostNow;

    ScheduledAction action;
    action.revision = rev;
    action.eventId = eventId;
    action.authorId = from;
    action.targetPositionMs = t;
    action.targetHostTimestampMs = targetHostMs;
    action.playbackRate = rate;
    action.origin = ActionOrigin::ScheduledRemote;

    if (type == QLatin1String("play")) {
        action.type = ScheduledActionType::Play;
        action.playing = true;
        action.activityNote = QStringLiteral("Resumed by a member");
    } else if (type == QLatin1String("pause")) {
        action.type = ScheduledActionType::Pause;
        action.playing = false;
        action.activityNote = QStringLiteral("Paused by a member");
    } else {
        action.type = ScheduledActionType::Seek;
        action.playing = m_timeline.isValid() ? m_timeline.snapshot().playing : m_partyPlaying;
        action.activityNote = QStringLiteral("Skipped to ") + formatTime(t);
    }

    if (action.type == ScheduledActionType::Pause || action.type == ScheduledActionType::Seek) {
        // Pause and seek are direct user commands. Queueing either against a remote
        // clock estimate can add up to two seconds of apparent input lag. Apply them
        // when received; keep their media position intact and leave play scheduling
        // unchanged.
        targetHostMs = estHostNow;
        action.targetHostTimestampMs = targetHostMs;
    }

    const QString hostPeerId = m_session ? m_session->hostMemberId() : QString();
    ScheduledAction readyAction;
    const ScheduleStatus status = m_scheduler.schedule(action, estHostNow, localNowMs(), readyAction, hostPeerId);

    if (status == ScheduleStatus::DuplicateIgnored || status == ScheduleStatus::StaleDropped) {
        FLUX_LOG_DEBUG("Teleparty",
                       QString("Ignored %1 '%2' event from '%3' (rev=%4, currentRev=%5)")
                           .arg(status == ScheduleStatus::DuplicateIgnored ? "duplicate" : "stale/out-of-order",
                                type, from)
                           .arg(rev)
                           .arg(m_timeline.revision()));
        emit diagnosticsChanged();
        return;
    }

    // Update authoritative timeline
    TimelineSnapshot snap = m_timeline.snapshot();
    snap.sessionId = m_session ? m_session->code() : QString();
    snap.revision = rev;
    snap.eventId = eventId;
    snap.authorId = from;
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        snap.mediaId = m_hostLocalDescriptor.mediaId;
        snap.mediaUrl = m_currentUrl;
        snap.mediaTitle = m_currentTitle;
        snap.durationMs = m_hostLocalDescriptor.durationMs;
    } else if (!m_currentUrl.isEmpty()) {
        if (snap.mediaId.isEmpty()) snap.mediaId = encodeCatalogMovieId(m_currentUrl);
        snap.mediaUrl = m_currentUrl;
        snap.mediaTitle = m_currentTitle;
        snap.durationMs = m_partyDurationMs;
    } else {
        snap.durationMs = m_partyDurationMs;
    }
    snap.basePositionMs = t;
    snap.hostTimestampMs = targetHostMs;
    snap.playing = action.playing;
    snap.playbackRate = rate;
    snap.valid = !m_currentUrl.isEmpty() || snap.valid || !snap.mediaId.isEmpty();
    m_timeline.forceUpdate(snap);

    const qint64 expectedNowPos = m_timeline.isValid() ? m_timeline.expectedPositionMs(estHostNow) : t;
    m_partyTimeMs = expectedNowPos;
    m_partyPlaying = action.playing;
    m_partyUpdatedLocalMs = localNowMs();
    emit partyPlayingChanged();

    if (!m_currentUrl.isEmpty()) {
        emit partyMediaAvailable(m_currentUrl, m_currentTitle, expectedNowPos, m_partyDurationMs);
    }

    if (status == ScheduleStatus::Queued) {
        updateScheduleTimer();
        emit diagnosticsChanged();
        return;
    }

    executeScheduledAction(readyAction);
    emit diagnosticsChanged();
}

// ============================================================================
// Local Video Teleparty Implementation ("lopen", "lver", "lstart")
// ============================================================================

void TelepartySync::cancelActiveInspection() {
    if (m_activeInspectCancel) {
        m_activeInspectCancel->store(true, std::memory_order_relaxed);
        m_activeInspectCancel.reset();
    }
}

void TelepartySync::clearLocalSessionState() {
    cancelActiveInspection();
    m_localReadinessTimeoutTimer.stop();
    m_mediaMode = TelepartyMediaMode::OnlineCatalog;
    m_localVerificationState = LocalVerificationState::None;
    m_localCompatibilityReason = LocalCompatibilityReason::None;
    m_hostLocalDescriptor = LocalMediaDescriptor{};
    m_guestLocalDescriptor = LocalMediaDescriptor{};
    m_localMediaAuthorityPeerId.clear();
    m_localFilePath.clear();
    m_peerLocalStatus.clear();
    m_localPlaybackCommitted = false;
    m_localPlaybackAligned = false;
    m_hostSessionAlreadyStarted = false;
    m_localReadinessWaitTimedOut = false;
    m_localReadinessWaitStartedMs = 0;
    emit localVerificationChanged();
}

void TelepartySync::setLocalReadinessTimeoutMs(qint64 timeoutMs) {
    m_localReadinessTimeoutMs = std::max<qint64>(500, timeoutMs);
}

ParticipantLocalStatus TelepartySync::peerLocalStatus(const QString &peerId) const {
    return m_peerLocalStatus.value(peerId, ParticipantLocalStatus{});
}

bool TelepartySync::isLocalParticipantSynchronized() const {
    if (m_mediaMode != TelepartyMediaMode::LocalFile) {
        return !m_currentUrl.isEmpty();
    }
    return m_hostLocalDescriptor.isValidForSession() &&
           !m_currentUrl.isEmpty() &&
           m_guestLocalDescriptor.fileSizeBytes > 0 &&
           m_localVerificationState != LocalVerificationState::IncompatibleMedia &&
           m_localVerificationState != LocalVerificationState::UnresolvedMetadata &&
           m_localVerificationState != LocalVerificationState::PlaybackError;
}

bool TelepartySync::areAllParticipantsReadyToSync() const {
    if (m_mediaMode != TelepartyMediaMode::LocalFile || !m_hostLocalDescriptor.isValidForSession()) {
        return false;
    }
    if (m_localVerificationState != LocalVerificationState::ReadyToSync) {
        return false;
    }
    if (!m_session || !m_session->active()) {
        return false;
    }
    const QStringList members = m_session->memberIds();
    if (members.size() < 2) {
        return false;
    }
    for (const QString &peerId : members) {
        if (peerId == m_session->memberId()) {
            continue;
        }
        const auto it = m_peerLocalStatus.constFind(peerId);
        if (it == m_peerLocalStatus.constEnd()) {
            return false;
        }
        if (it->mediaId != m_hostLocalDescriptor.mediaId ||
            it->revision != m_hostLocalDescriptor.revision ||
            it->state != LocalVerificationState::ReadyToSync) {
            return false;
        }
    }
    return true;
}

int TelepartySync::readyParticipantCount() const {
    if (m_mediaMode != TelepartyMediaMode::LocalFile || !m_session || !m_session->active()) {
        return 0;
    }
    int count = (m_localVerificationState == LocalVerificationState::ReadyToSync) ? 1 : 0;
    for (const QString &peerId : m_session->memberIds()) {
        if (peerId == m_session->memberId()) continue;
        const auto it = m_peerLocalStatus.constFind(peerId);
        if (it != m_peerLocalStatus.constEnd() &&
            it->mediaId == m_hostLocalDescriptor.mediaId &&
            it->revision == m_hostLocalDescriptor.revision &&
            it->state == LocalVerificationState::ReadyToSync) {
            ++count;
        }
    }
    return count;
}

QString TelepartySync::localErrorText() const {
    if (m_mediaMode != TelepartyMediaMode::LocalFile) {
        return QString();
    }
    if (m_localReadinessWaitTimedOut && !m_localPlaybackCommitted) {
        return QStringLiteral("Timed out preparing the local video for synchronized playback.");
    }
    if (m_localVerificationState == LocalVerificationState::IncompatibleMedia ||
        m_localVerificationState == LocalVerificationState::UnresolvedMetadata ||
        m_localVerificationState == LocalVerificationState::PlaybackError) {
        return localCompatibilityReasonToMessage(m_localCompatibilityReason);
    }
    // Surface peer incompatibility to the participant who started this video.
    if (m_session && m_localMediaAuthorityPeerId == m_session->memberId() &&
        !m_localPlaybackCommitted) {
        for (auto it = m_peerLocalStatus.constBegin(); it != m_peerLocalStatus.constEnd(); ++it) {
            if (it->mediaId == m_hostLocalDescriptor.mediaId &&
                it->revision == m_hostLocalDescriptor.revision) {
                if (it->state == LocalVerificationState::IncompatibleMedia ||
                    it->state == LocalVerificationState::UnresolvedMetadata ||
                    it->state == LocalVerificationState::PlaybackError) {
                    return QStringLiteral("A participant's selected local video failed verification (%1).")
                        .arg(localCompatibilityReasonToMessage(it->reason));
                }
            }
        }
    }
    return QString();
}

QString TelepartySync::localStatusText() const {
    if (m_mediaMode != TelepartyMediaMode::LocalFile) {
        return QString();
    }
    if (m_localReadinessWaitTimedOut && !m_localPlaybackCommitted) {
        return QStringLiteral("Waiting timed out — retry or cancel Local Teleparty.");
    }
    switch (m_localVerificationState) {
    case LocalVerificationState::None:
        return QString();
    case LocalVerificationState::WaitingForLocalSelection:
        return QStringLiteral("Select '%1' from your Library to match this video.")
            .arg(m_hostLocalDescriptor.displayTitle);
    case LocalVerificationState::InspectingLocalMedia:
        return QStringLiteral("Matching local video by name…");
    case LocalVerificationState::VerifiedCompatible:
    case LocalVerificationState::OpeningMedia:
        return QStringLiteral("Verified compatible — opening local video…");
    case LocalVerificationState::BufferingMedia:
        return QStringLiteral("Preparing decoder and buffer for synchronized start…");
    case LocalVerificationState::IncompatibleMedia:
    case LocalVerificationState::UnresolvedMetadata:
    case LocalVerificationState::PlaybackError:
        return localCompatibilityReasonToMessage(m_localCompatibilityReason);
    case LocalVerificationState::ReadyToSync: {
        if (m_localPlaybackCommitted) {
            return QStringLiteral("Local video verified & synchronized (%1/%2 ready).")
                .arg(readyParticipantCount())
                .arg(m_session ? std::max(1, m_session->memberCount()) : 1);
        }
        if (m_session && m_localMediaAuthorityPeerId != m_session->memberId()) {
            return QStringLiteral("Your local copy is ready — waiting for the participant who started playback…");
        }
        const int total = m_session ? m_session->memberCount() : 1;
        if (total <= 1) {
            return QStringLiteral("Local video ready — starting playback.");
        }
        return QStringLiteral("Local video ready — starting with %1/%2 participants ready.")
            .arg(readyParticipantCount())
            .arg(total);
    }
    }
    return QString();
}

bool TelepartySync::broadcastLocalOpen(const QString &fileUrlOrPath,
                                       const QString &title,
                                       qint64 knownDurationMs) {
    if (!m_session || !m_session->active()) {
        return false;
    }

    QString localPath = fileUrlOrPath.trimmed();
    if (localPath.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        localPath = QUrl(localPath).toLocalFile();
    }
    const QString resolvedUrl = QUrl::fromLocalFile(localPath).toString();
    const qint64 effectiveDurationMs =
        (knownDurationMs > 0)
            ? knownDurationMs
            : ((m_player && m_player->url() == resolvedUrl && m_player->durationMs() > 0)
                   ? m_player->durationMs()
                   : 0);

    cancelActiveInspection();
    m_localMediaAuthorityPeerId = m_session->memberId();
    m_mediaMode = TelepartyMediaMode::LocalFile;
    m_localVerificationState = LocalVerificationState::InspectingLocalMedia;
    m_localCompatibilityReason = LocalCompatibilityReason::None;
    emit localVerificationChanged();

    if (m_forceAsyncInspection) {
        m_activeInspectCancel = LocalMediaInspector::inspectFileAsync(
            this,
            localPath,
            title,
            effectiveDurationMs,
            m_verificationPolicy.requireExactFullFileHash,
            [this, resolvedUrl, localPath](LocalMediaDescriptor desc, LocalCompatibilityReason errReason) {
                finishHostLocalInspection(resolvedUrl, localPath, std::move(desc), errReason);
            },
            m_verificationPolicy.requireContentFingerprint);
        return true;
    }

    LocalCompatibilityReason errReason = LocalCompatibilityReason::None;
    LocalMediaDescriptor desc = LocalMediaInspector::inspectFile(
        localPath,
        title,
        effectiveDurationMs,
        m_verificationPolicy.requireExactFullFileHash,
        &errReason,
        nullptr,
        true,
        m_verificationPolicy.requireContentFingerprint);
    finishHostLocalInspection(resolvedUrl, localPath, std::move(desc), errReason);
    return m_hostLocalDescriptor.isValidForSession();
}

void TelepartySync::finishHostLocalInspection(const QString &resolvedUrl,
                                              const QString &resolvedLocalPath,
                                              LocalMediaDescriptor desc,
                                              LocalCompatibilityReason errReason) {
    m_activeInspectCancel.reset();
    if (!m_session || !m_session->active() ||
        m_localMediaAuthorityPeerId != m_session->memberId()) {
        return;
    }

    const bool hasUsableName = !desc.normalizedTitle.isEmpty() && !desc.displayTitle.isEmpty()
                               && desc.fileSizeBytes > 0;
    if ((errReason != LocalCompatibilityReason::None &&
         errReason != LocalCompatibilityReason::InsufficientMetadata) || !hasUsableName) {
        if (errReason == LocalCompatibilityReason::InsufficientMetadata || !hasUsableName) {
            m_localVerificationState = LocalVerificationState::UnresolvedMetadata;
            m_localCompatibilityReason = LocalCompatibilityReason::InsufficientMetadata;
        } else if (errReason == LocalCompatibilityReason::UnsupportedContainer) {
            m_localVerificationState = LocalVerificationState::IncompatibleMedia;
            m_localCompatibilityReason = errReason;
        } else {
            m_localVerificationState = LocalVerificationState::PlaybackError;
            m_localCompatibilityReason = errReason;
        }
        m_hostLocalDescriptor = LocalMediaDescriptor{};
        m_guestLocalDescriptor = LocalMediaDescriptor{};
        m_localFilePath.clear();
        m_currentUrl.clear();
        m_localPlaybackCommitted = false;
        emit currentMediaChanged();
        emit localVerificationChanged();
        emit activity(localCompatibilityReasonToMessage(m_localCompatibilityReason));
        return;
    }

    const qint64 rev = m_timeline.nextRevision();
    desc.sessionId = m_session->code();
    desc.revision = rev;
    desc.mediaId = LocalMediaDescriptor::generateSessionMediaId(desc.sessionId, rev, desc);

    const QJsonObject netJson = desc.toNetworkJson();
    if (!desc.isValidForSession() || LocalMediaDescriptor::containsPrivatePathLeakInJson(netJson)) {
        FLUX_LOG_WARN("Teleparty", "Rejected host LocalMediaDescriptor: failed session or privacy validation");
        m_localVerificationState = LocalVerificationState::UnresolvedMetadata;
        m_localCompatibilityReason = LocalCompatibilityReason::InvalidSessionDescriptor;
        emit localVerificationChanged();
        return;
    }

    flushSeek();
    m_scheduler.reset();
    m_driftController.reset();
    m_scheduleTimer.stop();

    m_mediaMode = TelepartyMediaMode::LocalFile;
    m_hostLocalDescriptor = desc;
    m_guestLocalDescriptor = desc;
    m_localFilePath = resolvedLocalPath;
    m_currentUrl = resolvedUrl;
    m_currentTitle = desc.displayTitle;
    m_partyDurationMs = desc.durationMs;
    m_partyTimeMs = (m_player && m_player->url() == resolvedUrl)
                        ? AuthoritativeTimeline::sanitizePositionMs(m_player->timeMs(), desc.durationMs)
                        : 0;
    m_partyPlaying = false;
    m_partyUpdatedLocalMs = localNowMs();
    m_localPlaybackCommitted = false;
    m_localPlaybackAligned = false;
    m_hostSessionAlreadyStarted = false;
    m_localReadinessWaitTimedOut = false;
    m_localReadinessWaitStartedMs = m_partyUpdatedLocalMs;
    m_localCompatibilityReason = LocalCompatibilityReason::MatchedExact;
    m_localVerificationState = LocalVerificationState::VerifiedCompatible;

    // Reset peer verification status for the new media revision
    m_peerLocalStatus.clear();
    for (const QString &peerId : m_session->memberIds()) {
        if (peerId == m_session->memberId()) continue;
        ParticipantLocalStatus st;
        st.peerId = peerId;
        st.mediaId = desc.mediaId;
        st.revision = desc.revision;
        st.state = LocalVerificationState::WaitingForLocalSelection;
        st.reason = LocalCompatibilityReason::None;
        st.updatedAtLocalMs = m_partyUpdatedLocalMs;
        m_peerLocalStatus.insert(peerId, st);
    }

    const qint64 hostNow = estimatedHostNowMs();
    TimelineSnapshot snap;
    snap.sessionId = m_session->code();
    snap.revision = rev;
    snap.authorId = m_session->memberId();
    snap.mediaId = desc.mediaId;
    snap.mediaUrl = m_currentUrl;
    snap.mediaTitle = m_currentTitle;
    snap.durationMs = desc.durationMs;
    snap.basePositionMs = m_partyTimeMs;
    snap.hostTimestampMs = hostNow;
    snap.playing = false;
    snap.playbackRate = 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);

    if (m_player && m_player->url() != m_currentUrl) {
        QString playerLocalPath = m_player->url().trimmed();
        if (playerLocalPath.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
            playerLocalPath = QUrl(playerLocalPath).toLocalFile();
        }
        if (!playerLocalPath.isEmpty() &&
            QFileInfo(playerLocalPath).absoluteFilePath() == QFileInfo(resolvedLocalPath).absoluteFilePath()) {
            m_player->setUrl(m_currentUrl);
        } else {
            emit remoteOpenRequested(m_currentUrl, m_currentTitle, m_partyTimeMs);
        }
    }

    // Keep the host paused at the initial position until its own decoder is ready.
    if (m_player && m_player->url() == m_currentUrl && m_player->isPlaying()) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remoteSetRate(1.0);
        m_player->remotePause();
    }

    if (m_localReadinessTimeoutMs > 0) {
        m_localReadinessTimeoutTimer.start(static_cast<int>(std::min<qint64>(m_localReadinessTimeoutMs, 600000)));
    }

    QVariantMap d = netJson.toVariantMap();
    d.insert(QStringLiteral("pos"), m_partyTimeMs);
    d.insert(QStringLiteral("ht"), hostNow);
    d.insert(QStringLiteral("p"), 0);
    d.insert(QStringLiteral("committed"), 0);
    d.insert(QStringLiteral("trev"), rev);
    m_session->sendEvent(QStringLiteral("lopen"), d);

    updateLocalVerificationReadiness(false);
    tryCommitLocalPlaybackStart();

    emit currentMediaChanged();
    emit localVerificationChanged();
    emit diagnosticsChanged();
}

void TelepartySync::handleLocalOpen(const QVariantMap &data, const QString &from) {
    if (!m_session || !m_session->active()) {
        return;
    }

    // A local-media declaration comes from the participant who selected it.
    // Syncplay room membership authenticates the sender; ignore our own echo.
    if (from.isEmpty() || from == m_session->memberId()) {
        return;
    }

    const QJsonObject jsonObj = QJsonObject::fromVariantMap(data);
    if (LocalMediaDescriptor::containsPrivatePathLeakInJson(jsonObj)) {
        FLUX_LOG_WARN("Teleparty", "Rejected 'lopen' event containing private local filesystem path leak");
        return;
    }

    const LocalMediaDescriptor incoming = LocalMediaDescriptor::fromNetworkJson(jsonObj);
    if (!incoming.isValidForSession() ||
        (!incoming.sessionId.isEmpty() && incoming.sessionId != m_session->code())) {
        FLUX_LOG_WARN("Teleparty", "Rejected 'lopen' event: invalid or incomplete LocalMediaDescriptor");
        m_mediaMode = TelepartyMediaMode::LocalFile;
        m_localVerificationState = LocalVerificationState::UnresolvedMetadata;
        m_localCompatibilityReason = LocalCompatibilityReason::InvalidSessionDescriptor;
        emit localVerificationChanged();
        return;
    }

    const qint64 posMs = cleanTime(data.value(QStringLiteral("pos"), 0));
    const qint64 rawHt = data.value(QStringLiteral("ht"), 0).toLongLong();
    const qint64 trev = std::max<qint64>(incoming.revision,
                                         data.value(QStringLiteral("trev"), incoming.revision).toLongLong());
    const bool hostPlaying = (data.value(QStringLiteral("p"), 0).toInt() != 0);
    const bool hostCommitted = (data.value(QStringLiteral("committed"), 0).toInt() != 0);

    const bool sameAuthority = (m_localMediaAuthorityPeerId == from);
    // Ignore stale revisions from the current media leader. A different room
    // member may start a new video with an independent revision counter.
    if (m_mediaMode == TelepartyMediaMode::LocalFile &&
        sameAuthority &&
        m_hostLocalDescriptor.isValidForSession() &&
        incoming.revision < m_hostLocalDescriptor.revision) {
        return;
    }

    // Same mediaId + revision (e.g. response to "syncme" after reconnect or late join)
    if (m_mediaMode == TelepartyMediaMode::LocalFile &&
        m_hostLocalDescriptor.mediaId == incoming.mediaId &&
        m_hostLocalDescriptor.revision == incoming.revision) {
        m_needState = false;
        m_stateTimer.stop();
        m_hostSessionAlreadyStarted = hostCommitted;

        TimelineSnapshot snap = m_timeline.snapshot();
        snap.sessionId = m_session->code();
        snap.revision = std::max<qint64>(snap.revision, trev);
        snap.authorId = from;
        snap.mediaId = incoming.mediaId;
        snap.mediaUrl = m_currentUrl;
        snap.mediaTitle = incoming.displayTitle;
        snap.durationMs = incoming.durationMs;
        snap.playing = hostPlaying;
        snap.playbackRate = 1.0;
        snap.valid = true;
        if (rawHt > 0 && m_clockEstimator.hasEstimate()) {
            snap.basePositionMs = posMs;
            snap.hostTimestampMs = rawHt;
            m_reanchorStateOnFirstClockLock = false;
        } else {
            const qint64 oneWayCompMs = hostPlaying ? ClockEstimator::kDefaultOneWayMs : 0;
            snap.basePositionMs = AuthoritativeTimeline::sanitizePositionMs(posMs + oneWayCompMs, incoming.durationMs);
            snap.hostTimestampMs = estimatedHostNowMs();
            if (rawHt > 0) {
                m_reanchorStateOnFirstClockLock = true;
                m_rawStateHostTimestampMs = rawHt;
                m_rawStateBasePositionMs = posMs;
            }
        }
        m_timeline.forceUpdate(snap);

        // Re-validate local file after reconnect
        if (!m_localFilePath.isEmpty()) {
            if (LocalMediaInspector::hasFileChangedSince(m_localFilePath, m_guestLocalDescriptor)) {
                m_localVerificationState = LocalVerificationState::IncompatibleMedia;
                m_localCompatibilityReason = LocalCompatibilityReason::FingerprintMismatch;
                m_localPlaybackCommitted = false;
                m_currentUrl.clear();
                if (m_player && !m_player->isPaused()) {
                    ScopedInternalAction guard(m_internalActionDepth);
                    m_player->remotePause();
                }
                sendLocalVerificationReport();
                emit currentMediaChanged();
                emit localVerificationChanged();
                return;
            }
            updateLocalVerificationReadiness(true);
            sendLocalVerificationReport();
            if (m_localVerificationState == LocalVerificationState::ReadyToSync && m_hostSessionAlreadyStarted) {
                m_localPlaybackCommitted = true;
                const qint64 livePos = m_timeline.expectedPositionMs(estimatedHostNowMs());
                align(livePos, hostPlaying, QString(), ActionOrigin::JoinSync);
            }
        } else {
            sendLocalVerificationReport();
        }
        emit localVerificationChanged();
        emit diagnosticsChanged();
        return;
    }

    // New local media declaration (or host switched to a different local video)
    cancelActiveInspection();
    m_localMediaAuthorityPeerId = from;
    m_needState = false;
    m_stateTimer.stop();
    m_scheduler.reset();
    m_driftController.reset();
    m_scheduleTimer.stop();

    m_mediaMode = TelepartyMediaMode::LocalFile;
    m_hostLocalDescriptor = incoming;
    m_guestLocalDescriptor = LocalMediaDescriptor{};
    m_localFilePath.clear();
    m_currentUrl.clear();
    m_currentTitle = incoming.displayTitle;
    m_partyDurationMs = incoming.durationMs;
    m_partyTimeMs = posMs;
    m_partyPlaying = hostPlaying;
    m_partyUpdatedLocalMs = localNowMs();
    m_localPlaybackCommitted = false;
    m_localPlaybackAligned = false;
    m_hostSessionAlreadyStarted = hostCommitted;
    m_localReadinessWaitTimedOut = false;
    m_localReadinessWaitStartedMs = m_partyUpdatedLocalMs;
    m_localVerificationState = LocalVerificationState::WaitingForLocalSelection;
    m_localCompatibilityReason = LocalCompatibilityReason::None;

    // Pause any previously playing media until the user selects and verifies the new local video
    if (m_player && !m_player->isPaused()) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remoteSetRate(1.0);
        m_player->remotePause();
    }

    TimelineSnapshot snap;
    snap.sessionId = m_session->code();
    snap.revision = trev;
    snap.authorId = from;
    snap.mediaId = incoming.mediaId;
    snap.mediaUrl = QString();
    snap.mediaTitle = incoming.displayTitle;
    snap.durationMs = incoming.durationMs;
    snap.playing = hostPlaying;
    snap.playbackRate = 1.0;
    snap.valid = true;
    if (rawHt > 0 && m_clockEstimator.hasEstimate()) {
        snap.basePositionMs = posMs;
        snap.hostTimestampMs = rawHt;
        m_reanchorStateOnFirstClockLock = false;
    } else {
        const qint64 oneWayCompMs = hostPlaying ? ClockEstimator::kDefaultOneWayMs : 0;
        snap.basePositionMs = AuthoritativeTimeline::sanitizePositionMs(posMs + oneWayCompMs, incoming.durationMs);
        snap.hostTimestampMs = estimatedHostNowMs();
        if (rawHt > 0) {
            m_reanchorStateOnFirstClockLock = true;
            m_rawStateHostTimestampMs = rawHt;
            m_rawStateBasePositionMs = posMs;
        }
    }
    m_timeline.forceUpdate(snap);

    if (m_localReadinessTimeoutMs > 0) {
        m_localReadinessTimeoutTimer.start(static_cast<int>(std::min<qint64>(m_localReadinessTimeoutMs, 600000)));
    }

    sendLocalVerificationReport();

    emit currentMediaChanged();
    emit localVerificationChanged();
    emit diagnosticsChanged();
    emit partyMediaAvailable(QStringLiteral("local://") + incoming.mediaId,
                             incoming.displayTitle,
                             posMs,
                             incoming.durationMs);
    emit localMediaPromptRequested(incoming.displayTitle, incoming.durationMs, incoming.containerFormat);
    emit activity(QStringLiteral("A participant selected local video: %1 — choose your matching local copy in Library")
                      .arg(incoming.displayTitle));
}

bool TelepartySync::selectGuestLocalMedia(const QString &fileUrlOrPath,
                                          const QString &title,
                                          qint64 knownDurationMs,
                                          bool isQuietAutoMatch) {
    if (!m_session || !m_session->active()) {
        return false;
    }
    if (!m_hostLocalDescriptor.isValidForSession()) {
        return broadcastLocalOpen(fileUrlOrPath, title, knownDurationMs);
    }
    if (m_localMediaAuthorityPeerId == m_session->memberId()) {
        FLUX_LOG_WARN("Teleparty", "The media leader already selected this local video");
        return false;
    }
    if (m_mediaMode != TelepartyMediaMode::LocalFile) {
        FLUX_LOG_WARN("Teleparty", "Cannot verify guest local media: no active host LocalMediaDescriptor");
        return false;
    }

    QString localPath = fileUrlOrPath.trimmed();
    if (localPath.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
        localPath = QUrl(localPath).toLocalFile();
    }
    const QString resolvedUrl = QUrl::fromLocalFile(localPath).toString();
    const qint64 effectiveDurationMs =
        (knownDurationMs > 0)
            ? knownDurationMs
            : ((m_player && m_player->url() == resolvedUrl && m_player->durationMs() > 0)
                   ? m_player->durationMs()
                   : 0);

    cancelActiveInspection();
    m_localVerificationState = LocalVerificationState::InspectingLocalMedia;
    m_localCompatibilityReason = LocalCompatibilityReason::None;
    emit localVerificationChanged();

    if (m_forceAsyncInspection) {
        m_activeInspectCancel = LocalMediaInspector::inspectFileAsync(
            this,
            localPath,
            title,
            effectiveDurationMs,
            m_verificationPolicy.requireExactFullFileHash,
            [this, resolvedUrl, localPath, isQuietAutoMatch](LocalMediaDescriptor guestDesc, LocalCompatibilityReason errReason) {
                finishGuestLocalInspection(resolvedUrl, localPath, std::move(guestDesc), errReason, isQuietAutoMatch);
            },
            m_verificationPolicy.requireContentFingerprint);
        return true;
    }

    LocalCompatibilityReason errReason = LocalCompatibilityReason::None;
    LocalMediaDescriptor guestDesc = LocalMediaInspector::inspectFile(
        localPath,
        title,
        effectiveDurationMs,
        m_verificationPolicy.requireExactFullFileHash,
        &errReason,
        nullptr,
        true,
        m_verificationPolicy.requireContentFingerprint);
    finishGuestLocalInspection(resolvedUrl, localPath, std::move(guestDesc), errReason, isQuietAutoMatch);
    return m_localVerificationState == LocalVerificationState::VerifiedCompatible ||
           m_localVerificationState == LocalVerificationState::OpeningMedia ||
           m_localVerificationState == LocalVerificationState::BufferingMedia ||
           m_localVerificationState == LocalVerificationState::ReadyToSync;
}

void TelepartySync::finishGuestLocalInspection(const QString &resolvedUrl,
                                               const QString &resolvedLocalPath,
                                               LocalMediaDescriptor guestDesc,
                                               LocalCompatibilityReason errReason,
                                               bool isQuietAutoMatch) {
    m_activeInspectCancel.reset();
    if (!m_session || !m_session->active() || m_mediaMode != TelepartyMediaMode::LocalFile) {
        return;
    }

    // Handle file-level inspection failures (missing, empty/unreadable, unsupported container)
    if (errReason == LocalCompatibilityReason::MissingFile ||
        errReason == LocalCompatibilityReason::UnreadableOrEmptyFile) {
        if (!isQuietAutoMatch) {
            m_localVerificationState = LocalVerificationState::PlaybackError;
            m_localCompatibilityReason = errReason;
            m_guestLocalDescriptor = LocalMediaDescriptor{};
            m_localFilePath.clear();
            m_currentUrl.clear();
            m_localPlaybackCommitted = false;
            if (m_player && !m_player->isPaused()) {
                ScopedInternalAction guard(m_internalActionDepth);
                m_player->remotePause();
            }
            sendLocalVerificationReport();
            emit currentMediaChanged();
            emit localVerificationChanged();
            emit activity(localCompatibilityReasonToMessage(errReason));
        } else {
            emit localCandidateCheckFailed(localCompatibilityReasonToMessage(errReason));
        }
        return;
    }

    const LocalCompatibilityResult verifyRes =
        LocalMediaVerifier::verify(m_hostLocalDescriptor, guestDesc, m_verificationPolicy);

    if (!verifyRes.isCompatible()) {
        if (!isQuietAutoMatch) {
            m_localVerificationState = verifyRes.state;
            m_localCompatibilityReason = verifyRes.reason;
            m_guestLocalDescriptor = guestDesc;
            m_localFilePath.clear();
            m_currentUrl.clear();
            m_localPlaybackCommitted = false;
            if (m_player && !m_player->isPaused()) {
                ScopedInternalAction guard(m_internalActionDepth);
                m_player->remotePause();
            }
            sendLocalVerificationReport();
            emit currentMediaChanged();
            emit localVerificationChanged();
            emit activity(verifyRes.userMessage);
        } else {
            emit localCandidateCheckFailed(verifyRes.userMessage);
        }
        return;
    }

    // Compatible!
    m_guestLocalDescriptor = guestDesc;
    m_localFilePath = resolvedLocalPath;
    m_currentUrl = resolvedUrl;
    m_currentTitle = m_hostLocalDescriptor.displayTitle;
    m_localCompatibilityReason = verifyRes.reason;
    m_localVerificationState = LocalVerificationState::VerifiedCompatible;
    m_localReadinessWaitTimedOut = false;
    m_localPlaybackCommitted = m_hostSessionAlreadyStarted && m_timeline.isValid() &&
                               m_timeline.snapshot().playing;

    TimelineSnapshot snap = m_timeline.snapshot();
    snap.mediaUrl = m_currentUrl;
    m_timeline.forceUpdate(snap);

    const qint64 initialPos = m_timeline.isValid()
                                  ? m_timeline.expectedPositionMs(estimatedHostNowMs())
                                  : m_partyTimeMs;

    if (m_player && m_player->url() != m_currentUrl) {
        QString playerLocalPath = m_player->url().trimmed();
        if (playerLocalPath.startsWith(QLatin1String("file://"), Qt::CaseInsensitive)) {
            playerLocalPath = QUrl(playerLocalPath).toLocalFile();
        }
        if (!playerLocalPath.isEmpty() &&
            QFileInfo(playerLocalPath).absoluteFilePath() == QFileInfo(resolvedLocalPath).absoluteFilePath()) {
            m_player->setUrl(m_currentUrl);
        } else {
            emit remoteOpenRequested(m_currentUrl, m_currentTitle, initialPos);
        }
    }

    updateLocalVerificationReadiness(false);
    sendLocalVerificationReport();
    scheduleLocalPlaybackResume();

    if (m_localVerificationState == LocalVerificationState::ReadyToSync && m_hostSessionAlreadyStarted) {
        m_localPlaybackCommitted = true;
        m_localReadinessTimeoutTimer.stop();
        align(initialPos,
              m_timeline.snapshot().playing,
              QStringLiteral("Verified compatible — synchronized with Local Teleparty"),
              ActionOrigin::JoinSync);
    } else if (m_localVerificationState == LocalVerificationState::ReadyToSync &&
               m_player && m_player->url() == m_currentUrl && !m_localPlaybackCommitted) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remoteSetRate(1.0);
        if (m_player->isPlaying()) {
            m_player->remotePause();
        }
        if (m_player->isSeekable() &&
            std::llabs(m_player->timeMs() - initialPos) > m_driftController.config().pausedSeekThresholdMs) {
            m_player->remoteSeekTo(initialPos);
            m_driftController.notifyExternalSeek(localNowMs(), initialPos);
        }
    }

    emit currentMediaChanged();
    emit localVerificationChanged();
    emit diagnosticsChanged();
}

void TelepartySync::sendLocalVerificationReport() {
    if (!m_session || !m_session->active() ||
        m_localMediaAuthorityPeerId == m_session->memberId()) {
        return;
    }
    if (m_mediaMode != TelepartyMediaMode::LocalFile || !m_hostLocalDescriptor.isValidForSession()) {
        return;
    }

    QVariantMap d;
    d.insert(QStringLiteral("sid"), m_session->code());
    d.insert(QStringLiteral("mid"), m_hostLocalDescriptor.mediaId);
    d.insert(QStringLiteral("rev"), m_hostLocalDescriptor.revision);
    d.insert(QStringLiteral("st"), localVerificationStateToCode(m_localVerificationState));
    d.insert(QStringLiteral("rs"), localCompatibilityReasonToCode(m_localCompatibilityReason));
    if (m_guestLocalDescriptor.durationMs > 0 && m_hostLocalDescriptor.durationMs > 0) {
        d.insert(QStringLiteral("dd"), std::llabs(m_hostLocalDescriptor.durationMs - m_guestLocalDescriptor.durationMs));
    }

    if (!LocalMediaDescriptor::containsPrivatePathLeakInJson(QJsonObject::fromVariantMap(d))) {
        m_session->sendEvent(QStringLiteral("lver"), d);
    }
}

void TelepartySync::handleLocalVerifyReport(const QVariantMap &data, const QString &from) {
    if (!m_session || !m_session->active() || m_mediaMode != TelepartyMediaMode::LocalFile) {
        return;
    }
    if (LocalMediaDescriptor::containsPrivatePathLeakInJson(QJsonObject::fromVariantMap(data))) {
        FLUX_LOG_WARN("Teleparty", "Rejected 'lver' report containing private path leak");
        return;
    }

    const QString mid = data.value(QStringLiteral("mid")).toString().trimmed();
    const qint64 rev = data.value(QStringLiteral("rev"), 0).toLongLong();
    if (mid != m_hostLocalDescriptor.mediaId || rev != m_hostLocalDescriptor.revision) {
        return; // Ignore verification report for an older or different media declaration
    }

    ParticipantLocalStatus st;
    st.peerId = from;
    st.mediaId = mid;
    st.revision = rev;
    st.state = localVerificationStateFromCode(data.value(QStringLiteral("st")).toString());
    st.reason = localCompatibilityReasonFromCode(data.value(QStringLiteral("rs")).toString());
    st.durationDeltaMs = data.value(QStringLiteral("dd"), 0).toLongLong();
    st.updatedAtLocalMs = localNowMs();
    m_peerLocalStatus.insert(from, st);

    if (m_localMediaAuthorityPeerId == m_session->memberId()) {
        tryCommitLocalPlaybackStart();
    }
    emit localVerificationChanged();
    emit diagnosticsChanged();
}

void TelepartySync::tryCommitLocalPlaybackStart() {
    if (!m_session || !m_session->active() ||
        m_localMediaAuthorityPeerId != m_session->memberId()) {
        return;
    }
    if (m_mediaMode != TelepartyMediaMode::LocalFile || m_localPlaybackCommitted) {
        return;
    }
    // Start as soon as the host's own copy is ready. Guests without a matching
    // local file remain paused and can join later after selecting their copy.
    if (m_localVerificationState != LocalVerificationState::ReadyToSync ||
        !m_hostLocalDescriptor.isValidForSession() || m_currentUrl.isEmpty()) {
        return;
    }

    m_localPlaybackCommitted = true;
    m_localPlaybackAligned = true;
    m_hostSessionAlreadyStarted = true;
    m_localReadinessWaitTimedOut = false;
    m_localReadinessTimeoutTimer.stop();

    const qint64 posMs = AuthoritativeTimeline::sanitizePositionMs(
        m_timeline.snapshot().basePositionMs, m_partyDurationMs);
    const qint64 hostNow = estimatedHostNowMs();
    const qint64 leadMs = m_useScheduledLeadTime ? scheduledLeadTimeMs() : 0;
    const qint64 targetHostMs = hostNow + leadMs;
    const qint64 rev = m_timeline.nextRevision();

    m_partyTimeMs = posMs;
    m_partyPlaying = true;
    m_partyUpdatedLocalMs = localNowMs();

    TimelineSnapshot snap = m_timeline.snapshot();
    snap.sessionId = m_session->code();
    snap.revision = rev;
    snap.authorId = m_session->memberId();
    snap.mediaId = m_hostLocalDescriptor.mediaId;
    snap.mediaUrl = m_currentUrl;
    snap.mediaTitle = m_currentTitle;
    snap.durationMs = m_hostLocalDescriptor.durationMs;
    snap.basePositionMs = posMs;
    snap.hostTimestampMs = targetHostMs;
    snap.playing = true;
    snap.playbackRate = 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);
    m_scheduler.cancelSuperseded(rev);

    QVariantMap d;
    d.insert(QStringLiteral("sid"), m_session->code());
    d.insert(QStringLiteral("mid"), m_hostLocalDescriptor.mediaId);
    d.insert(QStringLiteral("mrev"), m_hostLocalDescriptor.revision);
    d.insert(QStringLiteral("rev"), rev);
    d.insert(QStringLiteral("t"), posMs);
    d.insert(QStringLiteral("ht"), targetHostMs);
    d.insert(QStringLiteral("p"), 1);
    m_session->sendEvent(QStringLiteral("lstart"), d);

    align(posMs, true, QStringLiteral("Starting Local Teleparty"), ActionOrigin::ScheduledRemote);
    emit localVerificationChanged();
    emit diagnosticsChanged();
}

void TelepartySync::handleLocalStartCommit(const QVariantMap &data, const QString &from) {
    if (!m_session || !m_session->active() || m_mediaMode != TelepartyMediaMode::LocalFile) {
        return;
    }

    if (from != m_localMediaAuthorityPeerId || from == m_session->memberId()) {
        return;
    }

    const QString mid = data.value(QStringLiteral("mid")).toString().trimmed();
    const qint64 mrev = data.value(QStringLiteral("mrev"), m_hostLocalDescriptor.revision).toLongLong();
    if (mid != m_hostLocalDescriptor.mediaId || mrev != m_hostLocalDescriptor.revision) {
        return;
    }

    m_hostSessionAlreadyStarted = true;
    const bool playing = (data.value(QStringLiteral("p"), 1).toInt() != 0);
    m_localReadinessTimeoutTimer.stop();
    const qint64 t = cleanTime(data.value(QStringLiteral("t"), 0));
    const qint64 rawHt = data.value(QStringLiteral("ht"), 0).toLongLong();
    const qint64 rev = data.value(QStringLiteral("rev"), m_timeline.nextRevision()).toLongLong();
    const qint64 hostTimestamp = (rawHt > 0 && m_clockEstimator.hasEstimate())
                                     ? rawHt
                                     : estimatedHostNowMs();

    TimelineSnapshot snap = m_timeline.snapshot();
    snap.sessionId = m_session->code();
    snap.revision = std::max<qint64>(snap.revision, rev);
    snap.authorId = from;
    snap.mediaId = mid;
    snap.mediaUrl = m_currentUrl;
    snap.mediaTitle = m_hostLocalDescriptor.displayTitle;
    snap.durationMs = m_hostLocalDescriptor.durationMs;
    snap.basePositionMs = t;
    snap.hostTimestampMs = hostTimestamp;
    snap.playing = playing;
    snap.playbackRate = 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);

    // Keep the host's start state even if the guest decoder is still opening. The
    // verified local copy can start as soon as VLC accepts playback; readiness for
    // seek corrections is tracked separately by updateLocalVerificationReadiness().
    if (m_currentUrl.isEmpty()) {
        emit localVerificationChanged();
        return;
    }

    m_localPlaybackCommitted = true;
    m_localReadinessWaitTimedOut = false;
    m_localReadinessTimeoutTimer.stop();
    if (m_player && isSameOrMatchingMedia(m_player->url(), m_currentUrl) &&
        m_localVerificationState == LocalVerificationState::ReadyToSync) {
        m_localPlaybackAligned = true;
        const qint64 livePosition = m_timeline.expectedPositionMs(estimatedHostNowMs());
        align(livePosition,
              playing,
              playing ? QStringLiteral("Starting Local Teleparty") : QString(),
              ActionOrigin::JoinSync);
        scheduleLocalPlaybackResume();
    }
    emit localVerificationChanged();
}

void TelepartySync::scheduleLocalPlaybackResume() {
    if (m_localResumeCheckPending || m_mediaMode != TelepartyMediaMode::LocalFile ||
        !m_localPlaybackCommitted || !m_timeline.isValid() || !m_timeline.snapshot().playing ||
        !m_player || m_player->state() != QLatin1String("Paused") ||
        !isSameOrMatchingMedia(m_player->url(), m_currentUrl)) {
        return;
    }

    m_localResumeCheckPending = true;
    const QString mediaId = m_hostLocalDescriptor.mediaId;
    QTimer::singleShot(350, this, [this, mediaId]() {
        m_localResumeCheckPending = false;
        if (m_mediaMode == TelepartyMediaMode::LocalFile && m_localPlaybackCommitted &&
            m_hostLocalDescriptor.mediaId == mediaId && m_timeline.isValid() &&
            m_timeline.snapshot().playing && m_player &&
            isSameOrMatchingMedia(m_player->url(), m_currentUrl) &&
            m_player->state() == QLatin1String("Paused")) {
            align(m_timeline.expectedPositionMs(estimatedHostNowMs()),
                  true,
                  QStringLiteral("Synchronized with Local Teleparty"),
                  ActionOrigin::JoinSync);
        }
    });
}

void TelepartySync::updateLocalVerificationReadiness(bool allowBroadcast) {
    if (m_mediaMode != TelepartyMediaMode::LocalFile || !m_session || !m_session->active()) {
        return;
    }

    // Do not overwrite pre-verification or failed verification states
    if (m_localVerificationState == LocalVerificationState::None ||
        m_localVerificationState == LocalVerificationState::WaitingForLocalSelection ||
        m_localVerificationState == LocalVerificationState::InspectingLocalMedia ||
        m_localVerificationState == LocalVerificationState::IncompatibleMedia ||
        m_localVerificationState == LocalVerificationState::UnresolvedMetadata) {
        return;
    }

    if (m_currentUrl.isEmpty()) {
        return;
    }

    // Detect if the local video file on disk was replaced, modified, or deleted during the session
    if (!m_localFilePath.isEmpty() &&
        m_guestLocalDescriptor.fileSizeBytes > 0 &&
        LocalMediaInspector::hasFileChangedSince(m_localFilePath, m_guestLocalDescriptor)) {
        FLUX_LOG_WARN("Teleparty", "Local media file changed or disappeared on disk; invalidating verification");
        const QFileInfo fi(m_localFilePath);
        m_localVerificationState = fi.exists()
                                       ? LocalVerificationState::IncompatibleMedia
                                       : LocalVerificationState::PlaybackError;
        m_localCompatibilityReason = fi.exists()
                                         ? LocalCompatibilityReason::FingerprintMismatch
                                         : LocalCompatibilityReason::MissingFile;
        m_localPlaybackCommitted = false;
        m_currentUrl.clear();
        if (m_player && !m_player->isPaused()) {
            ScopedInternalAction guard(m_internalActionDepth);
            m_player->remotePause();
        }
        if (allowBroadcast) {
            sendLocalVerificationReport();
        }
        emit currentMediaChanged();
        emit localVerificationChanged();
        emit activity(localCompatibilityReasonToMessage(m_localCompatibilityReason));
        return;
    }

    LocalVerificationState nextState = m_localVerificationState;
    if (!m_player) {
        nextState = LocalVerificationState::ReadyToSync;
    } else if (!isSameOrMatchingMedia(m_player->url(), m_currentUrl)) {
        nextState = LocalVerificationState::OpeningMedia;
    } else {
        const QString st = m_player->state();
        if (st == QLatin1String("Error")) {
            nextState = LocalVerificationState::PlaybackError;
            m_localCompatibilityReason = LocalCompatibilityReason::UnplayableMedia;
            m_localPlaybackCommitted = false;
        } else if (st == QLatin1String("Opening") || st == QLatin1String("Idle")) {
            nextState = LocalVerificationState::OpeningMedia;
        } else if (m_player->isBuffering() || st.startsWith(QLatin1String("Buffering"))) {
            nextState = LocalVerificationState::BufferingMedia;
        } else if (st == QLatin1String("Playing") || st == QLatin1String("Paused")) {
            // A local file can report non-seekable briefly while VLC is opening its
            // demuxer. Playback readiness must not wait for seeking to become available.
            nextState = LocalVerificationState::ReadyToSync;
        } else if (m_localPlaybackCommitted) {
            nextState = LocalVerificationState::ReadyToSync;
        } else {
            nextState = LocalVerificationState::PlaybackError;
            m_localCompatibilityReason = LocalCompatibilityReason::UnplayableMedia;
            m_localPlaybackCommitted = false;
        }
    }

    // Hold local player paused at the initial position until playback start is committed
    const bool startingAfterHost = m_hostSessionAlreadyStarted &&
                                   nextState == LocalVerificationState::ReadyToSync;
    if (!m_localPlaybackCommitted && !startingAfterHost && m_player &&
        isSameOrMatchingMedia(m_player->url(), m_currentUrl) && m_player->isPlaying()) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remotePause();
    }

    if (nextState != m_localVerificationState) {
        m_localVerificationState = nextState;
        if (allowBroadcast && m_localMediaAuthorityPeerId != m_session->memberId()) {
            sendLocalVerificationReport();
        }
        if (m_localMediaAuthorityPeerId == m_session->memberId()) {
            tryCommitLocalPlaybackStart();
        } else if (m_localVerificationState == LocalVerificationState::ReadyToSync &&
                   (m_hostSessionAlreadyStarted || m_localPlaybackCommitted) &&
                   !m_localPlaybackAligned) {
            m_localPlaybackAligned = true;
            m_localPlaybackCommitted = true;
            m_localReadinessTimeoutTimer.stop();
            const qint64 livePos = m_timeline.expectedPositionMs(estimatedHostNowMs());
            align(livePos,
                  m_timeline.isValid() ? m_timeline.snapshot().playing : true,
                  QStringLiteral("Synchronized with Local Teleparty"),
                  ActionOrigin::JoinSync);
            scheduleLocalPlaybackResume();
        }
        emit localVerificationChanged();
    }
}

void TelepartySync::onLocalReadinessTimeout() {
    if (m_mediaMode != TelepartyMediaMode::LocalFile || m_localPlaybackCommitted) {
        return;
    }
    m_localReadinessWaitTimedOut = true;
    if (m_player && !m_player->isPaused()) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remotePause();
    }
    emit localVerificationChanged();
    emit activity(localErrorText());
}

void TelepartySync::retryLocalReadinessWait() {
    if (m_mediaMode != TelepartyMediaMode::LocalFile || !m_session || !m_session->active()) {
        return;
    }
    m_localReadinessWaitTimedOut = false;
    m_localReadinessWaitStartedMs = localNowMs();
    if (m_localReadinessTimeoutMs > 0) {
        m_localReadinessTimeoutTimer.start(static_cast<int>(std::min<qint64>(m_localReadinessTimeoutMs, 600000)));
    }
    if (m_localMediaAuthorityPeerId == m_session->memberId()) {
        sendState();
        tryCommitLocalPlaybackStart();
    } else {
        sendLocalVerificationReport();
        m_session->sendEvent(QStringLiteral("syncme"));
    }
    emit localVerificationChanged();
}

void TelepartySync::cancelLocalMediaSession() {
    if (m_mediaMode != TelepartyMediaMode::LocalFile) {
        return;
    }
    clearLocalSessionState();
    m_currentUrl.clear();
    m_currentTitle.clear();
    m_timeline.reset(m_session ? m_session->code() : QString());
    if (m_player && !m_player->isPaused()) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remotePause();
    }
    emit currentMediaChanged();
    emit diagnosticsChanged();
}

void TelepartySync::processScheduledActions() {
    if (m_scheduler.pendingCount() == 0) {
        return;
    }

    const qint64 estHostNow = estimatedHostNowMs();
    const QList<ScheduledAction> due = m_scheduler.popDueActions(estHostNow);
    for (const ScheduledAction &action : due) {
        executeScheduledAction(action);
    }
    updateScheduleTimer();
    if (!due.isEmpty()) {
        emit diagnosticsChanged();
    }
}

void TelepartySync::updateScheduleTimer() {
    const qint64 delayMs = m_scheduler.nextWakeupDelayMs(estimatedHostNowMs());
    if (delayMs < 0) {
        m_scheduleTimer.stop();
    } else {
        m_scheduleTimer.start(static_cast<int>(delayMs));
    }
}

void TelepartySync::executeScheduledAction(const ScheduledAction &action) {
    if (m_mediaMode == TelepartyMediaMode::LocalFile && !isLocalParticipantSynchronized()) {
        return;
    }
    const bool playerHasPartyMedia = m_player &&
                                     !m_currentUrl.isEmpty() &&
                                     isSameOrMatchingMedia(m_player->url(), m_currentUrl) &&
                                     m_player->state() != QLatin1String("Idle") &&
                                     m_player->state() != QLatin1String("Stopped");

    // Not on the player page (and not currently playing the party video in the background):
    // keep tracking the party's timestamp so clicking Continue Watching / Now Playing starts right there.
    if (!m_watching && !playerHasPartyMedia) {
        return;
    }
    if (m_currentUrl.isEmpty() || !m_player || !isSameOrMatchingMedia(m_player->url(), m_currentUrl)) {
        return;
    }

    if (action.type == ScheduledActionType::Play) {
        align(action.targetPositionMs, true, action.activityNote, action.origin);
    } else if (action.type == ScheduledActionType::Pause) {
        align(action.targetPositionMs, false, action.activityNote, action.origin);
    } else if (action.type == ScheduledActionType::Seek) {
        applySeek(action.targetPositionMs, action.activityNote, action.origin);
        ScopedInternalAction guard(m_internalActionDepth);
        if (action.playing && m_player->isPaused()) {
            m_player->remoteResume();
        } else if (!action.playing && !m_player->isPaused()) {
            m_player->remotePause();
        }
    }
}

void TelepartySync::align(qint64 timeMs, bool playing, const QString &note, ActionOrigin origin) {
    if (!m_player) return;
    ScopedInternalAction guard(m_internalActionDepth);

    const QString st = m_player->state();
    const qint64 localNow = localNowMs();
    const bool isOpeningOrBuffering = (st == QLatin1String("Opening") ||
                                       m_player->isBuffering() ||
                                       st.startsWith(QLatin1String("Buffering")));

    if (st == QLatin1String("Ended") || st == QLatin1String("Stopped")) {
        if (playing) {
            m_player->remoteRestart(timeMs);
            m_driftController.notifyExternalSeek(localNow, timeMs);
        }
    } else if (playing) {
        const qint64 drift = std::llabs(m_player->timeMs() - timeMs);
        const qint64 threshold = (origin == ActionOrigin::JoinSync)
                                     ? kCatchUpDriftMs
                                     : m_driftController.config().hardSeekThresholdMs;
        const bool canSeekNow = !isOpeningOrBuffering &&
                                (origin == ActionOrigin::JoinSync || !m_player->isSeekSettling());
        if (canSeekNow && drift > threshold) {
            m_player->remoteSetRate(1.0);
            m_player->remoteSeekTo(timeMs);
            m_driftController.notifyExternalSeek(localNow, timeMs);
        }
        if (m_player->isPaused()) {
            m_player->remoteResume();
        }
    } else {
        m_player->remoteSetRate(1.0);
        m_player->remotePause();
        const qint64 drift = std::llabs(m_player->timeMs() - timeMs);
        if (!isOpeningOrBuffering && drift > m_driftController.config().pausedSeekThresholdMs) {
            m_player->remoteSeekTo(timeMs);
            m_driftController.notifyExternalSeek(localNow, timeMs);
        }
    }

    if (!note.isEmpty()) emit activity(note);
}

void TelepartySync::applySeek(qint64 timeMs, const QString &note, ActionOrigin) {
    if (!m_player) return;
    ScopedInternalAction guard(m_internalActionDepth);

    m_player->remoteSetRate(1.0);
    m_player->remoteSeekTo(timeMs);
    m_driftController.notifyExternalSeek(localNowMs(), timeMs);
    const QString msg = note.isEmpty() ? (QStringLiteral("Skipped to ") + formatTime(timeMs)) : note;
    emit activity(msg);
}

// ============================================================================
// Periodic Three-Tier Adaptive Drift Controller Loop
// ============================================================================

void TelepartySync::evaluateDriftTick() {
    processScheduledActions();
    updateLocalReadiness(true);
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        updateLocalVerificationReadiness(true);
        if (!m_localPlaybackCommitted &&
            !m_localReadinessWaitTimedOut &&
            m_localReadinessWaitStartedMs > 0 &&
            m_localReadinessTimeoutMs > 0 &&
            (localNowMs() - m_localReadinessWaitStartedMs) >= m_localReadinessTimeoutMs) {
            onLocalReadinessTimeout();
        }
        if (!isLocalParticipantSynchronized()) {
            return;
        }
    }

    if (!m_session || !m_session->active() || !m_watching || !m_player) {
        return;
    }
    if (m_currentUrl.isEmpty() || !isSameOrMatchingMedia(m_player->url(), m_currentUrl) || !m_timeline.isValid()) {
        return;
    }
    if (m_pendingSync || m_needState || m_scheduler.pendingCount() > 0) {
        return;
    }

    const qint64 localNow = localNowMs();
    m_lastDriftTickLocalMs = localNow;

    const QString st = m_player->state();
    if (st == QLatin1String("Idle") || st == QLatin1String("Error")) {
        return;
    }

    PlayerObservation obs;
    obs.actualPositionMs = m_player->timeMs();
    obs.durationMs = m_player->durationMs();
    obs.isPlaying = m_player->isPlaying();
    obs.isPaused = m_player->isPaused();
    obs.isBuffering = m_player->isBuffering() || st.startsWith(QLatin1String("Buffering"));
    obs.isOpening = (st == QLatin1String("Opening"));
    obs.isSeekSettling = m_player->isSeekSettling();
    obs.isSeekable = m_player->isSeekable();
    obs.isEndedOrStopped = (st == QLatin1String("Ended") || st == QLatin1String("Stopped"));
    obs.currentRate = m_player->playbackRate();
    obs.rateControlSupported = false;

    const qint64 estHostNow = estimatedHostNowMs();
    const TimelineSnapshot &snap = m_timeline.snapshot();
    const qint64 expectedPos = m_timeline.expectedPositionMs(estHostNow);

    const DriftCorrectionDecision dec = m_driftController.evaluate(
        obs, expectedPos, snap.playing, snap.playbackRate, m_clockEstimator.uncertaintyMs(), localNow);

    {
        ScopedInternalAction guard(m_internalActionDepth);

        if (dec.shouldRestart) {
            m_player->remoteRestart(dec.targetSeekMs >= 0 ? dec.targetSeekMs : expectedPos);
            emit diagnosticsChanged();
            return;
        }
        if (dec.shouldPause) {
            m_player->remotePause();
        }
        if (dec.shouldResume) {
            m_player->remoteResume();
        }

        switch (dec.mode) {
        case DriftCorrectionMode::RateAdjust: {
            const bool accepted = m_player->remoteSetRate(dec.targetRate);
            if (!accepted) {
                m_driftController.notifyRateControlFailed();
                FLUX_LOG_WARN("Teleparty", "Playback rate adjustment unsupported by stream; falling back to seek control");
            } else if (m_lastSoftLogLocalMs == 0 || (localNow - m_lastSoftLogLocalMs) >= kSoftLogCooldownMs) {
                m_lastSoftLogLocalMs = localNow;
                FLUX_LOG_DEBUG("Teleparty",
                               QString("Soft drift rate adjust: drift=%1ms, rate=%2x, rev=%3")
                                   .arg(dec.signedDriftMs)
                                   .arg(dec.targetRate, 0, 'f', 3)
                                   .arg(snap.revision));
            }
            break;
        }
        case DriftCorrectionMode::RestoreRate: {
            m_player->remoteSetRate(dec.targetRate);
            FLUX_LOG_DEBUG("Teleparty",
                           QString("Restored playback rate to %1x (drift=%2ms)")
                               .arg(dec.targetRate, 0, 'f', 2)
                               .arg(dec.signedDriftMs));
            break;
        }
        case DriftCorrectionMode::HardSeek:
        case DriftCorrectionMode::BufferRecoverySeek: {
            m_player->remoteSetRate(snap.playbackRate);
            if (dec.targetSeekMs >= 0) {
                m_player->remoteSeekTo(dec.targetSeekMs);
                FLUX_LOG_INFO("Teleparty",
                              QString("%1 seek to %2ms (drift=%3ms, rev=%4)")
                                  .arg(dec.mode == DriftCorrectionMode::BufferRecoverySeek ? "Buffer-recovery" : "Hard",
                                       QString::number(dec.targetSeekMs),
                                       QString::number(dec.signedDriftMs),
                                       QString::number(snap.revision)));
            }
            break;
        }
        case DriftCorrectionMode::None:
        case DriftCorrectionMode::SuppressedBuffering:
        case DriftCorrectionMode::SuppressedSeeking:
            break;
        }
    }

    emit diagnosticsChanged();
}

void TelepartySync::stepSimulation() {
    const qint64 now = localNowMs();
    if (m_hasPendingSeek && (now - m_partyUpdatedLocalMs) >= kSeekMergeMs) {
        flushSeek();
    }
    processScheduledActions();

    if (m_session && m_session->active()) {
        if (!m_session->isHost() && m_session->memberCount() > 1) {
            const int pingInterval = m_clockEstimator.recommendedPingIntervalMs();
            if (m_lastClockPingLocalMs == 0 || (now - m_lastClockPingLocalMs) >= pingInterval) {
                sendClockPing();
            }
        }
        if (m_lastDriftTickLocalMs == 0 || (now - m_lastDriftTickLocalMs) >= kDriftTickIntervalMs) {
            evaluateDriftTick();
        }
    }
}

void TelepartySync::onPlayerBufferingChanged() {
    if (!m_player) return;
    updateLocalReadiness(true);
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        updateLocalVerificationReadiness(true);
    }
    // If buffering just started while playback rate was nudged, immediately restore 1.0x
    if (m_player->isBuffering() && std::abs(m_player->playbackRate() - 1.0) > 0.001) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remoteSetRate(1.0);
    }
}

void TelepartySync::onPlayerStateChanged() {
    if (!m_player) return;
    updateLocalReadiness(true);
    if (m_mediaMode == TelepartyMediaMode::LocalFile) {
        updateLocalVerificationReadiness(true);
        scheduleLocalPlaybackResume();
        return;
    }

    const QString st = m_player->state();
    if (st == QLatin1String("Error")) {
        m_pendingSync = false;
        m_syncOnPlay = false;
        m_announceOnPlay = false;
        return;
    }
    if (st != QLatin1String("Playing")) return;

    if (m_announceOnPlay) {
        m_announceOnPlay = false;
        sendState();
    }
    if (m_syncOnPlay) {
        m_syncOnPlay = false;
        armNeedState();
        if (m_session) m_session->sendEvent(QStringLiteral("syncme"));
    }

    if (!m_pendingSync) return;

    m_pendingSync = false;
    // Calculate exact expected position on the authoritative timeline (or fallback elapsed clock)
    const qint64 target = m_timeline.isValid()
                              ? m_timeline.expectedPositionMs(estimatedHostNowMs())
                              : (m_pendingTimeMs + (m_pendingPlaying ? std::max<qint64>(0, localNowMs() - m_pendingStartedLocalMs) : 0));
    const bool shouldPlay = m_timeline.isValid() ? m_timeline.snapshot().playing : m_pendingPlaying;

    ScopedInternalAction guard(m_internalActionDepth);
    if (std::llabs(m_player->timeMs() - target) > kCatchUpDriftMs) {
        m_player->remoteSeekTo(target);
        m_driftController.notifyExternalSeek(localNowMs(), target);
    }
    if (!shouldPlay) {
        m_player->remotePause();
    }
}

// ============================================================================
// Session & Authority Bookkeeping
// ============================================================================

void TelepartySync::onAuthorityChanged(const QString &authorityMemberId, bool localIsAuthority) {
    if (localIsAuthority) {
        const bool wasGuest = !m_clockEstimator.isSelfAuthority();
        const qint64 oldEstHostNow = estimatedHostNowMs();
        const qint64 newLocalNow = localNowMs();

        m_clockEstimator.setSelfAuthority(true);
        m_clockEstimator.setAuthorityPeerId(authorityMemberId);
        m_clockSyncTimer.stop();

        if (m_mediaMode == TelepartyMediaMode::LocalFile) {
            if (m_localMediaAuthorityPeerId.isEmpty() ||
                !m_session->memberIds().contains(m_localMediaAuthorityPeerId)) {
                m_localMediaAuthorityPeerId = authorityMemberId;
            }
            if (localIsAuthority) {
                m_localMediaAuthorityPeerId = m_session->memberId();
                sendState();
                tryCommitLocalPlaybackStart();
            }
        }
        if (wasGuest && m_timeline.isValid()) {
            m_timeline.migrateHostClock(authorityMemberId, oldEstHostNow, newLocalNow);
            sendState();
        }
    } else {
        if (m_clockEstimator.authorityPeerId() != authorityMemberId || m_clockEstimator.isSelfAuthority()) {
            m_clockEstimator.reset(false, authorityMemberId);
            if (m_player && m_session && m_session->active()) {
                m_clockSyncTimer.start(m_clockEstimator.recommendedPingIntervalMs());
                sendClockPing();
            }
        }
    }
    emit diagnosticsChanged();
}

void TelepartySync::onSessionStateChanged() {
    if (!m_session) return;
    const QString s = m_session->state();

    if (s == QLatin1String("idle")) {
        resetParty();
    } else if (s == QLatin1String("joined") && m_prevSessionState == QLatin1String("reconnecting")) {
        // Reconnected after a drop: preserve known monotonic clock offset while restarting
        // fast ping bursts, and request current room state
        m_clockEstimator.softResetForReconnect();
        if (m_player && !m_session->isHost()) {
            m_clockSyncTimer.start(m_clockEstimator.recommendedPingIntervalMs());
            sendClockPing();
        }
        armNeedState();
        m_session->sendEvent(QStringLiteral("syncme"));
    }
    m_prevSessionState = s;
}

void TelepartySync::armNeedState() {
    m_needState = true;
    m_stateTimer.start(kStateWaitMs);
}

void TelepartySync::noteOpen(const QString &url, const QString &title, qint64 rev,
                             qint64 hostTimestampMs, const QString &authorId, qint64 startMs) {
    const bool changed = (m_currentUrl != url || m_currentTitle != title);
    m_currentUrl = url;
    m_currentTitle = title;
    m_partyTimeMs = std::max<qint64>(0, startMs);
    m_partyDurationMs = 0;
    m_partyPlaying = true;
    m_partyUpdatedLocalMs = localNowMs();
    m_lastOpenUrl = url;
    m_lastOpenAt = m_partyUpdatedLocalMs;
    m_pendingSync = false;
    m_syncOnPlay = false;
    m_announceOnPlay = false;
    m_reanchorStateOnFirstClockLock = false;

    m_scheduler.reset();
    m_driftController.reset();
    m_scheduleTimer.stop();

    if (m_player && std::abs(m_player->playbackRate() - 1.0) > 0.001) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remoteSetRate(1.0);
    }

    TimelineSnapshot snap;
    snap.sessionId = m_session ? m_session->code() : QString();
    snap.revision = std::max<qint64>(1, rev);
    snap.authorId = authorId;
    snap.mediaId = encodeCatalogMovieId(url);
    snap.mediaUrl = url;
    snap.mediaTitle = title;
    snap.durationMs = 0;
    snap.basePositionMs = m_partyTimeMs;
    snap.hostTimestampMs = hostTimestampMs;
    snap.playing = true;
    snap.playbackRate = 1.0;
    snap.valid = true;
    m_timeline.forceUpdate(snap);

    if (changed) emit currentMediaChanged();
    emit diagnosticsChanged();
}

void TelepartySync::resetParty() {
    const bool hadMedia = hasPartyMedia();
    clearLocalSessionState();
    m_currentUrl.clear();
    m_currentTitle.clear();
    m_partyTimeMs = 0;
    m_partyDurationMs = 0;
    m_partyPlaying = true;
    m_partyUpdatedLocalMs = 0;
    emit partyPlayingChanged();
    m_lastOpenUrl.clear();
    m_lastOpenAt = 0;
    m_needState = false;
    m_pendingSync = false;
    m_syncOnPlay = false;
    m_announceOnPlay = false;
    m_hasPendingSeek = false;
    m_reanchorStateOnFirstClockLock = false;
    m_lastDriftTickLocalMs = 0;
    m_lastClockPingLocalMs = 0;
    m_lastSoftLogLocalMs = 0;
    m_lastReadinessSentLocalMs = 0;
    m_localReadiness = ParticipantReadiness::Idle;
    m_peerReadiness.clear();

    m_stateTimer.stop();
    m_seekTimer.stop();
    m_driftTimer.stop();
    m_clockSyncTimer.stop();
    m_scheduleTimer.stop();

    m_timeline.reset();
    m_clockEstimator.reset();
    m_scheduler.reset();
    m_driftController.reset();

    if (m_player && std::abs(m_player->playbackRate() - 1.0) > 0.001) {
        ScopedInternalAction guard(m_internalActionDepth);
        m_player->remoteSetRate(1.0);
    }

    if (hadMedia) emit currentMediaChanged();
    emit diagnosticsChanged();
}

bool TelepartySync::playerRunning() const {
    if (!m_player) return false;
    const QString st = m_player->state();
    return st == QLatin1String("Playing") || st == QLatin1String("Opening") || st.startsWith(QLatin1String("Buffering"));
}

qint64 TelepartySync::partyTimeMs() const {
    if (m_timeline.isValid()) {
        return m_timeline.expectedPositionMs(estimatedHostNowMs());
    }
    if (m_partyPlaying && m_partyUpdatedLocalMs > 0) {
        return m_partyTimeMs + std::max<qint64>(0, localNowMs() - m_partyUpdatedLocalMs);
    }
    return m_partyTimeMs;
}

// ============================================================================
// Observability & Diagnostics
// ============================================================================

qint64 TelepartySync::scheduledLeadTimeMs() const {
    const int members = m_session ? m_session->memberCount() : 1;
    return m_clockEstimator.recommendedLeadTimeMs(members);
}

qint64 TelepartySync::authoritativePositionMs() const {
    return m_timeline.isValid() ? m_timeline.expectedPositionMs(estimatedHostNowMs()) : m_partyTimeMs;
}

qint64 TelepartySync::actualPositionMs() const {
    return m_player ? m_player->timeMs() : 0;
}

double TelepartySync::currentPlaybackRate() const {
    return m_player ? m_player->playbackRate() : 1.0;
}

SyncDiagnostics TelepartySync::diagnostics() const {
    SyncDiagnostics diag;
    diag.smoothedRttMs = m_clockEstimator.medianRttMs();
    diag.minRttMs = m_clockEstimator.minRttMs();
    diag.worstRttMs = m_clockEstimator.worstRttMs();
    diag.jitterMs = m_clockEstimator.jitterMs();
    diag.clockOffsetMs = m_clockEstimator.clockOffsetMs();
    diag.clockUncertaintyMs = m_clockEstimator.uncertaintyMs();
    diag.clockConfidence = m_clockEstimator.confidence();
    diag.clockLocked = m_clockEstimator.isLocked();
    diag.clockSampleCount = m_clockEstimator.sampleCount();
    diag.rejectedSampleCount = m_clockEstimator.rejectedCount();

    diag.scheduledLeadTimeMs = scheduledLeadTimeMs();
    diag.pendingScheduledCount = m_scheduler.pendingCount();

    diag.currentRevision = m_timeline.revision();
    diag.authorityPeerId = m_session ? m_session->hostMemberId() : m_clockEstimator.authorityPeerId();
    diag.localReadiness = (m_mediaMode == TelepartyMediaMode::LocalFile)
                              ? localVerificationStateToString(m_localVerificationState)
                              : readinessToString(m_localReadiness);
    diag.authoritativePositionMs = authoritativePositionMs();
    diag.actualPositionMs = actualPositionMs();
    diag.signedDriftMs = m_driftController.lastSignedDriftMs();
    diag.smoothedDriftMs = m_driftController.smoothedDriftMs();
    diag.currentPlaybackRate = currentPlaybackRate();
    diag.activeCorrectionMode = activeCorrectionMode();

    diag.softCorrectionCount = m_driftController.softCorrectionCount();
    diag.hardSeekCorrectionCount = m_driftController.hardSeekCount();
    diag.bufferRecoveryCount = m_driftController.bufferRecoveryCount();
    diag.lateEventCount = m_scheduler.lateEventCount();
    diag.supersededActionCount = m_scheduler.supersededCount();
    diag.duplicateEventCount = m_scheduler.duplicateCount() + (m_session ? m_session->duplicateEventCount() : 0);
    diag.staleRevisionDropCount = m_scheduler.staleDroppedCount();
    return diag;
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

