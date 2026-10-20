#include "TelepartySession.h"
#include "../core/Logger.h"

#include <QClipboard>
#include <QCryptographicHash>
#include <QDateTime>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QUuid>
#include <algorithm>

namespace Flux {

namespace {

constexpr int kProtocolVersion = 1;
constexpr int kHeartbeatMs = 10000;          // "presence" every 10 s
constexpr qint64 kMemberTimeoutMs = 32000;   // a member silent this long is dropped
constexpr int kHostDiscoverMs = 1200;        // hosting: wait this long for a clash with an existing room
constexpr int kJoinDiscoverMs = 3500;        // joining: wait this long for someone to answer
constexpr int kMaxHostAttempts = 5;
constexpr int kMaxSeenSeqPerPeer = 32;

QString configuredHost() {
    const QString env = qEnvironmentVariable("FLUX_SYNCPLAY_HOST").trimmed();
    return env.isEmpty() ? QString::fromLatin1(SyncplayClient::kDefaultHost) : env;
}

quint16 configuredPort() {
    bool ok = false;
    const uint p = qEnvironmentVariable("FLUX_SYNCPLAY_PORT").trimmed().toUInt(&ok);
    return (ok && p > 0 && p <= 65535) ? static_cast<quint16>(p) : SyncplayClient::kDefaultPort;
}

qint64 nowMs() {
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

TelepartySession::TelepartySession(QObject *parent)
    : QObject(parent)
    , m_client(new SyncplayClient(this)) {
    m_discoverTimer.setSingleShot(true);
    connect(&m_discoverTimer, &QTimer::timeout, this, &TelepartySession::onDiscoveryTimeout);

    m_heartbeatTimer.setInterval(kHeartbeatMs);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &TelepartySession::onHeartbeat);

    connect(m_client, &SyncplayClient::joinedRoom, this, &TelepartySession::onJoinedRoom);
    connect(m_client, &SyncplayClient::disconnected, this, &TelepartySession::onDisconnected);
    connect(m_client, &SyncplayClient::fluxMessageReceived, this, &TelepartySession::onFluxMessage);
    connect(m_client, &SyncplayClient::peerPresent, this, &TelepartySession::onPeerPresent);
    connect(m_client, &SyncplayClient::peerLeft, this, &TelepartySession::onPeerLeft);

    QSettings settings;
    m_latchedRoomCode = settings.value(QStringLiteral("teleparty/latchedCode"), QString()).toString().trimmed().toUpper();
    connect(m_client, &SyncplayClient::connectionFailed, this, [this](const QString &reason) {
        if (m_state == State::Connecting || m_state == State::Reconnecting) {
            if (reason.contains(QLatin1String("TLS"), Qt::CaseInsensitive) ||
                reason.contains(QLatin1String("certificate"), Qt::CaseInsensitive)) {
                fail(QStringLiteral("Secure connection to Teleparty server failed."));
            } else if (reason.contains(QLatin1String("timed out"), Qt::CaseInsensitive)) {
                fail(QStringLiteral("Connection to Teleparty server timed out."));
            } else {
                fail(QStringLiteral("Couldn't reach the Teleparty service. Check your internet connection."));
            }
        }
    });

    FLUX_LOG_INFO("Teleparty",
                  QString("Configured Syncplay relay %1:%2 (StartTLS + TLS VerifyPeer)")
                      .arg(configuredHost())
                      .arg(configuredPort()));
}

TelepartySession::~TelepartySession() {
    leave();
}

// ============================================================================
// Properties
// ============================================================================

QString TelepartySession::state() const {
    switch (m_state) {
    case State::Idle:         return QStringLiteral("idle");
    case State::Connecting:   return QStringLiteral("connecting");
    case State::Joined:       return QStringLiteral("joined");
    case State::Reconnecting: return QStringLiteral("reconnecting");
    }
    return QStringLiteral("idle");
}

QString TelepartySession::statusText() const {
    switch (m_state) {
    case State::Idle:         return QStringLiteral("Teleparty");
    case State::Connecting:   return QStringLiteral("Teleparty: Connecting\u2026");
    case State::Joined:       return QStringLiteral("Teleparty: Joined");
    case State::Reconnecting: return QStringLiteral("Teleparty: Reconnecting\u2026");
    }
    return QStringLiteral("Teleparty");
}

int TelepartySession::memberCount() const {
    return active() ? static_cast<int>(m_members.size()) + 1 : 0;
}

QStringList TelepartySession::memberIds() const {
    QStringList ids = m_members.keys();
    if (!m_memberId.isEmpty() && active()) {
        ids.append(m_memberId);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

void TelepartySession::setState(State s) {
    if (m_state == s) return;
    m_state = s;
    emit stateChanged();
}

void TelepartySession::setError(const QString &message) {
    if (m_errorText == message) return;
    m_errorText = message;
    emit errorChanged();
}

void TelepartySession::clearError() {
    setError(QString());
}

void TelepartySession::copyCode() const {
    if (m_code.isEmpty()) return;
    if (QClipboard *clipboard = QGuiApplication::clipboard()) clipboard->setText(m_code);
}

// ============================================================================
// Host / join / leave
// ============================================================================

void TelepartySession::setCustomRoomCode(const QString &code) {
    const QString trimmed = code.trimmed().toUpper();
    if (m_customRoomCode == trimmed) return;
    m_customRoomCode = trimmed;
    emit customRoomCodeChanged();
}

void TelepartySession::setLatchedRoomCode(const QString &code) {
    const QString trimmed = code.trimmed().toUpper();
    if (m_latchedRoomCode == trimmed) return;
    m_latchedRoomCode = trimmed;
    QSettings settings;
    if (m_latchedRoomCode.isEmpty()) {
        settings.remove(QStringLiteral("teleparty/latchedCode"));
    } else {
        settings.setValue(QStringLiteral("teleparty/latchedCode"), m_latchedRoomCode);
    }
    emit latchedRoomCodeChanged();
}

void TelepartySession::clearLatchedRoomCode() {
    setLatchedRoomCode(QString());
}

void TelepartySession::host() {
    if (m_state != State::Idle) return;
    setError(QString());
    m_isHost = true;
    m_attemptsLeft = kMaxHostAttempts;
    if (!m_customRoomCode.trimmed().isEmpty()) {
        m_code = m_customRoomCode.trimmed().toUpper();
    } else {
        m_code = generateCode();
    }
    startSession();
}

void TelepartySession::join(const QString &code) {
    if (m_state != State::Idle) return;

    const QString trimmed = code.trimmed().toUpper();
    static const QRegularExpression codeRegex(QStringLiteral("^[A-Z0-9_-]{3,16}$"));
    if (!codeRegex.match(trimmed).hasMatch()) {
        setError(QStringLiteral("Enter a valid code (3-16 characters)."));
        return;
    }

    setError(QString());
    m_isHost = false;
    m_code = trimmed;
    startSession();
}

void TelepartySession::leave() {
    if (m_state == State::Idle) return;

    const bool wasActive = active();
    if (m_state == State::Joined) {
        publishMessage(makeMessage(QStringLiteral("bye")));
    }

    resetConnection();
    m_code.clear();
    m_isHost = false;
    m_hostMemberId.clear();
    setState(State::Idle);
    emit codeChanged();
    emit membersChanged();
    if (wasActive) emit leftSession();
    FLUX_LOG_INFO("Teleparty", "Left the session");
}

void TelepartySession::startSession() {
    const QString host = configuredHost();
    const quint16 port = configuredPort();

    // 11-char username ("FX_" + 8 hex chars) fits comfortably within Syncplay's 16-char limit
    m_memberId = QStringLiteral("FX_") +
                 QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-').left(8);
    m_hostMemberId = m_isHost ? m_memberId : QString();
    m_room = roomNameForCode(m_code);
    m_members.clear();
    m_seenSeqOrder.clear();
    m_seenSeqSet.clear();
    m_duplicateEventCount = 0;
    m_helloSent = false;
    m_discoverTimer.stop();
    m_heartbeatTimer.stop();

    setState(State::Connecting);
    emit codeChanged();
    emit membersChanged();

    FLUX_LOG_INFO("Teleparty",
                  QString("%1 session %2 (room %3) via %4:%5")
                      .arg(m_isHost ? "Hosting" : "Joining", m_code, m_room, host)
                      .arg(port));

    if (!m_simulatedTransport) {
        m_client->disconnectFromServer();
        m_client->connectToServer(host, port, m_memberId, m_room);
    }
}

void TelepartySession::becomeJoined() {
    m_discoverTimer.stop();
    if (m_isHost && m_hostMemberId.isEmpty()) {
        m_hostMemberId = m_memberId;
    }
    setState(State::Joined);
    m_heartbeatTimer.start();
    emit membersChanged();
    if (!m_hostMemberId.isEmpty()) {
        emit authorityChanged(m_hostMemberId, m_isHost);
    }
    emit joinedSession();
    FLUX_LOG_INFO("Teleparty", QString("Joined session %1 (%2 members)").arg(m_code).arg(memberCount()));

    // If a guest entered a room code that starts with a letter and connected successfully,
    // latch it for future quick reconnects.
    if (!m_isHost && !m_code.isEmpty() && m_code.at(0).isLetter()) {
        setLatchedRoomCode(m_code);
    }
}

void TelepartySession::handleHostCollision() {
    m_discoverTimer.stop();
    if (--m_attemptsLeft <= 0) {
        fail(QStringLiteral("Couldn't create a session. Please try again."));
        return;
    }
    if (!m_simulatedTransport) {
        m_client->disconnectFromServer();
    }
    if (!m_customRoomCode.trimmed().isEmpty()) {
        m_code = m_customRoomCode.trimmed().toUpper();
    } else {
        m_code = generateCode();
    }
    startSession();
}

void TelepartySession::fail(const QString &message) {
    FLUX_LOG_WARN("Teleparty", message);
    resetConnection();
    m_code.clear();
    m_isHost = false;
    m_hostMemberId.clear();
    setState(State::Idle);
    emit codeChanged();
    emit membersChanged();
    setError(message);
}

void TelepartySession::resetConnection() {
    m_discoverTimer.stop();
    m_heartbeatTimer.stop();
    if (!m_simulatedTransport) {
        m_client->disconnectFromServer();
    }
    m_members.clear();
    m_seenSeqOrder.clear();
    m_seenSeqSet.clear();
    m_helloSent = false;
    m_room.clear();
}

// ============================================================================
// Connection events
// ============================================================================

void TelepartySession::onJoinedRoom(const QString &room, const QString &username, int maxChatLength) {
    if (!username.isEmpty()) {
        m_memberId = username;
        if (m_isHost) {
            m_hostMemberId = m_memberId;
        }
    }
    if (!room.isEmpty()) {
        m_room = room;
    }

    FLUX_LOG_DEBUG("Teleparty",
                   QString("Handshake complete as '%1' in room '%2' (MaxChat=%3)")
                       .arg(m_memberId, m_room)
                       .arg(maxChatLength));

    if (m_state == State::Connecting) {
        publishMessage(makeMessage(QStringLiteral("hello")));
        if (!m_helloSent) {
            m_helloSent = true;
            // Wait for someone to answer: for a host that means the code is already taken,
            // for a joiner it means the session exists
            m_discoverTimer.start(m_isHost ? kHostDiscoverMs : kJoinDiscoverMs);
        }
    } else if (m_state == State::Reconnecting) {
        setState(State::Joined);
        publishMessage(makeMessage(QStringLiteral("hello"))); // re-announce, learn who is still here
    }
}

void TelepartySession::onDisconnected() {
    if (m_state == State::Joined) {
        setState(State::Reconnecting);
    }
}

void TelepartySession::onPeerPresent(const QString &username) {
    if (username.isEmpty() || username == m_memberId) return;
    if (!username.startsWith(QLatin1String("FX_"))) return;

    if (m_state == State::Connecting && m_discoverTimer.isActive() && m_isHost) {
        // Another FLUX participant is already in this Syncplay room: pick another 5-digit code
        handleHostCollision();
    }
}

void TelepartySession::onPeerLeft(const QString &username) {
    if (username.isEmpty()) return;
    m_seenSeqOrder.remove(username);
    m_seenSeqSet.remove(username);
    removeMember(username);
}

void TelepartySession::onDiscoveryTimeout() {
    if (m_state != State::Connecting) return;

    if (m_isHost) {
        becomeJoined(); // nobody else uses this code: the room is ours
    } else {
        fail(QStringLiteral("No Teleparty session found with that code."));
    }
}

void TelepartySession::onHeartbeat() {
    if (m_state != State::Joined) return;
    publishMessage(makeMessage(QStringLiteral("presence")));
    pruneMembers();
}

// ============================================================================
// Messages
// ============================================================================

bool TelepartySession::isRecognizedEventType(const QString &eventType) {
    static const QSet<QString> kAllowedEvents = {
        QStringLiteral("open"),
        QStringLiteral("play"),
        QStringLiteral("pause"),
        QStringLiteral("seek"),
        QStringLiteral("state"),
        QStringLiteral("syncme"),
        QStringLiteral("ts"),
        QStringLiteral("rd"),
        QStringLiteral("lopen"),
        QStringLiteral("lver"),
        QStringLiteral("lstart")
    };
    return kAllowedEvents.contains(eventType);
}

bool TelepartySession::recordAndCheckDuplicateSeq(const QString &from, qint64 seq) {
    if (seq <= 0) {
        return false;
    }

    QSet<qint64> &seenSet = m_seenSeqSet[from];
    if (seenSet.contains(seq)) {
        ++m_duplicateEventCount;
        return true;
    }

    QList<qint64> &seenOrder = m_seenSeqOrder[from];
    seenSet.insert(seq);
    seenOrder.append(seq);
    while (seenOrder.size() > kMaxSeenSeqPerPeer) {
        const qint64 oldest = seenOrder.takeFirst();
        seenSet.remove(oldest);
    }
    return false;
}

void TelepartySession::onFluxMessage(const QString &senderUsername, const QJsonObject &o) {
    if (m_state == State::Idle) return;
    if (o.value(QStringLiteral("v")).toInt() != kProtocolVersion) return;

    QString from = o.value(QStringLiteral("f")).toString();
    if (from.isEmpty()) {
        from = o.value(QStringLiteral("from")).toString();
    }
    const QString type = o.value(QStringLiteral("t")).toString();
    if (from.isEmpty() || from.size() > 32 || from == m_memberId) return;

    // Anti-spoofing: sender identity in payload must match the Syncplay server's Chat username
    if (!senderUsername.isEmpty() && from != senderUsername) {
        FLUX_LOG_WARN("Teleparty",
                      QString("Rejected spoofed FLUX message: payload from='%1' != chat username='%2'")
                          .arg(from, senderUsername));
        return;
    }

    if (type == QLatin1String("bye")) {
        m_seenSeqOrder.remove(from);
        m_seenSeqSet.remove(from);
        removeMember(from);
        return;
    }

    if (type != QLatin1String("hello") &&
        type != QLatin1String("presence") &&
        type != QLatin1String("event")) {
        FLUX_LOG_WARN("Teleparty", QString("Rejected unknown FLUX message type '%1'").arg(type));
        return;
    }

    touchMember(from);

    if (o.value(QStringLiteral("h")).toInt(0) == 1) {
        updateHostAuthority(from);
    } else if (m_hostMemberId.isEmpty() && !m_isHost) {
        updateHostAuthority(from);
    }

    if (type == QLatin1String("hello")) {
        // Reset sequence tracking for new / reconnected member
        m_seenSeqOrder.remove(from);
        m_seenSeqSet.remove(from);
        // Let the newcomer know we exist, and let the sync layer share the current state
        publishMessage(makeMessage(QStringLiteral("presence")));
        if (m_state == State::Joined) {
            emit memberJoined(from);
        }

    } else if (type == QLatin1String("presence")) {
        if (m_state == State::Connecting && m_discoverTimer.isActive()) {
            if (m_isHost) {
                handleHostCollision();
            } else {
                becomeJoined(); // found the session
            }
        }

    } else if (type == QLatin1String("event")) {
        if (m_state == State::Joined || m_state == State::Reconnecting) {
            const QString eventType = o.value(QStringLiteral("e")).toString();
            if (!isRecognizedEventType(eventType)) {
                FLUX_LOG_WARN("Teleparty", QString("Rejected unrecognized FLUX event '%1'").arg(eventType));
                return;
            }

            // Deduplicate events by sequence number per sender across a bounded window
            const qint64 seq = o.value(QStringLiteral("i")).toInteger(0);
            if (recordAndCheckDuplicateSeq(from, seq)) {
                return;
            }

            QVariantMap data = o.value(QStringLiteral("d")).toObject().toVariantMap();
            if (seq > 0) {
                data.insert(QStringLiteral("_seq"), seq);
            }
            emit eventReceived(eventType, data, from);
        }
    }
}

bool TelepartySession::sendEvent(const QString &type, const QVariantMap &data) {
    if (m_state != State::Joined || !isRecognizedEventType(type)) return false;
    return publishMessage(makeMessage(QStringLiteral("event"), type, data));
}

QJsonObject TelepartySession::makeMessage(const QString &type, const QString &eventType,
                                          const QVariantMap &data) {
    QJsonObject o;
    o.insert(QStringLiteral("v"), kProtocolVersion);
    o.insert(QStringLiteral("t"), type);
    o.insert(QStringLiteral("f"), m_memberId);
    if (m_isHost && (type == QLatin1String("hello") || type == QLatin1String("presence"))) {
        o.insert(QStringLiteral("h"), 1);
    }
    if (!eventType.isEmpty()) {
        o.insert(QStringLiteral("i"), ++m_nextEventId);
        o.insert(QStringLiteral("e"), eventType);
        if (!data.isEmpty()) {
            QVariantMap cleaned = data;
            cleaned.remove(QStringLiteral("_seq"));
            o.insert(QStringLiteral("d"), QJsonObject::fromVariantMap(cleaned));
        }
    }
    return o;
}

bool TelepartySession::publishMessage(const QJsonObject &message) {
    if (m_room.isEmpty()) {
        return false;
    }
    if (m_simulatedTransport) {
        return m_simulatedTransport(m_memberId, message);
    }
    return m_client->sendFluxMessage(message);
}

// ============================================================================
// Members & Deterministic Host Migration
// ============================================================================

bool TelepartySession::touchMember(const QString &id) {
    const bool isNew = !m_members.contains(id);
    m_members.insert(id, nowMs());
    if (isNew) emit membersChanged();
    return isNew;
}

void TelepartySession::removeMember(const QString &id) {
    if (m_members.remove(id)) {
        emit membersChanged();
        emit memberLeft(id);
        if (id == m_hostMemberId) {
            electNewHostIfNeeded();
        }
    }
}

void TelepartySession::pruneMembers() {
    const qint64 cutoff = nowMs() - kMemberTimeoutMs;
    QStringList removed;
    for (auto it = m_members.begin(); it != m_members.end();) {
        if (it.value() < cutoff) {
            m_seenSeqOrder.remove(it.key());
            m_seenSeqSet.remove(it.key());
            removed.append(it.key());
            it = m_members.erase(it);
        } else {
            ++it;
        }
    }
    if (!removed.isEmpty()) {
        emit membersChanged();
        for (const QString &id : removed) {
            emit memberLeft(id);
        }
        if (removed.contains(m_hostMemberId)) {
            electNewHostIfNeeded();
        }
    }
}

void TelepartySession::updateHostAuthority(const QString &candidateHostId) {
    if (candidateHostId.isEmpty() || m_hostMemberId == candidateHostId) {
        return;
    }
    // If we are the host, only yield if we are not yet joined (collision)
    if (m_isHost && m_state == State::Joined) {
        return;
    }
    m_hostMemberId = candidateHostId;
    m_isHost = (m_hostMemberId == m_memberId);
    if (m_state == State::Joined) {
        emit authorityChanged(m_hostMemberId, m_isHost);
    }
}

void TelepartySession::electNewHostIfNeeded() {
    if (!active()) {
        return;
    }
    // Deterministic leader election: sort all active member IDs in the room
    // and select the lexicographically lowest ID as the new host.
    const QStringList candidates = memberIds();
    if (candidates.isEmpty()) {
        return;
    }

    const QString elected = candidates.front();
    const bool wasHost = m_isHost;
    m_hostMemberId = elected;
    m_isHost = (elected == m_memberId);

    FLUX_LOG_INFO("Teleparty",
                  QString("Host migrated to '%1' (localIsHost=%2)")
                      .arg(m_hostMemberId, m_isHost ? "true" : "false"));

    if (wasHost != m_isHost) {
        emit codeChanged();
    }
    emit authorityChanged(m_hostMemberId, m_isHost);

    if (m_isHost && m_state == State::Joined) {
        publishMessage(makeMessage(QStringLiteral("presence")));
    }
}

// ============================================================================
// Simulation Hooks for Deterministic Testing
// ============================================================================

void TelepartySession::setSimulatedTransport(TransportHook hook) {
    m_simulatedTransport = std::move(hook);
}

void TelepartySession::simulateJoinRoom(const QString &code, bool asHost, const QString &fixedMemberId) {
    setError(QString());
    m_isHost = asHost;
    m_code = code.trimmed();
    m_room = roomNameForCode(m_code);
    m_memberId = fixedMemberId.isEmpty()
                     ? (QStringLiteral("FX_") + QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-').left(8))
                     : fixedMemberId;
    m_hostMemberId = asHost ? m_memberId : QString();
    m_members.clear();
    m_seenSeqOrder.clear();
    m_seenSeqSet.clear();
    m_duplicateEventCount = 0;
    m_helloSent = true;
    m_discoverTimer.stop();
    m_heartbeatTimer.stop();

    setState(State::Connecting);
    emit codeChanged();
    emit membersChanged();

    publishMessage(makeMessage(QStringLiteral("hello")));
    becomeJoined();
}

void TelepartySession::simulateReceiveMessage(const QString &senderUsername, const QJsonObject &payload) {
    onFluxMessage(senderUsername, payload);
}

void TelepartySession::simulatePeerLeft(const QString &username) {
    onPeerLeft(username);
}

void TelepartySession::simulateDisconnect() {
    onDisconnected();
}

void TelepartySession::simulateReconnect() {
    onJoinedRoom(m_room, m_memberId, SyncplayClient::kDefaultMaxChatLength);
}

// ============================================================================
// Helpers
// ============================================================================

QString TelepartySession::generateCode() {
    return QString::number(QRandomGenerator::global()->bounded(10000, 100000));
}

// Deterministic mapping of 5-digit code to a 24-character Syncplay room name (limit: 35 chars)
QString TelepartySession::roomNameForCode(const QString &code) {
    const QByteArray hash = QCryptographicHash::hash(
        QByteArrayLiteral("flux-teleparty-v1:") + code.trimmed().toUpper().toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("FLUX-TP-") + QString::fromLatin1(hash.left(16));
}

} // namespace Flux
