#include "Application.h"
#include "../media/VLCInstance.h"
#include "../media/VLCPlayer.h"
#include "../media/VLCVideoItem.h"
#include "../models/TestMedia.h"
#include "../core/Logger.h"
#include <QCoreApplication>
#include <QQmlContext>

namespace Flux {

Application::Application(QObject *parent)
    : QObject(parent) {
}

Application::~Application() {
    FLUX_LOG_INFO("Application", "Shutting down FLUX Application...");
    // The user store records final watch progress from the player, so it goes first
    m_userStore.reset();
    m_downloads.reset();   // aborts transfers (leaving resumable .part files) before the browser goes
    m_folderBrowser.reset();
    m_player.reset();
    m_testMedia.reset();
    m_searchManager.reset();
    VLCInstance::instance().shutdown();
}

bool Application::initialize(QQmlApplicationEngine &engine) {
    FLUX_LOG_INFO("Application", "Starting FLUX Media Player v0.0.3...");

    // 1. Initialize libVLC instance once
    if (!VLCInstance::instance().initialize()) {
        FLUX_LOG_ERROR("Application", "VLCInstance failed to initialize: " + VLCInstance::instance().lastError());
        return false;
    }

    // 2. Instantiate core media player, test catalog, and search manager
    m_player = std::make_unique<VLCPlayer>(this);
    m_testMedia = std::make_unique<TestMediaModel>(this);
    m_searchManager = std::make_unique<SearchManager>(this);
    m_folderBrowser = std::make_unique<FolderBrowser>(this);
    m_downloads = std::make_unique<DownloadManager>(m_folderBrowser.get(), this);
    m_userStore = std::make_unique<UserStore>(this);

    // 2b. Restore saved user settings
    {
        UserStore *store = m_userStore.get();
        VLCPlayer *player = m_player.get();
        SearchManager *search = m_searchManager.get();

        // Audio
        player->setVolume(store->intValue("audio/volume", 100));
        player->setMuted(store->boolValue("audio/muted", false));

        // Preferred languages: English first by default (then Hindi). Choices the user makes
        // in the track panel are learned and take over from these defaults.
        // One-time reset (prefs v2): English is the new default, so drop older learned choices.
        if (store->intValue("audio/prefsVersion", 0) < 2) {
            store->setValue("audio/preferredAudio", QStringList());
            store->setValue("audio/preferredSubtitle", QString());
            store->setValue("audio/prefsVersion", 2);
        }

        QStringList audioPrefs = store->value("audio/preferredAudio").toStringList();
        audioPrefs.removeAll(QString());
        if (audioPrefs.isEmpty()) {
            audioPrefs = QStringList{QStringLiteral("English"), QStringLiteral("Hindi")};
        }

        // Subtitles: English if the file has them. Picking "Disable" in the panel is learned
        // as "off" and sticks.
        QString subtitlePref = store->value("audio/preferredSubtitle").toString().trimmed();
        if (subtitlePref.isEmpty()) {
            subtitlePref = QStringLiteral("English");
        }
        player->setLanguagePreferences(audioPrefs, subtitlePref);

        // Selected categories (falls back to the built-in default when nothing is saved)
        QStringList savedCategories = store->value("categories/selected").toStringList();
        savedCategories.removeAll(QString());
        search->restoreSelection(savedCategories);

        // 2c. Persist changes as they happen
        connect(player, &VLCPlayer::volumeChanged, store, [store, player]() {
            store->setValue("audio/volume", player->volume());
        });
        connect(player, &VLCPlayer::muteChanged, store, [store, player]() {
            store->setValue("audio/muted", player->isMuted());
        });
        connect(player, &VLCPlayer::languagePreferencesChanged, store, [store, player]() {
            store->setValue("audio/preferredAudio", player->preferredAudio());
            store->setValue("audio/preferredSubtitle", player->preferredSubtitle());
        });
        connect(search, &SearchManager::selectedLibrariesChanged, store, [store, search]() {
            store->setValue("categories/selected", search->selectedCategoryIds());
        });

        // Download location (defaults to <Downloads>/FLUX when nothing is saved)
        DownloadManager *downloads = m_downloads.get();
        const QString savedLocation = store->value("downloads/location").toString();
        if (!savedLocation.isEmpty()) {
            downloads->changeLocation(savedLocation);
        }
        connect(downloads, &DownloadManager::locationChanged, store, [store, downloads]() {
            store->setValue("downloads/location", downloads->downloadLocation());
        });

        // Watch progress (resume / continue watching / watched)
        store->attachPlayer(player);
        connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, store, &UserStore::shutdown);
    }

    // 3. Register QML types
    qmlRegisterType<VLCVideoItem>("Flux.Media", 1, 0, "VLCVideoItem");
    qmlRegisterUncreatableType<VLCPlayer>("Flux.Media", 1, 0, "VLCPlayer", "VLCPlayer is provided by Application");

    // 4. Inject global contextual objects into QML
    QQmlContext *rootContext = engine.rootContext();
    rootContext->setContextProperty("fluxApp", this);
    rootContext->setContextProperty("fluxPlayer", m_player.get());
    rootContext->setContextProperty("testMediaModel", m_testMedia.get());
    rootContext->setContextProperty("fluxSearch", m_searchManager.get());
    rootContext->setContextProperty("fluxBrowser", m_folderBrowser.get());
    rootContext->setContextProperty("fluxDownloads", m_downloads.get());
    rootContext->setContextProperty("fluxUser", m_userStore.get());
    rootContext->setContextProperty("fluxLibrary", m_searchManager->libraryModel());
    rootContext->setContextProperty("fluxLogger", &Logger::instance());

    FLUX_LOG_INFO("Application", "Core services registered with QML engine");
    return true;
}

} // namespace Flux
