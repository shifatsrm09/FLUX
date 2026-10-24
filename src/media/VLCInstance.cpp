#include "VLCInstance.h"
#include "../core/Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QThreadPool>
#include <QSettings>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <vector>

namespace Flux {

VLCInstance& VLCInstance::instance() {
    static VLCInstance s_instance;
    return s_instance;
}

VLCInstance::VLCInstance() = default;

VLCInstance::~VLCInstance() {
    shutdown();
}

void VLCInstance::vlcLogCallback(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args) {
    Q_UNUSED(data);
    Q_UNUSED(ctx);

    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    QString msg = QString::fromUtf8(buffer).trimmed();

    if (msg.isEmpty()) return;

    LogLevel lvl = LogLevel::Debug;
    if (level == LIBVLC_ERROR) {
        lvl = LogLevel::Error;
    } else if (level == LIBVLC_WARNING) {
        lvl = LogLevel::Warning;
    } else if (level == LIBVLC_NOTICE || level == LIBVLC_DEBUG) {
        lvl = LogLevel::VLC;
    }

    Logger::instance().log(lvl, "libVLC", msg);
}

bool VLCInstance::initialize(int subtitleScale) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_initialized && m_vlc) {
            return true;
        }
        if (m_initializing) {
            while (m_initializing && !m_initialized) {
                m_waitCondition.wait(&m_mutex);
            }
            return m_vlc != nullptr;
        }
        m_initializing = true;
    }

    FLUX_LOG_INFO("VLCInstance", "Initializing libVLC engine...");

    // Search for plugins directory:
    // 1. App directory / plugins (production deployment)
    // 2. Installed VLC directory (development mode fallback)
    QString appDir = QCoreApplication::applicationDirPath();
    QString localPluginDir = QDir(appDir).filePath("plugins");
    QString systemVlcPluginDir = "C:/Program Files/VideoLAN/VLC/plugins";

    QString pluginPath;
    if (QDir(localPluginDir).exists()) {
        pluginPath = localPluginDir;
        FLUX_LOG_INFO("VLCInstance", QString("Using local plugins directory: %1").arg(pluginPath));
    } else if (QDir(systemVlcPluginDir).exists()) {
        pluginPath = systemVlcPluginDir;
        FLUX_LOG_INFO("VLCInstance", QString("Using system VLC plugins directory: %1").arg(pluginPath));
    }

    if (!pluginPath.isEmpty()) {
#ifdef Q_OS_WIN
        qputenv("VLC_PLUGIN_PATH", pluginPath.toUtf8());
#endif
    }

    // NOTE: --clock-jitter=0 / --clock-synchro=0 were removed. They disable libVLC's tolerance
    // for timing wobble and its A/V clock sync, which makes network playback drop frames and
    // stutter. libVLC's defaults are far smoother.
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FLUX"), QStringLiteral("FLUX"));
    int currentScale = (subtitleScale > 0) ? subtitleScale : settings.value("subtitles/scale", 100).toInt();
    if (currentScale < 10 || currentScale > 500) currentScale = 100;
    std::string subScaleArg = "--sub-text-scale=" + std::to_string(currentScale);

    std::vector<const char*> args = {
        "--no-video-title-show",
        "--network-caching=600",      // short pre-roll: fast starts/seeks and a shallow audio queue (volume changes are heard sooner)
        "--http-reconnect",            // Auto reconnect HTTP byte ranges
        "--no-snapshot-preview",
        "--quiet",
        subScaleArg.c_str()
    };

    libvlc_instance_t *vlc = libvlc_new(static_cast<int>(args.size()), args.data());

    QString lastError;
    if (!vlc) {
        const char *err = libvlc_errmsg();
        lastError = err ? QString::fromUtf8(err) : "Unknown libvlc_new failure";
        FLUX_LOG_ERROR("VLCInstance", QString("Failed to create libVLC instance: %1").arg(lastError));
    } else {
        // Set up libVLC log callback to forward internal VLC logs to our Logger
        libvlc_log_set(vlc, vlcLogCallback, this);

        const char *vlcVersion = libvlc_get_version();
        FLUX_LOG_INFO("VLCInstance", QString("libVLC successfully initialized (version %1)").arg(vlcVersion ? vlcVersion : "unknown"));
    }

    {
        QMutexLocker locker(&m_mutex);
        m_vlc = vlc;
        m_lastError = lastError;
        m_initialized = true;
        m_initializing = false;
        m_waitCondition.wakeAll();
    }

    return m_vlc != nullptr;
}

void VLCInstance::initializeAsync(int subtitleScale) {
    {
        QMutexLocker locker(&m_mutex);
        if (m_initialized || m_initializing) {
            return;
        }
        m_initializing = true;
    }

    FLUX_LOG_INFO("VLCInstance", "Starting asynchronous libVLC initialization in background thread...");

    QThreadPool::globalInstance()->start([this, subtitleScale]() {
        QString appDir = QCoreApplication::applicationDirPath();
        QString localPluginDir = QDir(appDir).filePath("plugins");
        QString systemVlcPluginDir = "C:/Program Files/VideoLAN/VLC/plugins";

        QString pluginPath;
        if (QDir(localPluginDir).exists()) {
            pluginPath = localPluginDir;
            FLUX_LOG_INFO("VLCInstance", QString("Using local plugins directory: %1").arg(pluginPath));
        } else if (QDir(systemVlcPluginDir).exists()) {
            pluginPath = systemVlcPluginDir;
            FLUX_LOG_INFO("VLCInstance", QString("Using system VLC plugins directory: %1").arg(pluginPath));
        }

        if (!pluginPath.isEmpty()) {
#ifdef Q_OS_WIN
            qputenv("VLC_PLUGIN_PATH", pluginPath.toUtf8());
#endif
        }

        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FLUX"), QStringLiteral("FLUX"));
        int currentScale = (subtitleScale > 0) ? subtitleScale : settings.value("subtitles/scale", 100).toInt();
        if (currentScale < 10 || currentScale > 500) currentScale = 100;
        std::string subScaleArg = "--sub-text-scale=" + std::to_string(currentScale);

        std::vector<const char*> args = {
            "--no-video-title-show",
            "--network-caching=600",
            "--http-reconnect",
            "--no-snapshot-preview",
            "--quiet",
            subScaleArg.c_str()
        };

        libvlc_instance_t *vlc = libvlc_new(static_cast<int>(args.size()), args.data());

        QString lastError;
        if (!vlc) {
            const char *err = libvlc_errmsg();
            lastError = err ? QString::fromUtf8(err) : "Unknown libvlc_new failure";
            FLUX_LOG_ERROR("VLCInstance", QString("Failed to create libVLC instance in background: %1").arg(lastError));
        } else {
            libvlc_log_set(vlc, vlcLogCallback, this);
            const char *vlcVersion = libvlc_get_version();
            FLUX_LOG_INFO("VLCInstance", QString("libVLC successfully initialized in background (version %1)").arg(vlcVersion ? vlcVersion : "unknown"));
        }

        {
            QMutexLocker locker(&m_mutex);
            m_vlc = vlc;
            m_lastError = lastError;
            m_initialized = true;
            m_initializing = false;
            m_waitCondition.wakeAll();
        }
    });
}

void VLCInstance::waitUntilInitialized() {
    QMutexLocker locker(&m_mutex);
    while (m_initializing && !m_initialized) {
        m_waitCondition.wait(&m_mutex);
    }
}

libvlc_instance_t* VLCInstance::get() {
    waitUntilInitialized();
    QMutexLocker locker(&m_mutex);
    return m_vlc;
}

bool VLCInstance::isValid() {
    waitUntilInitialized();
    QMutexLocker locker(&m_mutex);
    return m_vlc != nullptr;
}

bool VLCInstance::isInitialized() const {
    QMutexLocker locker(&m_mutex);
    return m_initialized;
}

QString VLCInstance::lastError() const {
    QMutexLocker locker(&m_mutex);
    return m_lastError;
}

bool VLCInstance::reinitialize(int subtitleScale) {
    shutdown();
    return initialize(subtitleScale);
}

void VLCInstance::shutdown() {
    waitUntilInitialized();
    QMutexLocker locker(&m_mutex);
    if (m_vlc) {
        FLUX_LOG_INFO("VLCInstance", "Releasing libVLC instance...");
        libvlc_log_unset(m_vlc);
        libvlc_release(m_vlc);
        m_vlc = nullptr;
    }
    m_initialized = false;
}

} // namespace Flux
