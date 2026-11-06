#pragma once

#include <vlc/vlc.h>
#include <QString>
#include <QMutex>
#include <QWaitCondition>
#include <memory>

namespace Flux {

class VLCInstance {
public:
    static VLCInstance& instance();

    bool initialize(int subtitleScale = -1);
    void initializeAsync(int subtitleScale = -1);
    bool reinitialize(int subtitleScale);
    void shutdown();
    void waitUntilInitialized();

    libvlc_instance_t* get();
    bool isValid();
    bool isInitialized() const;

    QString lastError() const;

private:
    VLCInstance();
    ~VLCInstance();

    VLCInstance(const VLCInstance&) = delete;
    VLCInstance& operator=(const VLCInstance&) = delete;

    static void vlcLogCallback(void *data, int level, const libvlc_log_t *ctx, const char *fmt, va_list args);

    libvlc_instance_t *m_vlc = nullptr;
    QString m_lastError;
    bool m_initialized = false;
    bool m_initializing = false;
    mutable QMutex m_mutex;
    QWaitCondition m_waitCondition;
};

} // namespace Flux
