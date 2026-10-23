#include "Application.h"
#include "WindowsSnapHelper.h"
#include "../media/VLCInstance.h"
#include "../media/VLCPlayer.h"
#include "../media/VLCVideoItem.h"
#include "../models/TestMedia.h"
#include "../core/Logger.h"
#include "../core/Version.h"
#include <QCoreApplication>
#include <QQmlContext>
#include <QTimer>
#include <QThreadPool>

namespace Flux {

Application::Application(QObject *parent)
    : QObject(parent) {
}

Application::~Application() {
    FLUX_LOG_INFO("Application", "Shutting down FLUX Application...");
    // Leave any Teleparty session (tells the other members) before the player goes away
    m_sync.reset();
    m_teleparty.reset();
    m_updater.reset();
    // The user store records final watch progress from the player, so it goes first
    m_userStore.reset();
    m_bookmarks.reset();
    m_downloads.reset();   // aborts transfers (leaving resumable .part files) before the browser goes
    m_folderBrowser.reset();
    m_player.reset();
    m_testMedia.reset();
    m_searchManager.reset();
    VLCInstance::instance().shutdown();
}

bool Application::initialize(QQmlApplicationEngine &engine) {
    FLUX_LOG_INFO("Application", QString("Starting FLUX Media Player v%1...").arg(QString::fromLatin1(FLUX_VERSION)));

    // 1. Initialize libVLC instance asynchronously in background so splash and UI appear instantly
    VLCInstance::instance().initializeAsync();

    // 2. Instantiate core media player, test catalog, and search manager
    m_player = std::make_unique<VLCPlayer>(this);

    // Wire up player as soon as background VLC initialization completes
    QThreadPool::globalInstance()->start([this]() {
        if (VLCInstance::instance().get()) {
            QMetaObject::invokeMethod(m_player.get(), &VLCPlayer::ensureMediaPlayer, Qt::QueuedConnection);
        }
    });
    m_testMedia = std::make_unique<TestMediaModel>(this);
    m_searchManager = std::make_unique<SearchManager>(this);
    m_folderBrowser = std::make_unique<FolderBrowser>(this);
    m_downloads = std::make_unique<DownloadManager>(m_folderBrowser.get(), this);
    m_mediaCache = std::make_unique<MediaCache>(this);
    m_userStore = std::make_unique<UserStore>(this);
    m_bookmarks = std::make_unique<BookmarkManager>(this);
    m_updater = std::make_unique<Updater>(this);
    m_torrentManager = std::make_unique<TorrentManager>(this);
    // Pre-warm update check immediately so the splash popup has data without waiting
    m_updater->checkForUpdates(true);

    // Teleparty: watch together through a public MQTT broker (no server of ours)
    m_teleparty = std::make_unique<TelepartySession>(this);
    m_sync = std::make_unique<TelepartySync>(m_teleparty.get(), m_player.get(), this);
    m_sync->setForceAsyncInspection(true);
    LocalVerificationPolicy localPolicy;
    localPolicy.requireContentFingerprint = false;
    m_sync->setLocalVerificationPolicy(localPolicy);
    m_sync->setUserStore(m_userStore.get());

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

        const QString latchedName = store->value("subtitles/latchedName").toString();
        const QString latchedLang = store->value("subtitles/latchedLang").toString();
        const int latchedIndex = store->value("subtitles/latchedIndex", -1).toInt();
        const bool latchedDisabled = store->value("subtitles/latchedDisabled", false).toBool();
        player->setLatchedSubtitle(latchedName, latchedLang, latchedIndex, latchedDisabled);

        auto updateTelepartyCustomCode = [this, store]() {
            if (!m_teleparty) return;
            if (store->devModeUnlocked() && store->useCustomTelepartyCode()) {
                m_teleparty->setCustomRoomCode(store->customTelepartyCode());
            } else {
                m_teleparty->setCustomRoomCode(QString());
            }
        };
        updateTelepartyCustomCode();
        connect(store, &UserStore::customTelepartyCodeChanged, this, updateTelepartyCustomCode);
        connect(store, &UserStore::devModeUnlockedChanged, this, updateTelepartyCustomCode);

        // Multi-server settings (1 = Dhaka Flix, 2 = ICC FTP)
        search->setIccftpUrl(store->iccftpUrl());
        search->setIccftpSession(store->iccftpSession());
        search->setActiveServer(store->activeServer());

        // Selected categories are remembered separately for each server; the built-in default
        // applies when nothing is saved. Restored again whenever the user switches server.
        auto restoreCategories = [store, search]() {
            const int server = search->activeServer();
            QStringList saved = store->value(QStringLiteral("categories/selected_s%1").arg(server)).toStringList();
            if (saved.isEmpty() && server == 1) {
                saved = store->value(QStringLiteral("categories/selected")).toStringList();   // older versions
            }
            saved.removeAll(QString());
            search->restoreSelection(saved);
        };
        restoreCategories();
        connect(search, &SearchManager::activeServerChanged, store, restoreCategories);

        connect(store, &UserStore::serverChanged, search, [store, search]() {
            search->setIccftpUrl(store->iccftpUrl());
            search->setIccftpSession(store->iccftpSession());
            search->setActiveServer(store->activeServer());
        });

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
            store->setValue("subtitles/latchedName", player->latchedSubtitleName());
            store->setValue("subtitles/latchedLang", player->latchedSubtitleLang());
            store->setValue("subtitles/latchedIndex", player->latchedSubtitleIndex());
            store->setValue("subtitles/latchedDisabled", player->latchedSubtitleDisabled());
        });
        connect(search, &SearchManager::selectedLibrariesChanged, store, [store, search]() {
            store->setValue(QStringLiteral("categories/selected_s%1").arg(search->activeServer()),
                            search->selectedCategoryIds());
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

        // Extra library targets: folders scanned for playable media. Stored separately from
        // the download location and never written to by downloads.
        downloads->setTargets(store->value("targets/paths").toStringList());
        connect(downloads, &DownloadManager::targetsChanged, store, [store, downloads]() {
            store->setValue("targets/paths", downloads->targets());
        });

        // Bookmarks & Posters
        m_bookmarks->setUserStore(store);
        m_folderBrowser->setUserStore(store);

        connect(search, &SearchManager::posterFound, store,
                [store](const QString &url, const QString &posterUrl, const QString &title) {
            store->rememberPoster(url, posterUrl, title);
        });

        connect(m_folderBrowser.get(), &FolderBrowser::posterFound, store,
                [store](const QString &url, const QString &posterUrl, const QString &title) {
            store->rememberPoster(url, posterUrl, title);
        });

        connect(m_folderBrowser.get(), &FolderBrowser::seriesMappingFound, store,
                [store](const QString &mediaFolderUrl, const QString &webFolderUrl, const QString &posterUrl, const QString &title) {
            store->mapSeriesFolder(mediaFolderUrl, webFolderUrl);
            if (!posterUrl.isEmpty()) {
                store->rememberPoster(mediaFolderUrl, posterUrl, title);
                store->rememberPoster(webFolderUrl, posterUrl, title);
            }
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
    rootContext->setContextProperty("fluxCache", m_mediaCache.get());
    rootContext->setContextProperty("fluxUser", m_userStore.get());
    rootContext->setContextProperty("fluxBookmarks", m_bookmarks.get());
    rootContext->setContextProperty("fluxUpdater", m_updater.get());
    rootContext->setContextProperty("fluxTeleparty", m_teleparty.get());
    rootContext->setContextProperty("fluxSync", m_sync.get());
    rootContext->setContextProperty("fluxLibrary", m_searchManager->libraryModel());
    rootContext->setContextProperty("fluxLogger", &Logger::instance());
    rootContext->setContextProperty("fluxSnap", WindowsSnapHelper::instance());
    rootContext->setContextProperty("fluxTorrent", m_torrentManager.get());

    // 5. Startup update check is driven immediately by the Discord-style StartupPopup window

    FLUX_LOG_INFO("Application", "Core services registered with QML engine");
    return true;
}

} // namespace Flux
