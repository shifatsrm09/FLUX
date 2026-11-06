#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include "../src/torrent/TorrentManager.h"
#include "../src/core/Logger.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================\n";
    std::cout << "FLUX Tork Multi-Engine Search Test\n";
    std::cout << "========================================\n";

    Flux::TorrentManager torrentMgr;

    QString testQuery = "dune";
    if (argc > 1) {
        testQuery = QString::fromUtf8(argv[1]);
    }

    std::cout << "Searching Tork Multi-Engine for: \"" << testQuery.toStdString() << "\"...\n";

    // Timeout safety
    QTimer::singleShot(20000, &app, [&]() {
        std::cerr << "Test timed out!\n";
        app.exit(1);
    });

    QObject::connect(&torrentMgr, &Flux::TorrentManager::torkResultsChanged, [&]() {
        if (torrentMgr.isSearchingTork()) return;

        QVariantList results = torrentMgr.torkResults();
        std::cout << "\n>>> Tork Search Completed!\n";
        std::cout << "Aggregated unique result count: " << results.size() << "\n";

        if (!torrentMgr.torkError().isEmpty()) {
            std::cout << "Note/Error: " << torrentMgr.torkError().toStdString() << "\n";
        }

        if (results.isEmpty()) {
            std::cerr << "FAIL: No search results returned from Tork!\n";
            app.exit(1);
            return;
        }

        int toShow = std::min<int>(static_cast<int>(results.size()), 5);
        for (int i = 0; i < toShow; ++i) {
            QVariantMap item = results[i].toMap();
            std::cout << "\n[" << (i + 1) << "] " << item["name"].toString().toStdString() << "\n";
            std::cout << "    Seeds: " << item["seeds"].toInt()
                      << " | Leeches: " << item["leeches"].toInt()
                      << " | Size: " << item["size"].toString().toStdString()
                      << " | Category: " << item["category"].toString().toStdString()
                      << " | Providers: " << item["provider"].toString().toStdString() << "\n";
            std::cout << "    Magnet: " << item["magnet"].toString().mid(0, 60).toStdString() << "...\n";
        }

        std::cout << "\n>>> Tork Multi-Engine Verification PASSED!\n";
        app.exit(0);
    });

    torrentMgr.searchTork(testQuery, "all");

    return app.exec();
}
