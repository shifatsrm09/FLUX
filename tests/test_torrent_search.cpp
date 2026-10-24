#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include "../src/torrent/TorrentManager.h"
#include "../src/core/Logger.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================\n";
    std::cout << "FLUX 1337x Torrent Search Integration Test\n";
    std::cout << "========================================\n";

    Flux::TorrentManager torrentMgr;

    QString testQuery = "inception";
    if (argc > 1) {
        testQuery = QString::fromUtf8(argv[1]);
    }

    std::cout << "Searching 1337x for: \"" << testQuery.toStdString() << "\"...\n";

    // Timeout safety
    QTimer::singleShot(20000, &app, [&]() {
        std::cerr << "Test timed out!\n";
        app.exit(1);
    });

    QMetaObject::Connection conn;
    conn = QObject::connect(&torrentMgr, &Flux::TorrentManager::searchResultsChanged, [&]() {
        if (torrentMgr.isSearching1337x()) return;

        QVariantList results = torrentMgr.searchResults();
        std::cout << "\n>>> Search Completed!\n";
        std::cout << "Result count: " << results.size() << "\n";

        if (!torrentMgr.searchError().isEmpty()) {
            std::cout << "Note/Error: " << torrentMgr.searchError().toStdString() << "\n";
        }

        if (results.isEmpty()) {
            std::cerr << "FAIL: No search results returned!\n";
            app.exit(1);
            return;
        }

        int toShow = std::min<int>(static_cast<int>(results.size()), 3);
        for (int i = 0; i < toShow; ++i) {
            QVariantMap item = results[i].toMap();
            std::cout << "\n[" << (i + 1) << "] " << item["name"].toString().toStdString() << "\n";
            std::cout << "    Seeds: " << item["seeds"].toInt()
                      << " | Leeches: " << item["leeches"].toInt()
                      << " | Size: " << item["size"].toString().toStdString()
                      << " | Date: " << item["date"].toString().toStdString() << "\n";
            std::cout << "    Detail URL: " << item["fullDetailUrl"].toString().toStdString() << "\n";
        }

        std::cout << "\nResolving magnet link for result [0]...\n";
        QObject::disconnect(conn);

        QObject::connect(&torrentMgr, &Flux::TorrentManager::torrentChanged, [&]() {
            std::cout << "Torrent loaded! Name: " << torrentMgr.torrentName().toStdString() << "\n";
            std::cout << "Status: " << torrentMgr.status().toStdString() << "\n";
            std::cout << "\nMagnet resolution and stream start verification PASSED!\n";
            app.exit(0);
        });

        torrentMgr.streamSearchResult(0);
    });

    torrentMgr.search1337x(testQuery, "Movies");

    return app.exec();
}
