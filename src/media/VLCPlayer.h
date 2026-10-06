#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QTimer>
#include <vlc/vlc.h>

namespace Flux {

class VLCPlayer : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString url READ url WRITE setUrl NOTIFY urlChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY stateChanged)
    Q_PROPERTY(bool isPaused READ isPaused NOTIFY stateChanged)
    Q_PROPERTY(bool isBuffering READ isBuffering NOTIFY bufferingChanged)
    Q_PROPERTY(float bufferingPercent READ bufferingPercent NOTIFY bufferingChanged)

    Q_PROPERTY(qreal position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 timeMs READ timeMs NOTIFY timeChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY durationChanged)
    Q_PROPERTY(QString formattedTime READ formattedTime NOTIFY timeChanged)
    Q_PROPERTY(QString formattedDuration READ formattedDuration NOTIFY durationChanged)

    Q_PROPERTY(int volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY muteChanged)

    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)

    Q_PROPERTY(QVariantList audioTracks READ audioTracks NOTIFY audioTracksChanged)
    Q_PROPERTY(int selectedAudioTrack READ selectedAudioTrack WRITE selectAudioTrack NOTIFY selectedAudioTrackChanged)

    Q_PROPERTY(QVariantList subtitleTracks READ subtitleTracks NOTIFY subtitleTracksChanged)
    Q_PROPERTY(int selectedSubtitleTrack READ selectedSubtitleTrack WRITE selectSubtitleTrack NOTIFY selectedSubtitleTrackChanged)

    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorOccurred)

public:
    explicit VLCPlayer(QObject *parent = nullptr);
    ~VLCPlayer() override;

    libvlc_media_player_t* vlcMediaPlayer() const { return m_mediaPlayer; }

    QString url() const { return m_url; }
    void setUrl(const QString &url);

    QString state() const { return m_state; }
    bool isPlaying() const;
    bool isPaused() const;
    bool isBuffering() const { return m_isBuffering; }
    float bufferingPercent() const { return m_bufferingPercent; }

    qreal position() const;
    void setPosition(qreal pos);

    qint64 timeMs() const { return m_timeMs; }
    qint64 durationMs() const { return m_durationMs; }
    QString formattedTime() const;
    QString formattedDuration() const;

    int volume() const;
    void setVolume(int vol);

    bool isMuted() const;
    void setMuted(bool mute);

    int videoWidth() const { return m_videoWidth; }
    int videoHeight() const { return m_videoHeight; }
    void notifyVideoSize(int w, int h);

    QVariantList audioTracks() const { return m_audioTracks; }
    int selectedAudioTrack() const;

    QVariantList subtitleTracks() const { return m_subtitleTracks; }
    int selectedSubtitleTrack() const;

    QString errorMessage() const { return m_errorMessage; }

public slots:
    Q_INVOKABLE void play(const QString &mediaUrl = QString());
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(qreal position);
    Q_INVOKABLE void seekRelative(qint64 deltaMs);
    Q_INVOKABLE void selectAudioTrack(int trackId);
    Q_INVOKABLE void selectSubtitleTrack(int spuId);

signals:
    void urlChanged();
    void stateChanged();
    void bufferingChanged();
    void positionChanged();
    void timeChanged();
    void durationChanged();
    void volumeChanged();
    void muteChanged();
    void videoSizeChanged();
    void audioTracksChanged();
    void selectedAudioTrackChanged();
    void subtitleTracksChanged();
    void selectedSubtitleTrackChanged();
    void errorOccurred(const QString &message);
    void mediaPlayerRecreated();

private:
    void setupVlcEvents();
    void detachVlcEvents();
    void updateTracks();
    static void handleVlcEvent(const libvlc_event_t *event, void *opaque);
    static QString formatMilliseconds(qint64 ms);

    libvlc_media_player_t *m_mediaPlayer = nullptr;
    libvlc_media_t *m_currentMedia = nullptr;

    QString m_url;
    QString m_state = "Idle";
    bool m_isBuffering = false;
    float m_bufferingPercent = 0.0f;

    qreal m_position = 0.0;
    qint64 m_timeMs = 0;
    qint64 m_durationMs = 0;

    int m_videoWidth = 0;
    int m_videoHeight = 0;

    QVariantList m_audioTracks;
    QVariantList m_subtitleTracks;
    QString m_errorMessage;

    QTimer *m_pollTimer = nullptr;
};

} // namespace Flux
