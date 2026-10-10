#pragma once

#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QSslSocket>
#include <QString>
#include <QTimer>

namespace Flux {

// Minimal MQTT 3.1.1 client over TLS (QoS 0 only, clean session).
// Just enough for Teleparty: connect, subscribe, publish, receive, last-will, keep-alive and
// automatic reconnect. Needs nothing beyond Qt Network.
class MqttClient : public QObject {
    Q_OBJECT

public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient() override;

    // Message the broker publishes for us if the connection dies without a clean disconnect.
    // Must be set before connectToBroker().
    void setWill(const QString &topic, const QByteArray &payload);

    // Optional MQTT 3.1.1 username / password credentials.
    // Must be set before connectToBroker().
    void setCredentials(const QString &username, const QString &password);

    // Connects over TLS. Topics added with subscribe() are (re)subscribed on every connect.
    void connectToBroker(const QString &host, quint16 port);
    void disconnectFromBroker();
    bool isConnected() const { return m_connected; }

    void subscribe(const QString &topic);
    void unsubscribe(const QString &topic);
    bool publish(const QString &topic, const QByteArray &payload);

signals:
    // Broker accepted us (also fires after every automatic reconnect)
    void connected();
    // An established connection dropped; the client keeps retrying on its own
    void disconnected();
    // The very first connection attempt (or authentication) failed; the client does not retry
    void connectionFailed(const QString &reason);
    // Broker confirmed a subscription (fires again after a reconnect)
    void subscribed(const QString &topic);
    void messageReceived(const QString &topic, const QByteArray &payload);

private:
    void openSocket();
    void sendConnect();
    void sendSubscribe(const QString &topic);
    bool sendPacket(quint8 header, const QByteArray &body);
    void onReadyRead();
    void handlePacket(quint8 header, const QByteArray &body);
    void handleLinkDown(const QString &reason);
    void scheduleReconnect();
    quint16 nextPacketId();

    QSslSocket *m_socket = nullptr;
    QTimer m_pingTimer;
    QTimer m_reconnectTimer;
    QTimer m_connectTimer;

    QString m_host;
    quint16 m_port = 0;
    QString m_username;
    QString m_password;
    QString m_clientId;
    QString m_willTopic;
    QByteArray m_willPayload;

    QSet<QString> m_topics;                    // wanted subscriptions
    QHash<quint16, QString> m_pendingSubs;     // packet id -> topic, until SUBACK
    QByteArray m_buffer;

    bool m_wantConnected = false;   // user asked to be connected
    bool m_connected = false;       // CONNACK received on the current socket
    bool m_everConnected = false;   // at least one CONNACK since connectToBroker()
    int m_retry = 0;
    quint16 m_packetId = 0;
};

} // namespace Flux
