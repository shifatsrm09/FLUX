#include "TelepartySession.h"
#include "../core/Logger.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>

namespace Flux {

namespace {

constexpr quint16 kDefaultBrokerPort = 8883;
constexpr int kProtocolVersion = 1;
constexpr int kMaxMessageBytes = 16 * 1024;
constexpr int kHeartbeatMs = 10000;          // "presence" every 10 s
constexpr qint64 kMemberTimeoutMs = 32000;   // a member silent this long is dropped
constexpr int kHostDiscoverMs = 1800;        // hosting: wait this long for a clash with an existing room
constexpr int kJoinDiscoverMs = 2500;        // joining: wait this long for someone to answer
constexpr int kMaxHostAttempts = 5;

struct BrokerConfig {
    QString host;
    quint16 port = kDefaultBrokerPort;
    QString protocol = QStringLiteral("mqtts");
    QString username;
    QString password;
    bool tlsEnabled = true;
    bool tlsVerify = true;
    QString sourcePath;
    QString error;
    bool isValid() const { return error.isEmpty(); }
};

QHash<QString, QString> parseDotEnvFile(const QString &filePath) {
    QHash<QString, QString> map;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return map;
    }

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        if (line.startsWith(QLatin1String("export "))) {
            line = line.mid(7).trimmed();
        }
        const int eqPos = line.indexOf('=');
        if (eqPos <= 0) {
            continue;
        }
        const QString key = line.left(eqPos).trimmed();
        QString val = line.mid(eqPos + 1).trimmed();
        if (val.size() >= 2 &&
            ((val.startsWith('"') && val.endsWith('"')) ||
             (val.startsWith('\'') && val.endsWith('\'')))) {
            val = val.mid(1, val.size() - 2);
        }
        if (!key.isEmpty()) {
            map.insert(key, val);
        }
    }
    return map;
}

bool parseBoolFlag(const QString &raw, bool defaultVal, bool *okOut = nullptr) {
    const QString s = raw.trimmed().toLower();
    if (s.isEmpty()) {
        if (okOut) *okOut = true;
        return defaultVal;
    }
    if (s == QLatin1String("true") || s == QLatin1String("1") || s == QLatin1String("yes")) {
        if (okOut) *okOut = true;
        return true;
    }
    if (s == QLatin1String("false") || s == QLatin1String("0") || s == QLatin1String("no")) {
        if (okOut) *okOut = true;
        return false;
    }
    if (okOut) *okOut = false;
    return defaultVal;
}

BrokerConfig loadBrokerConfig() {
    BrokerConfig cfg;

    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir::current().filePath(QStringLiteral(".env")),
        QDir(appDir).filePath(QStringLiteral(".env")),
        QDir(appDir).filePath(QStringLiteral("../.env")),
        QDir(appDir).filePath(QStringLiteral("../../.env")),
        QDir(appDir).filePath(QStringLiteral("../../../.env")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath(QStringLiteral(".env")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath(QStringLiteral(".env"))
    };

    QHash<QString, QString> fileVars;
    for (const QString &candidate : candidates) {
        const QFileInfo fi(candidate);
        if (fi.exists() && fi.isFile()) {
            cfg.sourcePath = fi.canonicalFilePath();
            fileVars = parseDotEnvFile(cfg.sourcePath);
            break;
        }
    }

    auto getBuildDefault = [](const char *name) -> QString {
#ifdef FLUX_ENV_MQTT_BROKER_HOST
        if (qstrcmp(name, "MQTT_BROKER_HOST") == 0) return QStringLiteral(FLUX_ENV_MQTT_BROKER_HOST);
#endif
#ifdef FLUX_ENV_MQTT_BROKER_PORT
        if (qstrcmp(name, "MQTT_BROKER_PORT") == 0) return QStringLiteral(FLUX_ENV_MQTT_BROKER_PORT);
#endif
#ifdef FLUX_ENV_MQTT_PROTOCOL
        if (qstrcmp(name, "MQTT_PROTOCOL") == 0) return QStringLiteral(FLUX_ENV_MQTT_PROTOCOL);
#endif
#ifdef FLUX_ENV_MQTT_USERNAME
        if (qstrcmp(name, "MQTT_USERNAME") == 0) return QStringLiteral(FLUX_ENV_MQTT_USERNAME);
#endif
#ifdef FLUX_ENV_MQTT_PASSWORD
        if (qstrcmp(name, "MQTT_PASSWORD") == 0) return QStringLiteral(FLUX_ENV_MQTT_PASSWORD);
#endif
#ifdef FLUX_ENV_MQTT_TLS_ENABLED
        if (qstrcmp(name, "MQTT_TLS_ENABLED") == 0) return QStringLiteral(FLUX_ENV_MQTT_TLS_ENABLED);
#endif
#ifdef FLUX_ENV_MQTT_TLS_VERIFY
        if (qstrcmp(name, "MQTT_TLS_VERIFY") == 0) return QStringLiteral(FLUX_ENV_MQTT_TLS_VERIFY);
#endif
        Q_UNUSED(name);
        return QString();
    };

    auto getVar = [&fileVars, &getBuildDefault](const char *name) -> QString {
        const QString envVal = qEnvironmentVariable(name).trimmed();
        if (!envVal.isEmpty()) {
            return envVal;
        }
        const QString fileVal = fileVars.value(QString::fromLatin1(name)).trimmed();
        if (!fileVal.isEmpty()) {
            return fileVal;
        }
        return getBuildDefault(name).trimmed();
    };

    cfg.host = getVar("MQTT_BROKER_HOST");
    const QString portStr = getVar("MQTT_BROKER_PORT");
    if (!portStr.isEmpty()) {
        bool portOk = false;
        const uint p = portStr.toUInt(&portOk);
        if (!portOk || p == 0 || p > 65535) {
            cfg.error = QStringLiteral("Invalid MQTT_BROKER_PORT in Teleparty configuration.");
            return cfg;
        }
        cfg.port = static_cast<quint16>(p);
    }

    const QString protoStr = getVar("MQTT_PROTOCOL").toLower();
    if (!protoStr.isEmpty()) {
        cfg.protocol = protoStr;
    }
    if (cfg.protocol != QLatin1String("mqtts") &&
        cfg.protocol != QLatin1String("tls") &&
        cfg.protocol != QLatin1String("ssl")) {
        cfg.error = QStringLiteral("Unsupported MQTT_PROTOCOL '%1' (expected 'mqtts').").arg(cfg.protocol);
        return cfg;
    }

    bool tlsEnabledOk = true;
    cfg.tlsEnabled = parseBoolFlag(getVar("MQTT_TLS_ENABLED"), true, &tlsEnabledOk);
    if (!tlsEnabledOk || !cfg.tlsEnabled) {
        cfg.error = QStringLiteral("Teleparty requires MQTT_TLS_ENABLED=true.");
        return cfg;
    }

    bool tlsVerifyOk = true;
    cfg.tlsVerify = parseBoolFlag(getVar("MQTT_TLS_VERIFY"), true, &tlsVerifyOk);
    if (!tlsVerifyOk || !cfg.tlsVerify) {
        cfg.error = QStringLiteral("Teleparty requires MQTT_TLS_VERIFY=true.");
        return cfg;
    }

    cfg.username = getVar("MQTT_USERNAME");
    cfg.password = getVar("MQTT_PASSWORD");

    if (cfg.host.isEmpty() || cfg.username.isEmpty() || cfg.password.isEmpty()) {
        cfg.error = QStringLiteral("Teleparty is not configured. Missing MQTT broker credentials in .env.");
        return cfg;
    }

    return cfg;
}

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
    connect(m_mqtt, &MqttClient::connectionFailed, this, [this](const QString &reason) {
        if (m_state == State::Connecting || m_state == State::Reconnecting) {
            if (reason.contains(QLatin1String("Authentication failed"), Qt::CaseInsensitive) ||
                reason.contains(QLatin1String("not authorized"), Qt::CaseInsensitive)) {
                fail(QStringLiteral("Teleparty authentication failed. Check your MQTT credentials."));
            } else if (reason.contains(QLatin1String("timed out"), Qt::CaseInsensitive)) {
                fail(QStringLiteral("Connection to Teleparty broker timed out."));
            } else {
                fail(QStringLiteral("Couldn't reach the Teleparty service. Check your internet connection."));
            }
        }
    });

    const BrokerConfig cfg = loadBrokerConfig();
    if (cfg.isValid()) {
        FLUX_LOG_INFO("Teleparty",
                      QString("Configured MQTT broker %1:%2 (protocol=%3, tls=true, verify=true, user=%4, source=%5)")
                          .arg(cfg.host)
                          .arg(cfg.port)
                          .arg(cfg.protocol, cfg.username,
                               cfg.sourcePath.isEmpty() ? QStringLiteral("environment") : cfg.sourcePath));
    } else {
        FLUX_LOG_WARN("Teleparty", cfg.error);
    }
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
    const BrokerConfig cfg = loadBrokerConfig();
    if (!cfg.isValid()) {
        fail(cfg.error);
        return;
    }

    m_memberId = QUuid::createUuid().toString(QUuid::WithoutBraces).remove('-').left(10);
    m_topic = roomTopic(m_code);
    m_members.clear();
    m_helloSent = false;
    m_discoverTimer.stop();
    m_heartbeatTimer.stop();

    setState(State::Connecting);
    emit codeChanged();
    emit membersChanged();

    FLUX_LOG_INFO("Teleparty",
                  QString("%1 session %2 via %3:%4 (%5)")
                      .arg(m_isHost ? "Hosting" : "Joining", m_code, cfg.host)
                      .arg(cfg.port)
                      .arg(cfg.protocol));

    m_mqtt->disconnectFromBroker();
    m_mqtt->setCredentials(cfg.username, cfg.password);
    // If our connection dies, the broker tells everyone else we are gone
    m_mqtt->setWill(m_topic, makeMessage(QStringLiteral("bye")));
    m_mqtt->subscribe(m_topic);
    m_mqtt->connectToBroker(cfg.host, cfg.port);
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

// The room name on the broker is a hash of the code, not the code itself
QString TelepartySession::roomTopic(const QString &code) {
    const QByteArray hash = QCryptographicHash::hash(
        QByteArrayLiteral("flux-teleparty-v1:") + code.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStringLiteral("flux/tp/v1/") + QString::fromLatin1(hash.left(24));
}

} // namespace Flux
