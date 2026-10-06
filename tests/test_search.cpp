#include <QCoreApplication>
#include <QTimer>
#include <iostream>
#include "../src/search/SearchManager.h"
#include "../src/core/Logger.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    std::cout << "========================================\n";
    std::cout << "FLUX SearchManager Integration Test\n";
    std::cout << "========================================\n";

    Flux::SearchManager searchMgr;

    QString testQuery = "kingdom";
    if (argc > 1) {
        testQuery = QString::fromUtf8(argv[1]);
    }

    std::cout << "Searching media server (172.16.50.14) for: " << testQuery.toStdString() << "\n";

    QObject::connect(&searchMgr, &Flux::SearchManager::resultCountChanged, [&]() {
        if (searchMgr.isSearching()) return;

        std::cout << "\n>>> Search Completed!\n";
        std::cout << "Status: " << searchMgr.statusMessage().toStdString() << "\n";
        std::cout << "Result Count: " << searchMgr.resultCount() << "\n";

        if (searchMgr.hasError()) {
            std::cout << "ERROR: " << searchMgr.errorMessage().toStdString() << "\n";
            app.exit(1);
            return;
        }

        int count = searchMgr.resultCount();
        int displayLimit = std::min(count, 5);
        std::cout << "Showing first " << displayLimit << " items:\n";

        for (int i = 0; i < displayLimit; ++i) {
            QVariantMap item = searchMgr.getResult(i);
            std::cout << "  [" << (i + 1) << "] "
                      << (item["isFolder"].toBool() ? "[DIR ] " : "[FILE] ")
                      << item["title"].toString().toStdString() << "\n"
                      << "      Path: " << item["parentPath"].toString().toStdString() << "\n"
                      << "      Size: " << item["formattedSize"].toString().toStdString()
                      << " (" << item["extension"].toString().toStdString() << ")\n"
                      << "      URL:  " << item["playUrl"].toString().toStdString() << "\n";
        }

        std::cout << "\nTest passed successfully.\n";
        app.quit();
    });

    // Timeout after 10 seconds
    QTimer::singleShot(10000, [&]() {
        std::cerr << "\nTIMEOUT: Search took longer than 10s.\n";
        app.exit(2);
    });

    searchMgr.search(testQuery);

    return app.exec();
}
