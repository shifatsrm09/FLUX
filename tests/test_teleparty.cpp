#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSslSocket>
#include <QVariantMap>
#include <functional>
#include <iostream>

#include "../src/core/Logger.h"
#include "../src/teleparty/SyncplayClient.h"
#include "../src/teleparty/TelepartySession.h"
#include "../src/teleparty/TelepartySync.h"

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

    std::cout << "=== FLUX Teleparty Syncplay Integration & Security Test Suite ===" << std::endl;

    // -------------------------------------------------------------------------
    // Test 1: Local Catalog Movie ID Encoding, Resolution & Security Validation
    // -------------------------------------------------------------------------
    {
        std::cout << "\n[Test 1] Verifying catalog movie ID encoding and security validation..." << std::endl;
        TelepartySession dummySession;
        TelepartySync sync(&dummySession, nullptr);

        // 1a: Built-in catalog movie ("ant-man")
        const QString antManUrl = QStringLiteral(
            "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/"
            "Ant-Man%20and%20the%20Wasp-Quantumania%20%282023%29%201080p%20DSNP/"
            "Ant-Man%20and%20the%20Wasp%20Quantumania%20%282023%29%201080p%20DSNP-WEB%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-PSA.mkv");
        const QString encodedId = sync.encodeCatalogMovieId(antManUrl);
        if (encodedId != QLatin1String("ant-man")) {
            std::cerr << "FAIL: Expected compact ID 'ant-man', got '" << encodedId.toStdString() << "'" << std::endl;
            return 1;
        }

        QString resolvedUrl, resolvedTitle;
        QVariantMap validPayload;
        validPayload.insert(QStringLiteral("id"), QStringLiteral("ant-man"));
        if (!sync.resolveCatalogMovie(validPayload, resolvedUrl, resolvedTitle) || resolvedUrl != antManUrl) {
            std::cerr << "FAIL: Could not resolve built-in movie ID 'ant-man'." << std::endl;
            return 1;
        }
        std::cout << "PASS: Compact movie ID 'ant-man' -> resolved to catalog URL ("
                  << resolvedTitle.toStdString() << ")" << std::endl;

        // 1b: Dynamic BDIX catalog path under a known MediaRoot ("english_movies_1080p")
        const QString dynamicUrl = QStringLiteral(
            "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282024%29%201080p/"
            "Dune%20Part%20Two%20%282024%29%201080p/Dune.Part.Two.2024.1080p.BluRay.x265.mkv");
        const QString dynId = sync.encodeCatalogMovieId(dynamicUrl);
        if (!dynId.startsWith(QLatin1String("english_movies_1080p:"))) {
            std::cerr << "FAIL: Expected 'english_movies_1080p:<relPath>', got '" << dynId.toStdString() << "'" << std::endl;
            return 1;
        }
        QVariantMap dynPayload;
        dynPayload.insert(QStringLiteral("id"), dynId);
        dynPayload.insert(QStringLiteral("n"), QStringLiteral("Dune: Part Two"));
        if (!sync.resolveCatalogMovie(dynPayload, resolvedUrl, resolvedTitle) || resolvedUrl != dynamicUrl) {
            std::cerr << "FAIL: Dynamic catalog movie ID did not round-trip to original BDIX URL." << std::endl;
            return 1;
        }
        std::cout << "PASS: Dynamic catalog movie ID round-tripped cleanly (" << dynId.size() << " chars)" << std::endl;

        // 1c: Reject arbitrary local file paths, unknown movie IDs, path traversal, and external URLs
        const QList<QVariantMap> maliciousPayloads = {
            {{QStringLiteral("id"), QStringLiteral("nonexistent-movie-id")}},
            {{QStringLiteral("id"), QStringLiteral("unknown_root:Movie.mkv")}},
            {{QStringLiteral("id"), QStringLiteral("english_movies_1080p:../../Windows/System32/cmd.exe")}},
            {{QStringLiteral("id"), QStringLiteral("english_movies_1080p:C:\\Users\\shifa\\secret.mkv")}},
            {{QStringLiteral("url"), QStringLiteral("file:///C:/Windows/System32/calc.exe")}},
            {{QStringLiteral("url"), QStringLiteral("C:\\Movies\\local.mkv")}},
            {{QStringLiteral("url"), QStringLiteral("http://evil.example.com/malware.mkv")}},
            {{QStringLiteral("url"), QStringLiteral("http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/../../secret.mkv")}}
        };
        for (const QVariantMap &bad : maliciousPayloads) {
            QString u, t;
            if (sync.resolveCatalogMovie(bad, u, t)) {
                std::cerr << "FAIL: Security check accepted untrusted payload: " << u.toStdString() << std::endl;
                return 1;
            }
        }
        std::cout << "PASS: All untrusted movie IDs, local paths, traversals, and external URLs rejected." << std::endl;
    }

    // -------------------------------------------------------------------------
    // Test 2: Deterministic Unit Test for Chunk Reassembly, Out-of-Order, Duplicates & Malformed Input
    // -------------------------------------------------------------------------
    {
        std::cout << "\n[Test 2] Testing chunk reassembly (out-of-order, duplicates, incomplete, malformed)..." << std::endl;
        SyncplayClient rxClient;
        int receivedCount = 0;
        QJsonObject lastPayload;
        QObject::connect(&rxClient, &SyncplayClient::fluxMessageReceived,
                         [&](const QString &, const QJsonObject &payload) {
                             ++receivedCount;
                             lastPayload = payload;
                         });

        // 2a: Truncated 150-char message (simulating server truncation of an unchunked 453-char message)
        const QString truncatedPreset5 = QStringLiteral(
            "FLUX|{\"t\":\"room_state\",\"id\":\"ant-man\",\"title\":\"Ant-Man and the Wasp: Quantumania\","
            "\"year\":2023,\"quality\":\"1080p\",\"duration\":7460,\"p\":3725,\"playing\":tru");
        rxClient.processIncomingChatForTest(QStringLiteral("FX_peer0001"), truncatedPreset5);
        if (receivedCount != 0) {
            std::cerr << "FAIL: Truncated JSON was not rejected!" << std::endl;
            return 1;
        }

        // 2b: Out-of-order + duplicate chunks reassembly
        QJsonObject original;
        original.insert(QStringLiteral("v"), 1);
        original.insert(QStringLiteral("t"), QStringLiteral("event"));
        original.insert(QStringLiteral("f"), QStringLiteral("FX_peer0001"));
        original.insert(QStringLiteral("e"), QStringLiteral("open"));
        QJsonObject dObj;
        dObj.insert(QStringLiteral("id"), QStringLiteral("english_movies_1080p:LongFolder/LongSubfolder/MovieName.mkv"));
        dObj.insert(QStringLiteral("n"), QStringLiteral("A Very Long Movie Title For Chunking Test"));
        original.insert(QStringLiteral("d"), dObj);

        const QByteArray rawJson = QJsonDocument(original).toJson(QJsonDocument::Compact);
        const QByteArray b64 = rawJson.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        const int sliceLen = 80;
        const int total = static_cast<int>((b64.size() + sliceLen - 1) / sliceLen);

        auto makeChunkMsg = [&](int idx) -> QString {
            QJsonObject c;
            c.insert(QStringLiteral("t"), QStringLiteral("c"));
            c.insert(QStringLiteral("m"), QStringLiteral("z9"));
            c.insert(QStringLiteral("i"), idx);
            c.insert(QStringLiteral("n"), total);
            c.insert(QStringLiteral("d"), QString::fromLatin1(b64.mid(idx * sliceLen, sliceLen)));
            return QStringLiteral("FLUX|") + QString::fromUtf8(QJsonDocument(c).toJson(QJsonDocument::Compact));
        };

        // Feed chunks in reverse order, with duplicates
        for (int i = total - 1; i >= 0; --i) {
            const QString chunkStr = makeChunkMsg(i);
            if (chunkStr.size() > SyncplayClient::kDefaultMaxChatLength) {
                std::cerr << "FAIL: Generated test chunk exceeds 150 chars (" << chunkStr.size() << ")" << std::endl;
                return 1;
            }
            rxClient.processIncomingChatForTest(QStringLiteral("FX_peer0001"), chunkStr);
            // Send duplicate of the same chunk immediately
            rxClient.processIncomingChatForTest(QStringLiteral("FX_peer0001"), chunkStr);
        }

        if (receivedCount != 1 || lastPayload != original) {
            std::cerr << "FAIL: Out-of-order chunked message did not reassemble into exact original JSON (count="
                      << receivedCount << ")." << std::endl;
            return 1;
        }
        std::cout << "PASS: Out-of-order + duplicate chunks reassembled accurately; truncated JSON rejected." << std::endl;
    }

    // -------------------------------------------------------------------------
    // Test 3: Host Session Creation on syncplay.pl:8995 (StartTLS + Hello + Heartbeat)
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 3] Creating Host TelepartySession on public Syncplay server (syncplay.pl:8995)..." << std::endl;
    TelepartySession hostSession;
    bool hostJoined = false;
    QObject::connect(&hostSession, &TelepartySession::joinedSession, [&]() {
        hostJoined = true;
    });

    // Track every raw chat frame sent by Host and Joiner to verify strict <= 150 char limit
    int maxObservedWireChars = 0;
    int totalWireFramesSent = 0;
    auto monitorClient = [&](TelepartySession &session) {
        if (SyncplayClient *client = session.findChild<SyncplayClient *>()) {
            QObject::connect(client, &SyncplayClient::rawChatSent, [&](const QString &chatText) {
                const int len = std::max(static_cast<int>(chatText.size()),
                                         static_cast<int>(chatText.toUtf8().size()));
                maxObservedWireChars = std::max(maxObservedWireChars, len);
                ++totalWireFramesSent;
                if (len > SyncplayClient::kDefaultMaxChatLength) {
                    std::cerr << "ERROR: Outgoing chat frame exceeded 150 chars (" << len << " chars): "
                              << chatText.toStdString() << std::endl;
                }
            });
        }
    };
    monitorClient(hostSession);

    hostSession.host();
    if (!waitForCondition(12000, [&]() { return hostJoined || !hostSession.errorText().isEmpty(); })) {
        std::cerr << "FAIL: Host session timed out waiting to join syncplay.pl:8995." << std::endl;
        return 1;
    }
    if (!hostJoined) {
        std::cerr << "FAIL: Host session failed with error: " << hostSession.errorText().toStdString() << std::endl;
        return 1;
    }

    const QString roomCode = hostSession.code();
    const QString mappedRoom = TelepartySession::roomNameForCode(roomCode);
    std::cout << "PASS: Host session established on syncplay.pl:8995! Code: " << roomCode.toStdString()
              << " -> Room: " << mappedRoom.toStdString()
              << ", state: " << hostSession.state().toStdString()
              << ", memberCount: " << hostSession.memberCount() << std::endl;

    // -------------------------------------------------------------------------
    // Test 4: Second Client Joins Room Using the 5-Digit Code
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 4] Joining room " << roomCode.toStdString() << " from second TelepartySession client..." << std::endl;
    TelepartySession joinerSession;
    monitorClient(joinerSession);

    bool joinerJoined = false;
    QString hostObservedNewMember;

    QObject::connect(&joinerSession, &TelepartySession::joinedSession, [&]() {
        joinerJoined = true;
    });
    QObject::connect(&hostSession, &TelepartySession::memberJoined, [&](const QString &memberId) {
        hostObservedNewMember = memberId;
    });

    // Attach TelepartySync to both sessions to test full movie selection & playback sync
    TelepartySync hostSync(&hostSession, nullptr);
    TelepartySync joinerSync(&joinerSession, nullptr);

    QString joinerOpenedUrl;
    QString joinerOpenedTitle;
    qint64 joinerOpenedStartMs = -1;
    QObject::connect(&joinerSync, &TelepartySync::remoteOpenRequested,
                     [&](const QString &url, const QString &title, qint64 startMs) {
                         joinerOpenedUrl = url;
                         joinerOpenedTitle = title;
                         joinerOpenedStartMs = startMs;
                     });

    joinerSession.join(roomCode);
    if (!waitForCondition(12000, [&]() {
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
    // Test 5: Movie Selection (Compact & Chunked), Play, Pause, and Seek Events
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 5] Testing movie selection (compact & chunked), play, pause, and seek across clients..." << std::endl;

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

    // 5a: Host selects built-in catalog movie ("Ant-Man and the Wasp: Quantumania") -> compact single-frame (< 150 chars)
    {
        const QString antManUrl = QStringLiteral(
            "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282023%29%201080p/"
            "Ant-Man%20and%20the%20Wasp-Quantumania%20%282023%29%201080p%20DSNP/"
            "Ant-Man%20and%20the%20Wasp%20Quantumania%20%282023%29%201080p%20DSNP-WEB%20x265%20HEVC%2010bit%20AAC%205.1%20MSubs-PSA.mkv");
        hostSync.broadcastOpen(antManUrl, QStringLiteral("Ant-Man and the Wasp: Quantumania"), 58000);

        if (!waitForCondition(6000, [&]() { return joinerOpenedUrl == antManUrl && joinerOpenedStartMs == 58000; })) {
            std::cerr << "FAIL: Joiner TelepartySync did not open compact catalog movie at startMs=58000 (got "
                      << joinerOpenedStartMs << ")." << std::endl;
            return 1;
        }
        std::cout << "PASS: Compact catalog movie 'open' with startMs=58000 received & resolved by Joiner (title=\""
                  << joinerOpenedTitle.toStdString() << "\", startMs=" << joinerOpenedStartMs << ")" << std::endl;
    }

    // 5b: Host selects a movie with a long BDIX catalog URL (> 150 chars raw) -> automatically chunked & reassembled
    {
        const QString longBdixUrl = QStringLiteral(
            "http://172.16.50.14/DHAKA-FLIX-14/English%20Movies%20%281080p%29/%282024%29%201080p/"
            "Spider-Man%20Across%20the%20Spider-Verse%20%282023%29%201080p%20BluRay%2010bit%20HEVC%20DTS-HD%20MA%205.1/"
            "Spider-Man.Across.the.Spider-Verse.2023.1080p.BluRay.x265.10bit.HEVC.AAC.5.1-FLUX.mkv");
        const QString longTitle = QStringLiteral("Spider-Man: Across the Spider-Verse (2023) [1080p BluRay HEVC]");
        hostSync.broadcastOpen(longBdixUrl, longTitle);

        if (!waitForCondition(8000, [&]() { return joinerOpenedUrl == longBdixUrl; })) {
            std::cerr << "FAIL: Joiner TelepartySync did not receive/reassemble chunked long BDIX movie open." << std::endl;
            return 1;
        }
        std::cout << "PASS: Chunked long BDIX movie 'open' reassembled & validated by Joiner (max wire frame="
                  << maxObservedWireChars << " chars <= 150)" << std::endl;
    }

    // 5c: Host sends "pause"
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 42500);
        hostSession.sendEvent(QStringLiteral("pause"), d);
        if (!waitForCondition(6000, [&]() { return lastJoinerEvent == QLatin1String("pause"); })) {
            std::cerr << "FAIL: Joiner did not receive 'pause' event." << std::endl;
            return 1;
        }
        std::cout << "PASS: 'pause' event received by Joiner (t="
                  << lastJoinerData.value(QStringLiteral("t")).toLongLong() << "ms)" << std::endl;
    }

    // 5d: Joiner sends "seek" back to Host
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 120000);
        joinerSession.sendEvent(QStringLiteral("seek"), d);
        if (!waitForCondition(6000, [&]() { return lastHostEvent == QLatin1String("seek"); })) {
            std::cerr << "FAIL: Host did not receive 'seek' event from Joiner." << std::endl;
            return 1;
        }
        std::cout << "PASS: 'seek' event from Joiner received by Host (t="
                  << lastHostData.value(QStringLiteral("t")).toLongLong() << "ms)" << std::endl;
    }

    // 5e: Joiner sends "play" back to Host
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 120000);
        joinerSession.sendEvent(QStringLiteral("play"), d);
        if (!waitForCondition(6000, [&]() { return lastHostEvent == QLatin1String("play"); })) {
            std::cerr << "FAIL: Host did not receive 'play' event from Joiner." << std::endl;
            return 1;
        }
        std::cout << "PASS: 'play' event from Joiner received by Host (t="
                  << lastHostData.value(QStringLiteral("t")).toLongLong() << "ms)" << std::endl;
    }

    // Verify no outgoing frame ever exceeded 150 chars
    if (maxObservedWireChars > SyncplayClient::kDefaultMaxChatLength) {
        std::cerr << "FAIL: Outgoing wire message exceeded 150-char limit (" << maxObservedWireChars << " chars)" << std::endl;
        return 1;
    }
    std::cout << "PASS: All " << totalWireFramesSent << " outgoing chat frames respected the 150-char limit (max="
              << maxObservedWireChars << " chars)." << std::endl;

    // -------------------------------------------------------------------------
    // Test 6: Simulate Disconnect, Automatic Reconnect & Verify No Duplicate Events
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 6] Testing disconnect, automatic reconnect, and non-duplicate delivery..." << std::endl;
    QSslSocket *joinerSocket = joinerSession.findChild<QSslSocket *>();
    if (!joinerSocket) {
        std::cerr << "FAIL: Could not locate joiner QSslSocket for disconnect simulation." << std::endl;
        return 1;
    }
    joinerSocket->disconnectFromHost();
    if (!waitForCondition(4000, [&]() { return joinerSession.state() == QLatin1String("reconnecting"); })) {
        std::cerr << "FAIL: Joiner did not enter 'reconnecting' state after socket drop." << std::endl;
        return 1;
    }
    std::cout << "PASS: Joiner entered 'reconnecting' state on drop." << std::endl;

    if (!waitForCondition(12000, [&]() { return joinerSession.state() == QLatin1String("joined"); })) {
        std::cerr << "FAIL: Joiner did not automatically reconnect to 'joined' state." << std::endl;
        return 1;
    }
    std::cout << "PASS: Joiner automatically reconnected and restored 'joined' state." << std::endl;

    // Verify a single event after reconnect arrives exactly once
    const int beforeCount = joinerEventCount;
    {
        QVariantMap d;
        d.insert(QStringLiteral("t"), 185000);
        hostSession.sendEvent(QStringLiteral("seek"), d);
        if (!waitForCondition(6000, [&]() { return joinerEventCount > beforeCount; })) {
            std::cerr << "FAIL: Joiner did not receive event after reconnect." << std::endl;
            return 1;
        }
        // Wait 600ms to ensure no duplicate copy arrives
        waitForCondition(600, [&]() { return false; });
        if (joinerEventCount != beforeCount + 1) {
            std::cerr << "FAIL: Expected exactly 1 event after reconnect, got "
                      << (joinerEventCount - beforeCount) << std::endl;
            return 1;
        }
        std::cout << "PASS: Post-reconnect event delivered exactly once." << std::endl;
    }

    // -------------------------------------------------------------------------
    // Test 7: Leave Room and Clean Up
    // -------------------------------------------------------------------------
    std::cout << "\n[Test 7] Testing participant leave & session cleanup..." << std::endl;
    joinerSession.leave();
    if (!waitForCondition(6000, [&]() {
            return joinerSession.state() == QLatin1String("idle") && hostSession.memberCount() == 1;
        })) {
        std::cerr << "FAIL: Host memberCount did not update to 1 after Joiner left." << std::endl;
        return 1;
    }
    std::cout << "PASS: Joiner left cleanly; Host memberCount is now " << hostSession.memberCount() << std::endl;

    hostSession.leave();
    std::cout << "PASS: Host left cleanly; state is now " << hostSession.state().toStdString() << std::endl;

    std::cout << "\n=== ALL TELEPARTY SYNCPLAY TESTS PASSED ===" << std::endl;
    return 0;
}
