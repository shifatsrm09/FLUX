#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QDir>
#include <QUrl>
#include "app/Application.h"
#include "core/Logger.h"

int main(int argc, char *argv[]) {
    // High-DPI and modern desktop rendering defaults
    QGuiApplication::setOrganizationName("FLUX");
    QGuiApplication::setOrganizationDomain("flux.local");
    QGuiApplication::setApplicationName("FLUX");
    QGuiApplication::setApplicationDisplayName("FLUX");

    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");

    FLUX_LOG_INFO("Main", "Initializing FLUX Native Desktop Player...");

    QQmlApplicationEngine engine;
    Flux::Application fluxApp;

    if (!fluxApp.initialize(engine)) {
        FLUX_LOG_ERROR("Main", "Failed to initialize FLUX application services!");
        return -1;
    }

    // Try loading QML from Qt resource system (qt_add_qml_module) or local disk fallback
    const QUrl qmlResourceUrl(u"qrc:/FLUX/qml/Main.qml"_s);
    const QString localQmlPath = QDir(QGuiApplication::applicationDirPath()).filePath("../qml/Main.qml");

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [qmlResourceUrl](QObject *obj, const QUrl &objUrl) {
        if (!obj && objUrl == qmlResourceUrl) {
            FLUX_LOG_ERROR("Main", "Failed to instantiate root QML object!");
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(qmlResourceUrl);

    if (engine.rootObjects().isEmpty()) {
        FLUX_LOG_WARN("Main", "QRC QML not found, falling back to local file path: " + localQmlPath);
        engine.load(QUrl::fromLocalFile(localQmlPath));
    }

    if (engine.rootObjects().isEmpty()) {
        FLUX_LOG_ERROR("Main", "Unable to load QML interface!");
        return -1;
    }

    FLUX_LOG_INFO("Main", "FLUX UI successfully launched. Entering main event loop.");
    return app.exec();
}
