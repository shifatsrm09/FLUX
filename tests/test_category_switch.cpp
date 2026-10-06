#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include <cassert>
#include "../src/search/SearchManager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================\n";
    std::cout << "FLUX Category Switching Integration Test\n";
    std::cout << "========================================\n";

    Flux::SearchManager searchMgr;
    int step = 1;

    QObject::connect(&searchMgr, &Flux::SearchManager::resultCountChanged, [&]() {
        if (searchMgr.isSearching()) return;

        if (step == 1) {
            std::cout << "\n[Step 1 Completed] English Movies -> Batman\n";
            std::cout << "Results: " << searchMgr.resultCount() << "\n";
            std::cout << "Library: " << searchMgr.selectedLibraryName().toStdString() << "\n";
            assert(searchMgr.resultCount() > 0);
            assert(searchMgr.selectedLibraryId() == "english_movies");

            // Step 2: Switch to Korean TV & Web Series and search "Kingdom"
            step = 2;
            std::cout << "\n[Step 2 Initiated] Switching to Korean TV & Web Series -> Kingdom\n";
            searchMgr.selectLibraryById("korean_series");
            searchMgr.search("Kingdom");
        } else if (step == 2) {
            std::cout << "\n[Step 2 Completed] Korean TV & Web Series -> Kingdom\n";
            std::cout << "Results: " << searchMgr.resultCount() << "\n";
            std::cout << "Library: " << searchMgr.selectedLibraryName().toStdString() << "\n";
            assert(searchMgr.resultCount() == 14);
            assert(searchMgr.selectedLibraryId() == "korean_series");

            // Verify no Batman items remain
            for (int i = 0; i < searchMgr.resultCount(); ++i) {
                QVariantMap item = searchMgr.getResult(i);
                std::string title = item["title"].toString().toStdString();
                assert(title.find("Batman") == std::string::npos);
            }

            std::cout << "\n>>> Category Switching Test PASSED: No stale results observed!\n";
            app.quit();
        }
    });

    // Start Step 1
    std::cout << "\n[Step 1 Initiated] English Movies -> Batman\n";
    searchMgr.selectLibraryById("english_movies");
    searchMgr.search("Batman");

    QTimer::singleShot(20000, [&]() {
        std::cerr << "TIMEOUT: Category switching test timed out.\n";
        app.exit(2);
    });

    return app.exec();
}
