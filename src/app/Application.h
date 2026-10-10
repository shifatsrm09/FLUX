#pragma once

#include <QObject>
#include <QQmlApplicationEngine>
#include <memory>

#include "../media/VLCPlayer.h"
#include "../models/TestMedia.h"
#include "../search/SearchManager.h"
#include "../search/FolderBrowser.h"
#include "../core/UserStore.h"
#include "../core/Updater.h"
#include "../downloads/DownloadManager.h"
#include "../teleparty/TelepartySession.h"
#include "../teleparty/TelepartySync.h"

namespace Flux {

class Application : public QObject {
    Q_OBJECT
    Q_PROPERTY(Flux::VLCPlayer* player READ player CONSTANT)
    Q_PROPERTY(Flux::TestMediaModel* testMedia READ testMedia CONSTANT)
    Q_PROPERTY(Flux::SearchManager* searchManager READ searchManager CONSTANT)

public:
    explicit Application(QObject *parent = nullptr);
    ~Application() override;

    bool initialize(QQmlApplicationEngine &engine);

    VLCPlayer* player() const { return m_player.get(); }
    TestMediaModel* testMedia() const { return m_testMedia.get(); }
    SearchManager* searchManager() const { return m_searchManager.get(); }

private:
    std::unique_ptr<VLCPlayer> m_player;
    std::unique_ptr<TestMediaModel> m_testMedia;
    std::unique_ptr<SearchManager> m_searchManager;
    std::unique_ptr<FolderBrowser> m_folderBrowser;
    std::unique_ptr<DownloadManager> m_downloads;
    std::unique_ptr<TelepartySession> m_teleparty;
    std::unique_ptr<TelepartySync> m_sync;
    std::unique_ptr<UserStore> m_userStore;
    std::unique_ptr<Updater> m_updater;
};

} // namespace Flux
