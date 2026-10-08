#pragma once

#include <QQuickItem>
#include <QImage>
#include <QMutex>
#include <vlc/vlc.h>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "VLCPlayer.h"

namespace Flux {

// Renders libVLC frames through the Qt Quick scene graph (GPU texture).
//
// Frame hand-off is triple-buffered:
//   decode  - libVLC writes the next frame here (no lock held while decoding)
//   ready   - most recent completed frame
//   display - owned by the GUI thread, copied into the texture
// Frame signals are coalesced, so a slow GUI thread skips to the newest frame
// instead of queueing a backlog (which is what causes stutter / freezes).
class VLCVideoItem : public QQuickItem {
    Q_OBJECT
    Q_PROPERTY(Flux::VLCPlayer* player READ player WRITE setPlayer NOTIFY playerChanged)
    Q_PROPERTY(bool hasVideoFrame READ hasVideoFrame NOTIFY hasVideoFrameChanged)
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)

public:
    explicit VLCVideoItem(QQuickItem *parent = nullptr);
    ~VLCVideoItem() override;

    VLCPlayer* player() const { return m_player; }
    void setPlayer(VLCPlayer *player);

    bool hasVideoFrame() const { return m_hasVideoFrame; }
    int videoWidth() const { return static_cast<int>(m_videoWidth); }
    int videoHeight() const { return static_cast<int>(m_videoHeight); }

signals:
    void playerChanged();
    void hasVideoFrameChanged();
    void videoSizeChanged();
    void frameReady();

public slots:
    void onFrameReady();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct Frame {
        std::vector<uchar> data;
        unsigned width = 0;
        unsigned height = 0;
        unsigned pitch = 0;
    };

    static std::shared_ptr<Frame> allocFrame(unsigned w, unsigned h);

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

    // Shared between the libVLC thread and the GUI thread (guarded by m_mutex)
    mutable QMutex m_mutex;
    std::shared_ptr<Frame> m_decodeFrame;
    std::shared_ptr<Frame> m_readyFrame;
    bool m_hasNewFrame = false;
    unsigned m_videoWidth = 0;
    unsigned m_videoHeight = 0;
    unsigned m_pitch = 0;

    // libVLC thread only
    std::shared_ptr<Frame> m_lockedFrame;

    // GUI thread only
    std::shared_ptr<Frame> m_displayFrame;
    QImage m_currentImage;
    bool m_textureDirty = false;
    bool m_hasVideoFrame = false;

    std::atomic<bool> m_framePending{false};
};

} // namespace Flux
