#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVariantMap>
#include <functional>

#include "SyncplayClient.h"

namespace Flux {

// Teleparty: a "room" identified by a 5-digit code, shared through the public Syncplay server
// (syncplay.pl:8995) over StartTLS + TLS using FLUX|-prefixed chat relay messages.
//
// Wire format (compact JSON after "FLUX|" prefix, automatically chunked if > MaxChat 150 chars):
//   {"v":1, "t":"hello|presence|bye|event", "f":"<memberId>", "h"?:1, "i"?:<seq>, "e"?:<eventType>, "d"?:{...}}
//
//   hello     a new (or reconnected) member announces itself; everyone answers with "presence"
//   presence  "I'm here"; also sent every few seconds as a heartbeat ("h":1 indicates room host)
//   bye       a member left cleanly (abrupt disconnects are also signaled by Syncplay's Set.user.left)
//   event     application data (open, play, pause, seek, state, syncme, ts, rd) - used by the sync layer
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
    Q_PROPERTY(QString customRoomCode READ customRoomCode WRITE setCustomRoomCode NOTIFY customRoomCodeChanged)
    Q_PROPERTY(QString latchedRoomCode READ latchedRoomCode NOTIFY latchedRoomCodeChanged)

public:
    using TransportHook = std::function<bool(const QString &senderUsername, const QJsonObject &message)>;

    explicit TelepartySession(QObject *parent = nullptr);
    ~TelepartySession() override;

    QString state() const;
    bool active() const { return m_state == State::Joined || m_state == State::Reconnecting; }
    QString statusText() const;
    QString code() const { return m_code; }
    bool isHost() const { return m_isHost; }
    int memberCount() const;
    QString errorText() const { return m_errorText; }
    QString customRoomCode() const { return m_customRoomCode; }
    void setCustomRoomCode(const QString &code);
    QString latchedRoomCode() const { return m_latchedRoomCode; }
    Q_INVOKABLE void setLatchedRoomCode(const QString &code);
    Q_INVOKABLE void clearLatchedRoomCode();

    QString memberId() const { return m_memberId; }
    QString hostMemberId() const { return m_hostMemberId; }
    QStringList memberIds() const;
    int duplicateEventCount() const { return m_duplicateEventCount; }

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

    // Deterministic mapping from 5-digit room code to Syncplay room name
    static QString roomNameForCode(const QString &code);

    // Deterministic simulation hooks for automated multi-client network testing
    void setSimulatedTransport(TransportHook hook);
    void simulateJoinRoom(const QString &code, bool asHost, const QString &fixedMemberId = QString());
    void simulateReceiveMessage(const QString &senderUsername, const QJsonObject &payload);
    void simulatePeerLeft(const QString &username);
    void simulateDisconnect();
    void simulateReconnect();

signals:
    void stateChanged();
    void codeChanged();
    void membersChanged();
    void errorChanged();
    void customRoomCodeChanged();
    void latchedRoomCodeChanged();

    // The session was created / found and you are now in it
    void joinedSession();
    void leftSession();

    // A member joined or rejoined: share your current state with them
    void memberJoined(const QString &memberId);
    // A member left or disconnected
    void memberLeft(const QString &memberId);
    // Emitted when the authoritative host identity is established or migrates
    void authorityChanged(const QString &authorityMemberId, bool localIsAuthority);
    // An event sent by another member
    void eventReceived(const QString &type, const QVariantMap &data, const QString &from);

private:
    enum class State { Idle, Connecting, Joined, Reconnecting };

    void startSession();
    void becomeJoined();
    void handleHostCollision();
    void fail(const QString &message);
    void resetConnection();
    void setState(State s);
    void setError(const QString &message);

    void onJoinedRoom(const QString &room, const QString &username, int maxChatLength);
    void onDisconnected();
    void onFluxMessage(const QString &senderUsername, const QJsonObject &payload);
    void onPeerPresent(const QString &username);
    void onPeerLeft(const QString &username);
    void onDiscoveryTimeout();
    void onHeartbeat();

    QJsonObject makeMessage(const QString &type, const QString &eventType = QString(),
                            const QVariantMap &data = QVariantMap());
    bool publishMessage(const QJsonObject &message);

    static bool isRecognizedEventType(const QString &eventType);

    // Returns true when the member was not known before
    bool touchMember(const QString &id);
    void removeMember(const QString &id);
    void pruneMembers();
    void updateHostAuthority(const QString &candidateHostId);
    void electNewHostIfNeeded();
    bool recordAndCheckDuplicateSeq(const QString &from, qint64 seq);

    static QString generateCode();

    SyncplayClient *m_client = nullptr;
    TransportHook m_simulatedTransport;
    QTimer m_discoverTimer;    // waits for someone to answer our hello
    QTimer m_heartbeatTimer;

    State m_state = State::Idle;
    QString m_code;
    QString m_room;
    QString m_memberId;
    QString m_customRoomCode;
    QString m_latchedRoomCode;
    QString m_hostMemberId;
    bool m_isHost = false;
    bool m_helloSent = false;
    int m_attemptsLeft = 0;                 // free-code retries when hosting
    qint64 m_nextEventId = 0;
    QHash<QString, qint64> m_members;       // other members -> last time we heard from them (ms)
    QHash<QString, QList<qint64>> m_seenSeqOrder;
    QHash<QString, QSet<qint64>> m_seenSeqSet;
    int m_duplicateEventCount = 0;
    QString m_errorText;
};

} // namespace Flux
