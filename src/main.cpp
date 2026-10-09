#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QDir>
#include <QUrl>
#include <QList>
#include <QString>
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

    // Install Qt message handler to route Qt warnings/errors to our release log file
    qInstallMessageHandler([](QtMsgType type, const QMessageLogContext &context, const QString &msg) {
        Q_UNUSED(context);
        Flux::LogLevel level = Flux::LogLevel::Info;
        switch (type) {
        case QtDebugMsg:    level = Flux::LogLevel::Debug; break;
        case QtInfoMsg:     level = Flux::LogLevel::Info; break;
        case QtWarningMsg:  level = Flux::LogLevel::Warning; break;
        case QtCriticalMsg: level = Flux::LogLevel::Error; break;
        case QtFatalMsg:    level = Flux::LogLevel::Error; break;
        }
        Flux::Logger::instance().log(level, "Qt", msg);
    });

    FLUX_LOG_INFO("Main", QString("Initializing FLUX v0.0.3 Native Desktop Player... Log path: %1")
                              .arg(Flux::Logger::instance().logFilePath()));

    QQmlApplicationEngine engine;
    Flux::Application fluxApp;

    if (!fluxApp.initialize(engine)) {
        FLUX_LOG_ERROR("Main", "Failed to initialize FLUX application services!");
        return -1;
    }

    using namespace Qt::StringLiterals;

    // Connect QML engine warnings to logger for full visibility
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, [](const QList<QQmlError> &warnings) {
        for (const auto &w : warnings) {
            FLUX_LOG_WARN("QML", w.toString());
        }
    });

    const QUrl qmlResourceUrl(QStringLiteral("qrc:/FLUX/qml/Main.qml"));
    const QUrl qmlResourceUrlQt6(QStringLiteral("qrc:/qt/qml/FLUX/qml/Main.qml"));
    const QString localQmlPath = QDir(QGuiApplication::applicationDirPath()).filePath("../../../qml/Main.qml");

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [qmlResourceUrl](QObject *obj, const QUrl &objUrl) {
        if (!obj && objUrl == qmlResourceUrl) {
            FLUX_LOG_ERROR("Main", "Failed to instantiate root QML object from URL: " + objUrl.toString());
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(qmlResourceUrl);

    if (engine.rootObjects().isEmpty()) {
        engine.load(qmlResourceUrlQt6);
    }

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
