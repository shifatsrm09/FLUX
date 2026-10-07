#include "Application.h"
#include "../media/VLCInstance.h"
#include "../media/VLCPlayer.h"
#include "../media/VLCVideoItem.h"
#include "../models/TestMedia.h"
#include "../core/Logger.h"
#include <QQmlContext>

namespace Flux {

Application::Application(QObject *parent)
    : QObject(parent) {
}

Application::~Application() {
    FLUX_LOG_INFO("Application", "Shutting down FLUX Application...");
    m_player.reset();
    m_testMedia.reset();
    m_searchManager.reset();
    VLCInstance::instance().shutdown();
}

bool Application::initialize(QQmlApplicationEngine &engine) {
    FLUX_LOG_INFO("Application", "Starting FLUX Media Player v0.0.2...");

    // 1. Initialize libVLC instance once
    if (!VLCInstance::instance().initialize()) {
        FLUX_LOG_ERROR("Application", "VLCInstance failed to initialize: " + VLCInstance::instance().lastError());
        return false;
    }

    // 2. Instantiate core media player, test catalog, and search manager
    m_player = std::make_unique<VLCPlayer>(this);
    m_testMedia = std::make_unique<TestMediaModel>(this);
    m_searchManager = std::make_unique<SearchManager>(this);

    // 3. Register QML types
    qmlRegisterType<VLCVideoItem>("Flux.Media", 1, 0, "VLCVideoItem");
    qmlRegisterUncreatableType<VLCPlayer>("Flux.Media", 1, 0, "VLCPlayer", "VLCPlayer is provided by Application");

    // 4. Inject global contextual objects into QML
    QQmlContext *rootContext = engine.rootContext();
    rootContext->setContextProperty("fluxApp", this);
    rootContext->setContextProperty("fluxPlayer", m_player.get());
    rootContext->setContextProperty("testMediaModel", m_testMedia.get());
    rootContext->setContextProperty("fluxSearch", m_searchManager.get());
    rootContext->setContextProperty("fluxLibrary", m_searchManager->libraryModel());
    rootContext->setContextProperty("fluxLogger", &Logger::instance());

    FLUX_LOG_INFO("Application", "Core services registered with QML engine");
    return true;
}

} // namespace Flux
