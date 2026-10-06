#pragma once

#include <QQuickPaintedItem>
#include <QImage>
#include <QMutex>
#include <QPainter>
#include <vlc/vlc.h>
#include <vector>
#include <memory>

#include "VLCPlayer.h"

namespace Flux {

class VLCVideoItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(Flux::VLCPlayer* player READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(bool hasVideoFrame READ hasVideoFrame NOTIFY hasVideoFrameChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)

public:
    explicit VLCVideoItem(QQuickItem *parent = nullptr);
    ~VLCVideoItem() override;

    void paint(QPainter *painter) override;

    VLCPlayer* player() const { return m_player; }
    void setPlayer(VLCPlayer *player);

    bool hasVideoFrame() const { return m_hasVideoFrame; }
    int videoWidth() const { return m_videoWidth; }
    int videoHeight() const { return m_videoHeight; }

signals:
    void playerChanged();
    void hasVideoFrameChanged();
    void videoSizeChanged();
    void frameReady();

public slots:
    void onFrameReady();

private:
    void setupVlcCallbacks();
    void cleanupVlcCallbacks();

    // libVLC video callbacks
    static void* lockCallback(void *opaque, void **planes);
    static void unlockCallback(void *opaque, void *picture, void *const *planes);
    static void displayCallback(void *opaque, void *picture);
    static unsigned formatCallback(void **opaque, char *chroma,
                                  unsigned *width, unsigned *height,
                                  unsigned *pitches, unsigned *lines);
    static void cleanupCallback(void *opaque);

    VLCPlayer *m_player = nullptr;

    mutable QMutex m_mutex;
    std::vector<uchar> m_pixelBuffer;
    QImage m_currentImage;
    bool m_hasVideoFrame = false;

    unsigned m_videoWidth = 0;
    unsigned m_videoHeight = 0;
    unsigned m_pitch = 0;
};

} // namespace Flux
