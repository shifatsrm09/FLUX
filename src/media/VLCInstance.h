#pragma once

#include <vlc/vlc.h>
#include <QString>
#include <memory>

namespace Flux {

class VLCInstance {
public:
    static VLCInstance& instance();

    bool initialize();
    void shutdown();

    libvlc_instance_t* get() const { return m_vlc; }
    bool isValid() const { return m_vlc != nullptr; }

    QString lastError() const { return m_lastError; }

private:
    VLCInstance();
    ~VLCInstance();

    VLCInstance(const VLCInstance&) = delete;
    VLCInstance& operator=(const VLCInstance&) = delete;

    static void vlcLogCallback(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args);

    libvlc_instance_t *m_vlc = nullptr;
    QString m_lastError;
    bool m_initialized = false;
};

} // namespace Flux
