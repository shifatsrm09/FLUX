#include <QGuiApplication>
#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QDir>
#include <QUrl>
#include <QList>
#include <QString>
#include <QIcon>
#include <QFile>
#include <QQuickWindow>
#include <QLocalServer>
#include <QLocalSocket>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include "app/Application.h"
#include "app/WindowsSnapHelper.h"
#include "core/Logger.h"
#include "core/Version.h"

int main(int argc, char *argv[]) {
    // High-DPI and modern desktop rendering defaults
    QGuiApplication::setOrganizationName("FLUX");
    QGuiApplication::setOrganizationDomain("flux.local");
    QGuiApplication::setApplicationName("FLUX");
    QGuiApplication::setApplicationDisplayName("FLUX");
    QGuiApplication::setApplicationVersion(QString::fromLatin1(FLUX_VERSION));

    QGuiApplication app(argc, argv);

#ifdef Q_OS_WIN
    // Ensure normal process priority class even when launched by Windows boot sequence
    SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

    // Single-instance enforcement: if FLUX is already running, activate it and exit
    HANDLE hSingleInstanceMutex = CreateMutexW(NULL, TRUE, L"Local\\FLUX_SingleInstance_Mutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        AllowSetForegroundWindow(ASFW_ANY);
        UINT activateMsg = RegisterWindowMessageW(L"FLUX_ACTIVATE_INSTANCE");
        if (activateMsg) {
            PostMessageW(HWND_BROADCAST, activateMsg, 0, 0);
            HWND hwnd = FindWindowW(NULL, L"FLUX");
            if (hwnd) {
                PostMessageW(hwnd, activateMsg, 0, 0);
            }
        }
        if (hSingleInstanceMutex) {
            CloseHandle(hSingleInstanceMutex);
        }
        return 0;
    }
#endif

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString icoPath = QDir(appDir).filePath(QStringLiteral("resources/flux.ico"));
    if (QFile::exists(icoPath)) {
        app.setWindowIcon(QIcon(icoPath));
    }

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

    FLUX_LOG_INFO("Main", QString("Initializing FLUX v%1 Native Desktop Player... Log path: %2")
                              .arg(QString::fromLatin1(FLUX_VERSION), Flux::Logger::instance().logFilePath()));

    QQmlApplicationEngine engine;
    Flux::Application fluxApp;



    bool startedFromBoot = false;
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QStringLiteral("--autostart") || arg == QStringLiteral("-autostart")) {
            startedFromBoot = true;
            break;
        }
    }

    if (!fluxApp.initialize(engine, startedFromBoot)) {
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

    // Instantly instantiate and display the Discord-style StartupPopup window (< 30ms)
    const QUrl popupUrl(QStringLiteral("qrc:/FLUX/qml/components/StartupPopup.qml"));
    QQmlComponent startupComponent(&engine, popupUrl);
    if (startupComponent.isError()) {
        const QString localPopupPath = QDir(QGuiApplication::applicationDirPath()).filePath("../../../qml/components/StartupPopup.qml");
        startupComponent.loadUrl(QUrl::fromLocalFile(localPopupPath));
    }

    QObject *startupObj = startupComponent.create();
    QQuickWindow *startupWindow = qobject_cast<QQuickWindow *>(startupObj);
    if (startupWindow) {
        engine.rootContext()->setContextProperty(QStringLiteral("fluxStartupPopup"), startupWindow);
        startupWindow->show();
        startupWindow->raise();
        startupWindow->requestActivate();
        QCoreApplication::processEvents();
    } else {
        engine.rootContext()->setContextProperty(QStringLiteral("fluxStartupPopup"), nullptr);
        if (startupComponent.isError()) {
            for (const auto &err : startupComponent.errors()) {
                FLUX_LOG_WARN("StartupPopup", err.toString());
            }
        }
    }

    const QUrl qmlResourceUrl(QStringLiteral("qrc:/FLUX/qml/Main.qml"));
    engine.load(qmlResourceUrl);

    if (engine.rootObjects().isEmpty()) {
        const QString localQmlPath = QDir(QGuiApplication::applicationDirPath()).filePath("../../../qml/Main.qml");
        FLUX_LOG_WARN("Main", "QRC QML not found, falling back to local file path: " + localQmlPath);
        engine.load(QUrl::fromLocalFile(localQmlPath));
    }

    if (engine.rootObjects().isEmpty()) {
        FLUX_LOG_ERROR("Main", "Unable to load QML interface!");
        return -1;
    }

    QQuickWindow *quickWindow = nullptr;
    for (QObject *root : engine.rootObjects()) {
        QQuickWindow *w = qobject_cast<QQuickWindow *>(root);
        if (w && w != startupWindow) {
            quickWindow = w;
            break;
        }
    }

#ifdef Q_OS_WIN
    if (quickWindow) {
        fluxApp.systemManager()->setWindow(quickWindow);
        Flux::installWindowsSnapHelper(quickWindow);
    }
#endif

    FLUX_LOG_INFO("Main", "FLUX UI successfully launched. Entering main event loop.");
#ifdef Q_OS_WIN
    int exitCode = app.exec();
    if (hSingleInstanceMutex) {
        CloseHandle(hSingleInstanceMutex);
    }
    return exitCode;
#else
    return app.exec();
#endif
}
