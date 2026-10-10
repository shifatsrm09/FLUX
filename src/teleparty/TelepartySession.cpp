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
#include <QUuid>

namespace Flux {

namespace {

// Free public EMQX broker (TLS). Nothing of ours runs anywhere.
const QString kBroker = QStringLiteral("broker.emqx.io");
constexpr quint16 kBrokerPort = 8883;

constexpr int kProtocolVersion = 1;
constexpr int kMaxMessageBytes = 16 * 1024;
constexpr int kHeartbeatMs = 10000;          // "presence" every 10 s
constexpr qint64 kMemberTimeoutMs = 32000;   // a member silent this long is dropped
constexpr int kHostDiscoverMs = 1800;        // hosting: wait this long for a clash with an existing room
constexpr int kJoinDiscoverMs = 2500;        // joining: wait this long for someone to answer
constexpr int kMaxHostAttempts = 5;

qint64 nowMs() {
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

TelepartySession::TelepartySession(QObject *parent)
    : QObject(parent)
    , m_mqtt(new MqttClient(this)) {
    m_discoverTimer.setSingleShot(true);
    connect(&m_discoverTimer, &QTimer::timeout, this, &TelepartySession::onDiscoveryTimeout);

    m_heartbeatTimer.setInterval(kHeartbeatMs);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &TelepartySession::onHeartbeat);

    connect(m_mqtt, &MqttClient::connected, this, &TelepartySession::onMqttConnected);
    connect(m_mqtt, &MqttClient::disconnected, this, &TelepartySession::onMqttDisconnected);
    connect(m_mqtt, &MqttClient::subscribed, this, &TelepartySession::onSubscribed);
    connect(m_mqtt, &MqttClient::messageReceived, this, &TelepartySession::onMessage);
    connect(m_mqtt, &MqttClient::connectionFailed, this, [this](const QString &) {
        if (m_state == State::Connecting) {
            fail(QStringLiteral("Couldn't reach the Teleparty service. Check your internet connection."));
        }
    });
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

void TelepartySession::host() {
    if (m_state != State::Idle) return;
    setError(QString());
    m_isHost = true;
    m_attemptsLeft = kMaxHostAttempts;
    m_code = generateCode();
    startSession();
}

void TelepartySession::join(const QString &code) {
    if (m_state != State::Idle) return;

    const QString trimmed = code.trimmed();
    static const QRegularExpression fiveDigits(QStringLiteral("^\\d{5}$"));
    if (!fiveDigits.match(trimmed).hasMatch()) {
        setError(QStringLiteral("Enter the 5-digit code."));
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
    if (m_state == State::Joined) publishMessage(makeMessage(QStringLiteral("bye")));

    resetConnection();
    m_code.clear();
    m_isHost = false;
    setState(State::Idle);
    emit codeChanged();
    emit membersChanged();
    if (wasActive) emit leftSession();
    FLUX_LOG_INFO("Teleparty", "Left the session");
}

void TelepartySession::startSession() {
    m_memberId = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-').left(10);
    m_topic = roomTopic(m_code);
    m_members.clear();
    m_helloSent = false;
    m_discoverTimer.stop();
    m_heartbeatTimer.stop();

    setState(State::Connecting);
    emit codeChanged();
    emit membersChanged();

    FLUX_LOG_INFO("Teleparty", QString("%1 session %2").arg(m_isHost ? "Hosting" : "Joining", m_code));

    // If our connection dies, the broker tells everyone else we are gone
    m_mqtt->setWill(m_topic, makeMessage(QStringLiteral("bye")));
    m_mqtt->subscribe(m_topic);
    m_mqtt->connectToBroker(kBroker, kBrokerPort);
}

void TelepartySession::becomeJoined() {
    m_discoverTimer.stop();
    setState(State::Joined);
    m_heartbeatTimer.start();
    emit membersChanged();
    emit joinedSession();
    FLUX_LOG_INFO("Teleparty", QString("Joined session %1 (%2 members)").arg(m_code).arg(memberCount()));
}

void TelepartySession::fail(const QString &message) {
    FLUX_LOG_WARN("Teleparty", message);
    resetConnection();
    m_code.clear();
    m_isHost = false;
    setState(State::Idle);
    emit codeChanged();
    emit membersChanged();
    setError(message);
}

void TelepartySession::resetConnection() {
    m_discoverTimer.stop();
    m_heartbeatTimer.stop();
    m_mqtt->disconnectFromBroker();
    m_members.clear();
    m_helloSent = false;
    m_topic.clear();
}

// ============================================================================
// Connection events
// ============================================================================

void TelepartySession::onMqttConnected() {
    FLUX_LOG_DEBUG("Teleparty", "Connected to broker");
    // The room topic was queued with subscribe(); hello goes out once the broker confirms it
}

void TelepartySession::onMqttDisconnected() {
    if (m_state == State::Joined) setState(State::Reconnecting);
}

void TelepartySession::onSubscribed(const QString &topic) {
    if (topic != m_topic) return;

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
        publishMessage(makeMessage(QStringLiteral("hello")));   // re-announce, learn who is still here
    }
}

void TelepartySession::onDiscoveryTimeout() {
    if (m_state != State::Connecting) return;

    if (m_isHost) {
        becomeJoined();   // nobody else uses this code: the room is ours
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

void TelepartySession::onMessage(const QString &topic, const QByteArray &payload) {
    if (m_state == State::Idle || topic != m_topic || payload.size() > kMaxMessageBytes) return;

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return;

    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("v")).toInt() != kProtocolVersion) return;

    const QString from = o.value(QStringLiteral("from")).toString();
    const QString type = o.value(QStringLiteral("t")).toString();
    if (from.isEmpty() || from.size() > 32 || from == m_memberId) return;   // ignore our own echo

    if (type == QLatin1String("bye")) {
        removeMember(from);
        return;
    }

    touchMember(from);

    if (type == QLatin1String("hello")) {
        // Let the newcomer know we exist, and let the sync layer share the current state
        publishMessage(makeMessage(QStringLiteral("presence")));
        if (m_state == State::Joined) emit memberJoined(from);

    } else if (type == QLatin1String("presence")) {
        if (m_state == State::Connecting && m_discoverTimer.isActive()) {
            if (m_isHost) {
                // Someone already uses this code: pick another one
                m_discoverTimer.stop();
                if (--m_attemptsLeft <= 0) {
                    fail(QStringLiteral("Couldn't create a session. Please try again."));
                    return;
                }
                m_mqtt->disconnectFromBroker();
                m_code = generateCode();
                startSession();
            } else {
                becomeJoined();   // found the session
            }
        }

    } else if (type == QLatin1String("event")) {
        if (m_state == State::Joined || m_state == State::Reconnecting) {
            emit eventReceived(o.value(QStringLiteral("e")).toString(),
                               o.value(QStringLiteral("d")).toObject().toVariantMap(),
                               from);
        }
    }
}

bool TelepartySession::sendEvent(const QString &type, const QVariantMap &data) {
    if (m_state != State::Joined || type.isEmpty()) return false;
    return m_mqtt->publish(m_topic, makeMessage(QStringLiteral("event"), type, data));
}

QByteArray TelepartySession::makeMessage(const QString &type, const QString &eventType,
                                         const QVariantMap &data) const {
    QJsonObject o;
    o.insert(QStringLiteral("v"), kProtocolVersion);
    o.insert(QStringLiteral("t"), type);
    o.insert(QStringLiteral("from"), m_memberId);
    if (!eventType.isEmpty()) {
        o.insert(QStringLiteral("e"), eventType);
        o.insert(QStringLiteral("d"), QJsonObject::fromVariantMap(data));
    }
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

void TelepartySession::publishMessage(const QByteArray &message) {
    if (!m_topic.isEmpty()) m_mqtt->publish(m_topic, message);
}

// ============================================================================
// Members
// ============================================================================

bool TelepartySession::touchMember(const QString &id) {
    const bool isNew = !m_members.contains(id);
    m_members.insert(id, nowMs());
    if (isNew) emit membersChanged();
    return isNew;
}

void TelepartySession::removeMember(const QString &id) {
    if (m_members.remove(id)) emit membersChanged();
}

void TelepartySession::pruneMembers() {
    const qint64 cutoff = nowMs() - kMemberTimeoutMs;
    bool changed = false;
    for (auto it = m_members.begin(); it != m_members.end();) {
        if (it.value() < cutoff) {
            it = m_members.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) emit membersChanged();
}

// ============================================================================
// Helpers
// ============================================================================

QString TelepartySession::generateCode() {
    return QString::number(QRandomGenerator::global()->bounded(10000, 100000));
}

// The room name on the public broker is a hash of the code, not the code itself
QString TelepartySession::roomTopic(const QString &code) {
    const QByteArray hash = QCryptographicHash::hash(
        QByteArrayLiteral("flux-teleparty-v1:") + code.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("flux/tp/v1/") + QString::fromLatin1(hash.left(24));
}

} // namespace Flux
