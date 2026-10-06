#include "VLCVideoItem.h"
#include "VLCPlayer.h"
#include "../core/Logger.h"
#include <cstring>

namespace Flux {

VLCVideoItem::VLCVideoItem(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
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

    QMutexLocker locker(&m_mutex);
    m_pixelBuffer.clear();
    m_currentImage = QImage();
    m_hasVideoFrame = false;
    emit hasVideoFrameChanged();
}

unsigned VLCVideoItem::formatCallback(void **opaque, char *chroma,
                                     unsigned *width, unsigned *height,
                                     unsigned *pitches, unsigned *lines) {
    auto *item = static_cast<VLCVideoItem*>(*opaque);
    if (!item || *width == 0 || *height == 0) return 0;

    // Use RV32 (BGRA/RGBA 32-bit format native to Qt)
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
        item->m_pixelBuffer.resize(pitch * h);
        std::fill(item->m_pixelBuffer.begin(), item->m_pixelBuffer.end(), 0);
    }

    FLUX_LOG_INFO("VLCVideoItem", QString("Video format negotiated: %1x%2 (pitch: %3)").arg(w).arg(h).arg(pitch));

    QMetaObject::invokeMethod(item, [item, w, h]() {
        item->m_player->notifyVideoSize(static_cast<int>(w), static_cast<int>(h));
        emit item->videoSizeChanged();
    }, Qt::QueuedConnection);

    return 1;
}

void VLCVideoItem::cleanupCallback(void *opaque) {
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return;

    QMutexLocker locker(&item->m_mutex);
    item->m_pixelBuffer.clear();
    item->m_videoWidth = 0;
    item->m_videoHeight = 0;
}

void* VLCVideoItem::lockCallback(void *opaque, void **planes) {
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return nullptr;

    item->m_mutex.lock();
    planes[0] = item->m_pixelBuffer.data();
    return nullptr;
}

void VLCVideoItem::unlockCallback(void *opaque, void *picture, void *const *planes) {
    Q_UNUSED(picture);
    Q_UNUSED(planes);
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return;

    item->m_mutex.unlock();
}

void VLCVideoItem::displayCallback(void *opaque, void *picture) {
    Q_UNUSED(picture);
    auto *item = static_cast<VLCVideoItem*>(opaque);
    if (!item) return;

    emit item->frameReady();
}

void VLCVideoItem::onFrameReady() {
    {
        QMutexLocker locker(&m_mutex);
        if (m_videoWidth > 0 && m_videoHeight > 0 && !m_pixelBuffer.empty()) {
            // RV32 in VLC corresponds to Format_RGB32 / Format_ARGB32 in Qt on Windows
            m_currentImage = QImage(m_pixelBuffer.data(),
                                    static_cast<int>(m_videoWidth),
                                    static_cast<int>(m_videoHeight),
                                    static_cast<int>(m_pitch),
                                    QImage::Format_RGB32).copy();
            m_hasVideoFrame = true;
        }
    }

    emit hasVideoFrameChanged();
    update();
}

void VLCVideoItem::paint(QPainter *painter) {
    if (!painter) return;

    QImage img;
    {
        QMutexLocker locker(&m_mutex);
        img = m_currentImage;
    }

    if (img.isNull()) {
        // Draw dark neutral background when no video is playing
        painter->fillRect(boundingRect(), QColor("#080c14"));
        return;
    }

    // Scale while preserving aspect ratio and center within item
    QRectF target = boundingRect();
    QSizeF imageSize = img.size();
    QSizeF scaledSize = imageSize.scaled(target.size(), Qt::KeepAspectRatio);

    qreal x = (target.width() - scaledSize.width()) / 2.0;
    qreal y = (target.height() - scaledSize.height()) / 2.0;
    QRectF destRect(x, y, scaledSize.width(), scaledSize.height());

    painter->fillRect(target, QColor("#000000"));
    painter->drawImage(destRect, img);
}

} // namespace Flux
