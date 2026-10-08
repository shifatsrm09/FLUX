#include "VLCInstance.h"
#include "../core/Logger.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
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

bool VLCInstance::initialize() {
    if (m_initialized && m_vlc) {
        return true;
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
    std::vector<const char*> args = {
        "--no-video-title-show",
        "--network-caching=1500",      // short pre-roll: fast starts and seeks on the BDIX LAN
        "--http-reconnect",            // Auto reconnect HTTP byte ranges
        "--no-snapshot-preview",
        "--quiet"
    };

    m_vlc = libvlc_new(static_cast<int>(args.size()), args.data());

    if (!m_vlc) {
        const char *err = libvlc_errmsg();
        m_lastError = err ? QString::fromUtf8(err) : "Unknown libvlc_new failure";
        FLUX_LOG_ERROR("VLCInstance", QString("Failed to create libVLC instance: %1").arg(m_lastError));
        return false;
    }

    // Set up libVLC log callback to forward internal VLC logs to our Logger
    libvlc_log_set(m_vlc, vlcLogCallback, this);

    const char *vlcVersion = libvlc_get_version();
    FLUX_LOG_INFO("VLCInstance", QString("libVLC successfully initialized (version %1)").arg(vlcVersion ? vlcVersion : "unknown"));

    m_initialized = true;
    return true;
}

void VLCInstance::shutdown() {
    if (m_vlc) {
        FLUX_LOG_INFO("VLCInstance", "Releasing libVLC instance...");
        libvlc_log_unset(m_vlc);
        libvlc_release(m_vlc);
        m_vlc = nullptr;
    }
    m_initialized = false;
}

} // namespace Flux
