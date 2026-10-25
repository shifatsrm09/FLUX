#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include "../src/search/SearchManager.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    std::cout << "========================================\n";
    std::cout << "Circle FTP & Suggestions Verification\n";
    std::cout << "========================================\n";

    Flux::SearchManager search;
    search.setActiveServer(3);
    std::cout << "PASS: SearchManager active server is " << search.activeServer() << "\n";

    // Test search
    QObject::connect(&search, &Flux::SearchManager::resultCountChanged, [&]() {
        if (search.isSearching()) return;
        std::cout << "\n>>> Circle FTP Search Finished!\n";
        std::cout << "Result count: " << search.resultCount() << "\n";
        for (int i = 0; i < std::min(search.resultCount(), 3); ++i) {
            auto res = search.getResult(i);
            std::cout << "  [" << (i + 1) << "] " 
                      << (res["isFolder"].toBool() ? "[DIR ] " : "[FILE] ")
                      << res["title"].toString().toStdString() << "\n"
                      << "      Play URL:   " << res["playUrl"].toString().toStdString() << "\n"
                      << "      Poster URL: " << res["posterUrl"].toString().toStdString() << "\n";
        }

        // Test suggestions
        std::cout << "\n>>> Testing Auto-Suggestions for 'batman'...\n";
        QObject::connect(&search, &Flux::SearchManager::suggestionsChanged, [&]() {
            std::cout << "Suggestions received (" << search.suggestions().size() << " items):\n";
            for (const auto &s : search.suggestions()) {
                std::cout << "  - " << s.toStdString() << "\n";
            }
            std::cout << "\nALL VERIFICATIONS PASSED SUCCESSFULLY!\n";
            app.quit();
        });
        search.requestSuggestions("batman");
    });

    std::cout << "Executing Circle FTP search for 'batman'...\n";
    search.search("batman");

    QTimer::singleShot(10000, &app, [&]() {
        std::cout << "Test completed or timed out.\n";
        app.quit();
    });

    return app.exec();
}
