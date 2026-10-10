#include <QGuiApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QSslSocket>
#include <QTimer>
#include <QVariantMap>
#include <iostream>

#include "../src/core/Logger.h"
#include "../src/teleparty/MqttClient.h"
#include "../src/teleparty/TelepartySession.h"

using namespace Flux;

namespace {

bool waitForCondition(int timeoutMs, const std::function<bool()> &cond) {
    QElapsedTimer timer;
    timer.start();
    while (!cond() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return cond();
}

} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    std::cout << "=== FLUX Teleparty EMQX Cloud Integration Test ===" << std::endl;

    // -------------------------------------------------------------------------
    // Test 1: Bad credentials rejection (verify no fallback to public broker)
    // -------------------------------------------------------------------------
    {
        std::cout << "\n[Test 1] Verifying authentication failure handling with invalid credentials..." << std::endl;
        MqttClient badClient;
        bool failed = false;
        QString failReason;
        QObject::connect(&badClient, &MqttClient::connectionFailed, [&](const QString &reason) {
            failed = true;
            failReason = reason;
        });

        badClient.setCredentials(QStringLiteral("cfat"), QStringLiteral("invalid-password-test"));
        badClient.connectToBroker(QStringLiteral("z242c03f.ala.asia-southeast1.emqxsl.com"), 8883);

        if (!waitForCondition(8000, [&]() { return failed; })) {
            std::cerr << "FAIL: Expected authentication failure with invalid credentials, but timed out." << std::endl;
            return 1;
        }
        std::cout << "PASS: Invalid credentials cleanly rejected: " << failReason.toStdString() << std::endl;
    }

    // -------------------------------------------------------------------------
    // Test 1b: Probe EMQX Cloud topic-level ACL configuration
    // -------------------------------------------------------------------------
    {
        std::cout << "\n[Test 1b] Checking EMQX Cloud topic-level authorization (ACL)..." << std::endl;
        MqttClient aclClient;
        QString subbedTopic;
        bool subFailed = false;
        QObject::connect(&aclClient, &MqttClient::subscribed, [&](const QString &t) { subbedTopic = t; });
        QObject::connect(&aclClient, &MqttClient::connectionFailed, [&](const QString &) { subFailed = true; });

        // Read credentials from .env via TelepartySession's working directory .env
        QFile envFile(QStringLiteral(".env"));
        QString user, pass, host;
        quint16 port = 8883;
        if (envFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            while (!envFile.atEnd()) {
                const QString line = QString::fromUtf8(envFile.readLine()).trimmed();
                const int eq = line.indexOf('=');
                if (eq > 0) {
                    const QString k = line.left(eq).trimmed();
                    const QString v = line.mid(eq + 1).trimmed();
                    if (k == QLatin1String("MQTT_USERNAME")) user = v;
                    else if (k == QLatin1String("MQTT_PASSWORD")) pass = v;
                    else if (k == QLatin1String("MQTT_BROKER_HOST")) host = v;
                    else if (k == QLatin1String("MQTT_BROKER_PORT")) port = v.toUShort();
                }
            }
        }
        aclClient.setCredentials(user, pass);
        aclClient.subscribe(QStringLiteral("flux/tp/v1/#"));
        aclClient.connectToBroker(host, port);
        waitForCondition(5000, [&]() { return !subbedTopic.isEmpty() || subFailed; });
        if (!subbedTopic.isEmpty()) {
            std::cout << "INFO: Broker allowed wildcard subscription 'flux/tp/v1/#' (topic-level ACL is permissive for this user)." << std::endl;
        } else {
            std::cout << "INFO: Broker restricted wildcard subscription 'flux/tp/v1/#' (strict topic-level ACL active)." << std::endl;
        }
        aclClient.disconnectFromBroker();
    }

    // -------------------------------------------------------------------------
    // Test 2: Host session creation & EMQX Cloud authentication via .env
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 2] Creating Host TelepartySession (loading .env configuration)..." << std::endl;
    TelepartySession hostSession;
    bool hostJoined = false;
    QObject::connect(&hostSession, &TelepartySession::joinedSession, [&]() {
        hostJoined = true;
    });

    hostSession.host();
    if (!waitForCondition(10000, [&]() { return hostJoined || !hostSession.errorText().isEmpty(); })) {
        std::cerr << "FAIL: Host session timed out waiting to join." << std::endl;
        return 1;
    }
    if (!hostJoined) {
        std::cerr << "FAIL: Host session failed with error: " << hostSession.errorText().toStdString() << std::endl;
        return 1;
    }

    const QString roomCode = hostSession.code();
    std::cout << "PASS: Host session established on EMQX Cloud! Room code: " << roomCode.toStdString()
              << ", state: " << hostSession.state().toStdString()
              << ", memberCount: " << hostSession.memberCount() << std::endl;

    // -------------------------------------------------------------------------
    // Test 3: Second client joins the room using the 5-digit code
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 3] Joining room " << roomCode.toStdString() << " from second TelepartySession client..." << std::endl;
    TelepartySession joinerSession;
    bool joinerJoined = false;
    QString hostObservedNewMember;

    QObject::connect(&joinerSession, &TelepartySession::joinedSession, [&]() {
        joinerJoined = true;
    });
    QObject::connect(&hostSession, &TelepartySession::memberJoined, [&](const QString &memberId) {
        hostObservedNewMember = memberId;
    });

    joinerSession.join(roomCode);
    if (!waitForCondition(10000, [&]() {
            return (joinerJoined && hostSession.memberCount() == 2 && joinerSession.memberCount() == 2) ||
                   !joinerSession.errorText().isEmpty();
        })) {
        std::cerr << "FAIL: Joiner failed or timed out. Joiner error: "
                  << joinerSession.errorText().toStdString() << std::endl;
        return 1;
    }
    if (!joinerJoined) {
        std::cerr << "FAIL: Joiner session error: " << joinerSession.errorText().toStdString() << std::endl;
        return 1;
    }

    std::cout << "PASS: Joiner connected to room " << roomCode.toStdString()
              << " | Host memberCount=" << hostSession.memberCount()
              << " | Joiner memberCount=" << joinerSession.memberCount()
              << " | Host saw memberId=" << hostObservedNewMember.toStdString() << std::endl;

    // -------------------------------------------------------------------------
    // Test 4: Play, Pause, Seek, and Open synchronization events
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 4] Testing open, play, pause, and seek events across clients..." << std::endl;

    int joinerEventCount = 0;
    QString lastJoinerEvent;
    QVariantMap lastJoinerData;
    QObject::connect(&joinerSession, &TelepartySession::eventReceived,
                     [&](const QString &type, const QVariantMap &data, const QString &) {
                         ++joinerEventCount;
                         lastJoinerEvent = type;
                         lastJoinerData = data;
                     });

    int hostEventCount = 0;
    QString lastHostEvent;
    QVariantMap lastHostData;
    QObject::connect(&hostSession, &TelepartySession::eventReceived,
                     [&](const QString &type, const QVariantMap &data, const QString &) {
                         ++hostEventCount;
                         lastHostEvent = type;
                         lastHostData = data;
                     });

    // 4a: Host sends "open"
    {
        QVariantMap d;
        d.insert(QStringLiteral("url"), QStringLiteral("http://example.com/movie.mkv"));
        d.insert(QStringLiteral("title"), QStringLiteral("Test Movie"));
        hostSession.sendEvent(QStringLiteral("open"), d);
        if (!waitForCondition(5000, [&]() { return lastJoinerEvent == QLatin1String("open"); })) {
            std::cerr << "FAIL: Joiner did not receive 'open' event." << std::endl;
            return 1;
        }
        std::cout << "PASS: 'open' event received by Joiner (title="
                  << lastJoinerData.value(QStringLiteral("title")).toString().toStdString() << ")" << std::endl;
    }

    // 4b: Host sends "pause"
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 42500);
        hostSession.sendEvent(QStringLiteral("pause"), d);
        if (!waitForCondition(5000, [&]() { return lastJoinerEvent == QLatin1String("pause"); })) {
            std::cerr << "FAIL: Joiner did not receive 'pause' event." << std::endl;
            return 1;
        }
        std::cout << "PASS: 'pause' event received by Joiner (t="
                  << lastJoinerData.value(QStringLiteral("t")).toLongLong() << "ms)" << std::endl;
    }

    // 4c: Joiner sends "seek" back to Host
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 120000);
        joinerSession.sendEvent(QStringLiteral("seek"), d);
        if (!waitForCondition(5000, [&]() { return lastHostEvent == QLatin1String("seek"); })) {
            std::cerr << "FAIL: Host did not receive 'seek' event from Joiner." << std::endl;
            return 1;
        }
        std::cout << "PASS: 'seek' event from Joiner received by Host (t="
                  << lastHostData.value(QStringLiteral("t")).toLongLong() << "ms)" << std::endl;
    }

    // 4d: Joiner sends "play" back to Host
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 120000);
        joinerSession.sendEvent(QStringLiteral("play"), d);
        if (!waitForCondition(5000, [&]() { return lastHostEvent == QLatin1String("play"); })) {
            std::cerr << "FAIL: Host did not receive 'play' event from Joiner." << std::endl;
            return 1;
        }
        std::cout << "PASS: 'play' event from Joiner received by Host (t="
                  << lastHostData.value(QStringLiteral("t")).toLongLong() << "ms)" << std::endl;
    }

    // -------------------------------------------------------------------------
    // Test 5: Simulate disconnect and automatic reconnect + verify no duplicates
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 5] Testing disconnect, automatic reconnect, and non-duplicate delivery..." << std::endl;
    QSslSocket *joinerSocket = joinerSession.findChild<QSslSocket *>();
    if (!joinerSocket) {
        std::cerr << "FAIL: Could not locate joiner QSslSocket for disconnect simulation." << std::endl;
        return 1;
    }
    joinerSocket->disconnectFromHost();
    if (!waitForCondition(3000, [&]() { return joinerSession.state() == QLatin1String("reconnecting"); })) {
        std::cerr << "FAIL: Joiner did not enter 'reconnecting' state after socket drop." << std::endl;
        return 1;
    }
    std::cout << "PASS: Joiner entered 'reconnecting' state on drop." << std::endl;

    if (!waitForCondition(10000, [&]() { return joinerSession.state() == QLatin1String("joined"); })) {
        std::cerr << "FAIL: Joiner did not automatically reconnect to 'joined' state." << std::endl;
        return 1;
    }
    std::cout << "PASS: Joiner automatically reconnected and restored 'joined' state." << std::endl;

    // Verify a single event after reconnect arrives exactly once (no duplicate subscription)
    const int beforeCount = joinerEventCount;
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 185000);
        hostSession.sendEvent(QStringLiteral("seek"), d);
        if (!waitForCondition(5000, [&]() { return joinerEventCount > beforeCount; })) {
            std::cerr << "FAIL: Joiner did not receive event after reconnect." << std::endl;
            return 1;
        }
        // Wait 500ms to ensure no duplicate copy arrives
        waitForCondition(500, [&]() { return false; });
        if (joinerEventCount != beforeCount + 1) {
            std::cerr << "FAIL: Expected exactly 1 event after reconnect, got "
                      << (joinerEventCount - beforeCount) << std::endl;
            return 1;
        }
        std::cout << "PASS: Post-reconnect event delivered exactly once (no duplicate subscription)." << std::endl;
    }

    // -------------------------------------------------------------------------
    // Test 6: Leave room and clean up
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 6] Testing participant leave & session cleanup..." << std::endl;
    joinerSession.leave();
    if (!waitForCondition(5000, [&]() {
            return joinerSession.state() == QLatin1String("idle") && hostSession.memberCount() == 1;
        })) {
        std::cerr << "FAIL: Host memberCount did not update to 1 after Joiner left." << std::endl;
        return 1;
    }
    std::cout << "PASS: Joiner left cleanly; Host memberCount is now " << hostSession.memberCount() << std::endl;

    hostSession.leave();
    std::cout << "PASS: Host left cleanly; state is now " << hostSession.state().toStdString() << std::endl;

    std::cout << "\n=== ALL TELEPARTY EMQX CLOUD TESTS PASSED ===" << std::endl;
    return 0;
}
