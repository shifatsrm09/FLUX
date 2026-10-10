#include "MqttClient.h"
#include "../core/Logger.h"

#include <QSignalBlocker>
#include <QSslError>
#include <QUuid>
#include <algorithm>

namespace Flux {

namespace {

constexpr int kKeepAliveSec = 30;
constexpr int kMaxPacketBytes = 1024 * 1024;

// MQTT "remaining length": 7 bits per byte, high bit = more bytes follow
QByteArray encodeLength(int len) {
    QByteArray out;
    do {
        quint8 digit = static_cast<quint8>(len % 128);
        len /= 128;
        if (len > 0) digit |= 0x80;
        out.append(static_cast<char>(digit));
    } while (len > 0);
    return out;
}

QByteArray u16(quint16 v) {
    QByteArray b;
    b.append(static_cast<char>(v >> 8));
    b.append(static_cast<char>(v & 0xFF));
    return b;
}

// Length-prefixed string / binary blob
QByteArray encodeString(const QByteArray &s) {
    return u16(static_cast<quint16>(s.size())) + s;
}

} // namespace

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QSslSocket(this)) {
    m_pingTimer.setInterval(kKeepAliveSec * 500);   // half the keep-alive interval
    connect(&m_pingTimer, &QTimer::timeout, this, [this]() { sendPacket(0xC0, QByteArray()); });

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this]() {
        if (m_wantConnected) openSocket();
    });

    connect(m_socket, &QSslSocket::encrypted, this, &MqttClient::sendConnect);
    connect(m_socket, &QSslSocket::readyRead, this, &MqttClient::onReadyRead);
    connect(m_socket, &QSslSocket::disconnected, this, [this]() {
        handleLinkDown(QStringLiteral("Connection closed"));
    });
    connect(m_socket, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        handleLinkDown(m_socket->errorString());
    });
    connect(m_socket, &QSslSocket::sslErrors, this, [](const QList<QSslError> &errors) {
        for (const QSslError &e : errors) {
            FLUX_LOG_WARN("Teleparty", QString("TLS error: %1").arg(e.errorString()));
        }
    });
}

MqttClient::~MqttClient() {
    m_wantConnected = false;
    m_socket->abort();
}

void MqttClient::setWill(const QString &topic, const QByteArray &payload) {
    m_willTopic = topic;
    m_willPayload = payload;
}

void MqttClient::connectToBroker(const QString &host, quint16 port) {
    m_host = host;
    m_port = port;
    m_wantConnected = true;
    m_everConnected = false;
    m_retry = 0;
    // A fresh id per connection attempt so a stale broker session can never clash with us
    m_clientId = QStringLiteral("flux-") + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
    openSocket();
}

void MqttClient::openSocket() {
    m_buffer.clear();
    m_connected = false;
    m_pendingSubs.clear();
    m_pingTimer.stop();
    {
        // Silence the old connection completely: aborting a socket that is still closing
        // would otherwise fire a stale disconnected()/error that looks like a failure of
        // the connection we are about to open.
        const QSignalBlocker blocker(m_socket);
        m_socket->abort();
    }
    m_socket->connectToHostEncrypted(m_host, m_port);
}

void MqttClient::disconnectFromBroker() {
    m_wantConnected = false;
    m_reconnectTimer.stop();
    m_pingTimer.stop();

    if (m_connected) {
        sendPacket(0xE0, QByteArray());   // DISCONNECT (a clean one does not trigger the will)
        m_socket->disconnectFromHost();   // flushes pending writes first
    } else {
        m_socket->abort();
    }

    m_connected = false;
    m_topics.clear();
    m_pendingSubs.clear();
    m_buffer.clear();
}

void MqttClient::subscribe(const QString &topic) {
    m_topics.insert(topic);
    if (m_connected) sendSubscribe(topic);
}

void MqttClient::unsubscribe(const QString &topic) {
    m_topics.remove(topic);
    if (!m_connected) return;
    QByteArray body = u16(nextPacketId());
    body += encodeString(topic.toUtf8());
    sendPacket(0xA2, body);   // UNSUBSCRIBE
}

bool MqttClient::publish(const QString &topic, const QByteArray &payload) {
    if (!m_connected) return false;
    return sendPacket(0x30, encodeString(topic.toUtf8()) + payload);   // PUBLISH, QoS 0
}

void MqttClient::sendConnect() {
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    QByteArray body;
    body += encodeString(QByteArrayLiteral("MQTT"));
    body.append(static_cast<char>(4));   // protocol level 3.1.1

    quint8 flags = 0x02;                 // clean session
    if (!m_willTopic.isEmpty()) flags |= 0x04;   // will flag (QoS 0, not retained)
    body.append(static_cast<char>(flags));

    body += u16(kKeepAliveSec);
    body += encodeString(m_clientId.toUtf8());
    if (!m_willTopic.isEmpty()) {
        body += encodeString(m_willTopic.toUtf8());
        body += encodeString(m_willPayload);
    }
    sendPacket(0x10, body);
}

void MqttClient::sendSubscribe(const QString &topic) {
    const quint16 id = nextPacketId();
    QByteArray body = u16(id);
    body += encodeString(topic.toUtf8());
    body.append(static_cast<char>(0));   // requested QoS 0
    m_pendingSubs.insert(id, topic);
    sendPacket(0x82, body);              // SUBSCRIBE
}

bool MqttClient::sendPacket(quint8 header, const QByteArray &body) {
    if (m_socket->state() != QAbstractSocket::ConnectedState) return false;
    QByteArray packet;
    packet.append(static_cast<char>(header));
    packet += encodeLength(static_cast<int>(body.size()));
    packet += body;
    const bool ok = m_socket->write(packet) == packet.size();
    m_socket->flush();
    return ok;
}

quint16 MqttClient::nextPacketId() {
    ++m_packetId;
    if (m_packetId == 0) m_packetId = 1;
    return m_packetId;
}

void MqttClient::onReadyRead() {
    m_buffer.append(m_socket->readAll());

    while (m_buffer.size() >= 2) {
        const quint8 header = static_cast<quint8>(m_buffer.at(0));

        // Decode the remaining length (1 to 4 bytes)
        int multiplier = 1;
        int length = 0;
        int index = 1;
        bool complete = false;
        while (index < m_buffer.size() && index <= 4) {
            const quint8 b = static_cast<quint8>(m_buffer.at(index++));
            length += (b & 0x7F) * multiplier;
            multiplier *= 128;
            if ((b & 0x80) == 0) { complete = true; break; }
        }

        if (!complete) {
            if (index > 4) {   // malformed length: drop the connection and reconnect
                FLUX_LOG_WARN("Teleparty", "Malformed MQTT packet");
                m_buffer.clear();
                m_socket->abort();
                handleLinkDown(QStringLiteral("Protocol error"));
            }
            return;   // need more bytes
        }

        if (length > kMaxPacketBytes) {
            m_buffer.clear();
            m_socket->abort();
            handleLinkDown(QStringLiteral("Packet too large"));
            return;
        }

        if (m_buffer.size() < index + length) return;   // wait for the rest

        const QByteArray body = m_buffer.mid(index, length);
        m_buffer.remove(0, index + length);
        handlePacket(header, body);
    }
}

void MqttClient::handlePacket(quint8 header, const QByteArray &body) {
    const int type = header >> 4;

    switch (type) {
    case 2: {   // CONNACK
        const int code = body.size() >= 2 ? static_cast<quint8>(body.at(1)) : 255;
        if (code != 0) {
            handleLinkDown(QStringLiteral("Broker refused the connection (code %1)").arg(code));
            return;
        }
        m_connected = true;
        m_everConnected = true;
        m_retry = 0;
        m_pingTimer.start();
        for (const QString &t : std::as_const(m_topics)) sendSubscribe(t);
        emit connected();
        break;
    }

    case 9: {   // SUBACK
        if (body.size() < 2) return;
        const quint16 id = static_cast<quint16>((static_cast<quint8>(body.at(0)) << 8) | static_cast<quint8>(body.at(1)));
        if (m_pendingSubs.contains(id)) emit subscribed(m_pendingSubs.take(id));
        break;
    }

    case 3: {   // PUBLISH
        if (body.size() < 2) return;
        const int qos = (header >> 1) & 0x03;
        const int topicLen = (static_cast<quint8>(body.at(0)) << 8) | static_cast<quint8>(body.at(1));
        int pos = 2 + topicLen;
        if (pos > body.size()) return;
        const QString topic = QString::fromUtf8(body.mid(2, topicLen));
        if (qos > 0) {
            if (pos + 2 > body.size()) return;
            if (qos == 1) sendPacket(0x40, body.mid(pos, 2));   // PUBACK
            pos += 2;
        }
        emit messageReceived(topic, body.mid(pos));
        break;
    }

    default:    // PINGRESP, UNSUBACK, ... nothing to do
        break;
    }
}

void MqttClient::handleLinkDown(const QString &reason) {
    if (!m_wantConnected) return;

    const bool wasConnected = m_connected;
    m_connected = false;
    m_pingTimer.stop();

    if (!m_everConnected) {
        // Never got in: report it and stop, so the caller can show an error
        m_wantConnected = false;
        m_socket->abort();
        FLUX_LOG_WARN("Teleparty", QString("Could not connect: %1").arg(reason));
        emit connectionFailed(reason);
        return;
    }

    FLUX_LOG_WARN("Teleparty", QString("Connection lost: %1").arg(reason));
    if (wasConnected) emit disconnected();
    scheduleReconnect();
}

void MqttClient::scheduleReconnect() {
    if (m_reconnectTimer.isActive()) return;
    const int delay = std::min(10000, 1500 * (1 << std::min(m_retry, 3)));
    ++m_retry;
    m_reconnectTimer.start(delay);
}

} // namespace Flux
