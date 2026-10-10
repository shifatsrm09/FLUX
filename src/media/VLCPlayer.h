#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QTimer>
#include <QElapsedTimer>
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

    // Learned / configured language preferences (used to auto-pick tracks)
    Q_PROPERTY(QStringList preferredAudio READ preferredAudio NOTIFY languagePreferencesChanged)
    Q_PROPERTY(QString preferredSubtitle READ preferredSubtitle NOTIFY languagePreferencesChanged)

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
    Q_INVOKABLE void refreshTracks();
    Q_INVOKABLE void playFrom(const QString &url, qint64 startMs);
    Q_INVOKABLE void setLanguagePreferences(const QStringList &audio, const QString &subtitle);

    // Teleparty: apply another member's action. These never emit userToggledPlay /
    // userSeeked, so an applied action is not broadcast back to the session.
    Q_INVOKABLE void remotePause();
    Q_INVOKABLE void remoteResume();
    Q_INVOKABLE void remoteSeekTo(qint64 timeMs);
    Q_INVOKABLE void remoteRestart(qint64 startMs);

    QStringList preferredAudio() const { return m_preferredAudio; }
    QString preferredSubtitle() const { return m_preferredSubtitle; }

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
    void languagePreferencesChanged();

    // Fired only for actions the local user performed (not for remote / Teleparty ones)
    void userToggledPlay(bool playing, qint64 timeMs);
    void userSeeked(qint64 timeMs);

private:
    void setupVlcEvents();
    void detachVlcEvents();
    void updateTracks();
    void applyPendingSeek();
    bool seekGuardActive() const;
    void applyMute();
    void beginSeekMute(qint64 targetMs);
    void endSeekMute();
    void applyLanguagePreferences();
    void learnAudioChoice(int trackId);
    void learnSubtitleChoice(int spuId);
    static QString languageKey(const QString &trackName);
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

    // Seek coalescing: rapid seeks (slider drag, repeated key presses) are merged
    // into one libVLC call every few milliseconds instead of hammering the stream.
    QTimer *m_seekTimer = nullptr;
    bool m_hasPendingSeek = false;
    bool m_pendingIsTime = false;
    qint64 m_pendingTimeMs = 0;
    qreal m_pendingPos = 0.0;
    QElapsedTimer m_uptime;
    qint64 m_seekGuardUntil = 0;   // ignore stale libVLC position until this uptime (ms)

    // Backing values (libVLC cannot always be queried before audio output exists)
    int m_volume = 100;            // 0..200 (%)
    bool m_muted = false;

    // Seek mute: silence the (stale) audio the instant a seek starts and restore it once
    // playback has landed near the target. Independent of the user's own mute setting.
    QTimer *m_seekMuteTimer = nullptr;   // failsafe so we can never stay muted
    bool m_seekMuted = false;
    qint64 m_seekMuteTargetMs = -1;      // -1 = unknown target

    // Resume support: one-shot start offset consumed by the next play()
    qint64 m_startTimeMs = 0;

    // Teleparty: >0 while a remote member's action is being applied (nothing is broadcast)
    int m_quiet = 0;
    bool m_pendingNotify = false;   // the queued seek came from the local user

    // Language auto-selection
    QStringList m_preferredAudio;
    QString m_preferredSubtitle;       // "" = leave default, "off" = disable, else language
    bool m_audioPrefApplied = false;
    bool m_subPrefApplied = false;
    int m_autoTrackAttempts = 0;
};

} // namespace Flux
