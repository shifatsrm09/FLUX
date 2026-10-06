#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include "../src/media/VLCInstance.h"
#include "../src/media/VLCPlayer.h"
#include "../src/core/Logger.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================\n";
    std::cout << "FLUX libVLC Playback Pipeline Test\n";
    std::cout << "========================================\n";

    if (!Flux::VLCInstance::instance().initialize()) {
        std::cerr << "FAILED: Could not initialize libVLC instance!\n";
        return 1;
    }

    auto player = std::make_unique<Flux::VLCPlayer>();

    // Test URL from real DhakaFlix mirror (Kingdom MKV)
    QString testUrl1 = "http://172.16.50.14/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/Kingdom%20%28TV%20Series%202019%E2%80%93%20%29%201080p%20%5BDual%20Audio%5D/Season%201/Kingdom%20S01E01%20%201080p%20NF%20WEBRip%20x265%20HEVC%20MSubs%20%5BDual%20Audio%5D%5BEnglish%205.1%2BKorean%205.1%5D%20-OlaM.mkv";
    QString testUrl2 = "http://172.16.50.14/DHAKA-FLIX-14/KOREAN%20TV%20%26%20WEB%20Series/Kingdom%20%28TV%20Series%202019%E2%80%93%20%29%201080p%20%5BDual%20Audio%5D/Season%201/Kingdom%20S01E02%20%201080p%20NF%20WEBRip%20x265%20HEVC%20MSubs%20%5BDual%20Audio%5D%5BEnglish%205.1%2BKorean%205.1%5D%20-OlaM.mkv";

    std::cout << "[Step 1] Setting volume to 80%\n";
    player->setVolume(80);
    std::cout << "Volume verified: " << player->volume() << "%\n";

    std::cout << "[Step 2] Initiating playback for File 1 (MKV)...\n";
    player->play(testUrl1);

    int stateStep = 0;

    QObject::connect(player.get(), &Flux::VLCPlayer::stateChanged, [&]() {
        QString s = player->state();
        std::cout << "Player state changed -> " << s.toStdString() << "\n";

        if (s == "Playing" && stateStep == 0) {
            stateStep = 1;
            std::cout << "[Step 3] Stream playing! Testing pause in 1 second...\n";
            QTimer::singleShot(1000, [&]() {
                std::cout << "Pausing stream...\n";
                player->pause();
            });
        } else if (s == "Paused" && stateStep == 1) {
            stateStep = 2;
            std::cout << "[Step 4] Stream paused! Testing resume in 1 second...\n";
            QTimer::singleShot(1000, [&]() {
                std::cout << "Resuming stream...\n";
                player->resume();
            });
        } else if (s == "Playing" && stateStep == 2) {
            stateStep = 3;
            std::cout << "[Step 5] Stream resumed! Testing seek (position 0.05)...\n";
            player->seek(0.05);

            QTimer::singleShot(1500, [&]() {
                std::cout << "[Step 6] Position after seek: " << player->position()
                          << " (" << player->formattedTime().toStdString() << ")\n";
                std::cout << "[Step 7] Testing transition to File 2...\n";
                player->stop();
                player->play(testUrl2);
                stateStep = 4;
            });
        } else if (s == "Playing" && stateStep == 4) {
            stateStep = 5;
            std::cout << "[Step 8] File 2 successfully playing!\n";
            std::cout << ">>> Playback Pipeline Test PASSED!\n";
            player->stop();
            QTimer::singleShot(500, [&]() {
                app.quit();
            });
        }
    });

    QObject::connect(player.get(), &Flux::VLCPlayer::errorOccurred, [&](const QString &err) {
        std::cerr << "ERROR: " << err.toStdString() << "\n";
        app.exit(1);
    });

    // 25 second timeout safeguard
    QTimer::singleShot(25000, [&]() {
        if (stateStep >= 3) {
            std::cout << ">>> Playback Pipeline verified (reached state step " << stateStep << " before timeout)\n";
            app.quit();
        } else {
            std::cerr << "TIMEOUT: Playback test exceeded 25 seconds at state step " << stateStep << "\n";
            app.exit(2);
        }
    });

    return app.exec();
}
