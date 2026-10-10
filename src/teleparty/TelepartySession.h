#pragma once

#include <QHash>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include "MqttClient.h"

namespace Flux {

// Teleparty: a "room" identified by a 5-digit code, shared through the configured EMQX Cloud MQTT broker.
// Every member publishes to, and listens on, the same room topic over TLS.
//
// Wire format (JSON on the room topic):
//   {"v":1, "t":"hello|presence|bye|event", "from":"<memberId>", "e":"<eventType>", "d":{...}}
//
//   hello     a new (or reconnected) member announces itself; everyone answers with "presence"
//   presence  "I'm here"; also sent every few seconds as a heartbeat
//   bye       a member left (also published by the broker if a member's connection dies)
//   event     application data (open video, play, pause, seek...) - used by the sync layer
//
// State is exposed to QML as fluxTeleparty.
class TelepartySession : public QObject {
    Q_OBJECT

    // "idle" | "connecting" | "joined" | "reconnecting"
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    // True while in a session (joined or briefly reconnecting)
    Q_PROPERTY(bool active READ active NOTIFY stateChanged)
    // Text for the title bar: "Teleparty", "Teleparty: Joined", ...
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString code READ code NOTIFY codeChanged)
    Q_PROPERTY(bool isHost READ isHost NOTIFY codeChanged)
    // Everyone in the session including you (0 when not in a session)
    Q_PROPERTY(int memberCount READ memberCount NOTIFY membersChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorChanged)

public:
    explicit TelepartySession(QObject *parent = nullptr);
    ~TelepartySession() override;

    QString state() const;
    bool active() const { return m_state == State::Joined || m_state == State::Reconnecting; }
    QString statusText() const;
    QString code() const { return m_code; }
    bool isHost() const { return m_isHost; }
    int memberCount() const;
    QString errorText() const { return m_errorText; }

    // Start a new session: generates a free 5-digit code
    Q_INVOKABLE void host();
    // Join an existing session by its 5-digit code
    Q_INVOKABLE void join(const QString &code);
    Q_INVOKABLE void leave();
    Q_INVOKABLE void clearError();
    // Copy the 5-digit code to the clipboard
    Q_INVOKABLE void copyCode() const;

    // Send an application event to every other member (only while joined)
    Q_INVOKABLE bool sendEvent(const QString &type, const QVariantMap &data = QVariantMap());

signals:
    void stateChanged();
    void codeChanged();
    void membersChanged();
    void errorChanged();

    // The session was created / found and you are now in it
    void joinedSession();
    void leftSession();

    // A member joined or rejoined: share your current state with them
    void memberJoined(const QString &memberId);
    // An event sent by another member
    void eventReceived(const QString &type, const QVariantMap &data, const QString &from);

private:
    enum class State { Idle, Connecting, Joined, Reconnecting };

    void startSession();
    void becomeJoined();
    void fail(const QString &message);
    void resetConnection();
    void setState(State s);
    void setError(const QString &message);

    void onMqttConnected();
    void onMqttDisconnected();
    void onSubscribed(const QString &topic);
    void onMessage(const QString &topic, const QByteArray &payload);
    void onDiscoveryTimeout();
    void onHeartbeat();

    QByteArray makeMessage(const QString &type, const QString &eventType = QString(),
                           const QVariantMap &data = QVariantMap()) const;
    void publishMessage(const QByteArray &message);

    // Returns true when the member was not known before
    bool touchMember(const QString &id);
    void removeMember(const QString &id);
    void pruneMembers();

    static QString generateCode();
    static QString roomTopic(const QString &code);

    MqttClient *m_mqtt = nullptr;
    QTimer m_discoverTimer;    // waits for someone to answer our hello
    QTimer m_heartbeatTimer;

    State m_state = State::Idle;
    QString m_code;
    QString m_topic;
    QString m_memberId;
    bool m_isHost = false;
    bool m_helloSent = false;
    int m_attemptsLeft = 0;           // free-code retries when hosting
    QHash<QString, qint64> m_members; // other members -> last time we heard from them (ms)
    QString m_errorText;
};

} // namespace Flux
