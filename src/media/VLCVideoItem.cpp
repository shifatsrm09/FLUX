#include "VLCVideoItem.h"
#include "VLCPlayer.h"
#include "../core/Logger.h"
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTexture>
#include <cstring>

namespace Flux {

std::shared_ptr<VLCVideoItem::Frame> VLCVideoItem::allocFrame(unsigned w, unsigned h) {
    auto f = std::make_shared<Frame>();
    f->width = w;
    f->height = h;
    f->pitch = w * 4;
    f->data.assign(static_cast<size_t>(f->pitch) * h, 0);
    return f;
}

VLCVideoItem::VLCVideoItem(QQuickItem *parent)
    : QQuickItem(parent) {
    setFlag(ItemHasContents, true);
    connect(this, &VLCVideoItem::frameReady, this, &VLCVideoItem::onFrameReady, Qt::QueuedConnection);
}

VLCVideoItem::~VLCVideoItem() {
    cleanupVlcCallbacks();
}

void VLCVideoItem::setPlayer(VLCPlayer *player) {
    if (m_player == player) return;

    if (m_player) {
        cleanupVlcCallbacks();
        disconnect(m_player, nullptr, this, nullptr);
    }

    m_player = player;

    if (m_player) {
        setupVlcCallbacks();
        connect(m_player, &VLCPlayer::mediaPlayerRecreated, this, &VLCVideoItem::setupVlcCallbacks);
    }

    emit playerChanged();
}

void VLCVideoItem::setupVlcCallbacks() {
    if (!m_player) return;

    libvlc_media_player_t *mp = m_player->vlcMediaPlayer();
    if (!mp) return;

    FLUX_LOG_INFO("VLCVideoItem", "Registering libVLC video format and render callbacks...");
    libvlc_video_set_format_callbacks(mp, formatCallback, cleanupCallback);
    libvlc_video_set_callbacks(mp, lockCallback, unlockCallback, displayCallback, this);
}

void VLCVideoItem::cleanupVlcCallbacks() {
    if (!m_player) return;

    libvlc_media_player_t *mp = m_player->vlcMediaPlayer();
    if (mp) {
        libvlc_video_set_callbacks(mp, nullptr, nullptr, nullptr, nullptr);
        libvlc_video_set_format_callbacks(mp, nullptr, nullptr);
    }

    // NOTE: the decode/ready buffers are intentionally left allocated. A video
    // output that is still shutting down may call lock() once more, and it must
    // always receive a valid plane pointer.
    {
        QMutexLocker locker(&m_mutex);
        m_hasNewFrame = false;
    }

    m_displayFrame.reset();
    m_currentImage = QImage();
    m_textureDirty = false;

    if (m_hasVideoFrame) {
        m_hasVideoFrame = false;
        emit hasVideoFrameChanged();
    }
    update();
}

unsigned VLCVideoItem::formatCallback(void **opaque, char *chroma,
                                     unsigned *width, unsigned *height,
                                     unsigned *pitches, unsigned *lines) {
    auto *item = static_cast<VLCVideoItem*>(*opaque);
    if (!item || *width == 0 || *height == 0) return 0;

    // RV32 = 32-bit BGRA, native to Qt's Format_RGB32 on little-endian machines
    memcpy(chroma, "RV32", 4);

    unsigned w = *width;
    unsigned h = *height;
    unsigned pitch = w * 4;

    *pitches = pitch;
    *lines = h;

    {
        QMutexLocker locker(&item->m_mutex);
        item->m_videoWidth = w;
        item->m_videoHeight = h;
        item->m_pitch = pitch;
        item->m_decodeFrame = allocFrame(w, h);
        item->m_readyFrame = allocFrame(w, h);
        item->m_hasNewFrame = false;
    }

    FLUX_LOG_INFO("VLCVideoItem", QString("Video format negotiated: %1x%2 (pitch: %3)").arg(w).arg(h).arg(pitch));

    QMetaObject::invokeMethod(item, [item, w, h]() {
        if (item->m_player) {
            item->m_player->notifyVideoSize(static_cast<int>(w), static_cast<int>(h));
        }
        emit item->videoSizeChanged();
    }, Qt::QueuedConnection);

    return 1;
}

void VLCVideoItem::cleanupCallback(void *opaque) {
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return;

    QMutexLocker locker(&item->m_mutex);
    item->m_hasNewFrame = false;
}

void* VLCVideoItem::lockCallback(void *opaque, void **planes) {
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return nullptr;

    {
        QMutexLocker locker(&item->m_mutex);
        // Re-allocate if a size change left us with a stale-sized buffer
        if (!item->m_decodeFrame
            || item->m_decodeFrame->width != item->m_videoWidth
            || item->m_decodeFrame->height != item->m_videoHeight) {
            item->m_decodeFrame = allocFrame(item->m_videoWidth, item->m_videoHeight);
        }
        item->m_lockedFrame = item->m_decodeFrame;
    }

    // Decoding happens here WITHOUT holding any lock.
    planes[0] = item->m_lockedFrame->data.data();
    return nullptr;
}

void VLCVideoItem::unlockCallback(void *opaque, void *picture, void *const *planes) {
    Q_UNUSED(picture);
    Q_UNUSED(planes);
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return;

    item->m_lockedFrame.reset();
}

void VLCVideoItem::displayCallback(void *opaque, void *picture) {
    Q_UNUSED(picture);
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return;

    {
        // Publish the finished frame: a pointer swap, effectively instant.
        QMutexLocker locker(&item->m_mutex);
        if (!item->m_decodeFrame || !item->m_readyFrame) return;
        std::swap(item->m_decodeFrame, item->m_readyFrame);
        item->m_hasNewFrame = true;
    }

    // Coalesce: only one wake-up may be in flight at a time.
    if (!item->m_framePending.exchange(true)) {
        emit item->frameReady();
    }
}

void VLCVideoItem::onFrameReady() {
    m_framePending.store(false);

    {
        QMutexLocker locker(&m_mutex);
        if (!m_hasNewFrame || !m_readyFrame) return;

        std::swap(m_displayFrame, m_readyFrame);
        m_hasNewFrame = false;

        // The slot we just took over from the display side may be empty on the
        // very first frame; libVLC needs a valid buffer there.
        if (!m_readyFrame && m_displayFrame) {
            m_readyFrame = allocFrame(m_displayFrame->width, m_displayFrame->height);
        }
    }

    if (!m_displayFrame) return;
    const Frame &f = *m_displayFrame;
    if (f.width == 0 || f.height == 0 || f.data.empty()) return;

    // The display buffer is exclusively ours now, so this copy needs no lock.
    // The copy is required because the GPU upload happens later on the render
    // thread, by which time this buffer may have been recycled.
    m_currentImage = QImage(f.data.data(),
                            static_cast<int>(f.width),
                            static_cast<int>(f.height),
                            static_cast<int>(f.pitch),
                            QImage::Format_RGB32).copy();
    m_textureDirty = true;

    if (!m_hasVideoFrame) {
        m_hasVideoFrame = true;
        emit hasVideoFrameChanged();
    }

    update();
}

void VLCVideoItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) {
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    update();
}

QSGNode *VLCVideoItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *node = static_cast<QSGSimpleTextureNode *>(oldNode);

    if (m_currentImage.isNull() || !window()) {
        delete node;
        return nullptr;
    }

    if (!node) {
        node = new QSGSimpleTextureNode();
        node->setFiltering(QSGTexture::Linear);
        node->setOwnsTexture(true);
        m_textureDirty = true;
    }

    if (m_textureDirty || !node->texture()) {
        QSGTexture *tex = window()->createTextureFromImage(m_currentImage, QQuickWindow::TextureIsOpaque);
        if (tex) {
            QSGTexture *old = node->texture();
            node->setOwnsTexture(false);
            node->setTexture(tex);
            node->setOwnsTexture(true);
            delete old;
        }
        m_textureDirty = false;
    }

    // Letterbox: keep aspect ratio and centre inside the item (GPU does the scaling)
    const QSizeF target = size();
    const QSizeF scaled = QSizeF(m_currentImage.size()).scaled(target, Qt::KeepAspectRatio);
    const qreal x = (target.width() - scaled.width()) / 2.0;
    const qreal y = (target.height() - scaled.height()) / 2.0;
    node->setRect(QRectF(x, y, scaled.width(), scaled.height()));

    return node;
}

} // namespace Flux
