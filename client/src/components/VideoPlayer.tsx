import { useRef, useState, useEffect } from 'react';
import Hls from 'hls.js';
import type { MediaMetadata, HlsSessionResponse } from '../types';
import './VideoPlayer.css';

interface VideoPlayerProps {
  url: string;
  metadata: MediaMetadata;
}

type PlaybackStatus =
  | 'idle'
  | 'preparing'
  | 'loading_playlist'
  | 'buffering'
  | 'playing'
  | 'seeking'
  | 'paused'
  | 'error'
  | 'session_expired';

export function VideoPlayer({ url, metadata }: VideoPlayerProps) {
  const videoRef = useRef<HTMLVideoElement>(null);
  const [playbackStatus, setPlaybackStatus] = useState<PlaybackStatus>('idle');
  const [errorTitle, setErrorTitle] = useState<string | null>(null);
  const [errorDetail, setErrorDetail] = useState<string | null>(null);
  const [activeSessionId, setActiveSessionId] = useState<string | null>(null);
  const [totalDuration, setTotalDuration] = useState<number | null>(metadata.durationSeconds);

  const isDirectPlay = metadata.directPlayCandidate;
  const videoCodec = (metadata.video?.codec || '').toLowerCase();
  const isHevc = videoCodec.includes('hevc') || videoCodec.includes('h265');
  const videoProfile = metadata.video?.profile || '';
  const audioCodec = metadata.audio?.codec?.toUpperCase() || 'AAC';
  const audioChannels = metadata.audio?.channelLayout || (metadata.audio?.channels ? `${metadata.audio.channels}ch` : '');

  // Setup player when url or metadata changes
  useEffect(() => {
    let isCancelled = false;
    let createdSessionId: string | null = null;
    let hlsInstance: Hls | null = null;

    setPlaybackStatus('idle');
    setErrorTitle(null);
    setErrorDetail(null);
    setActiveSessionId(null);
    setTotalDuration(metadata.durationSeconds);

    const video = videoRef.current;
    if (video) {
      video.pause();
      video.removeAttribute('src');
      video.load();
    }

    if (isDirectPlay) {
      // Direct Play path (native MP4 / WebM)
      if (video) {
        video.src = url;
        video.load();
      }
      return () => {
        isCancelled = true;
      };
    }

    // HLS VOD path: create session and load HLS playlist
    async function initHlsSession() {
      setPlaybackStatus('preparing');

      try {
        const res = await fetch('/api/hls/session', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ url }),
        });

        if (isCancelled) return;

        if (!res.ok) {
          const errData = await res.json().catch(() => ({}));
          throw new Error(errData.detail || errData.error || `HTTP ${res.status}`);
        }

        const sessionData: HlsSessionResponse = await res.json();
        if (isCancelled) {
          // Clean up if cancelled while request was in-flight
          fetch(`/api/hls/${sessionData.sessionId}`, { method: 'DELETE', keepalive: true }).catch(() => {});
          return;
        }

        createdSessionId = sessionData.sessionId;
        setActiveSessionId(sessionData.sessionId);
        if (sessionData.duration) {
          setTotalDuration(sessionData.duration);
        }

        setPlaybackStatus('loading_playlist');

        const playlistUrl = sessionData.playlistUrl;

        // Check if hls.js is supported (most modern browsers: Chrome, Firefox, Edge)
        if (Hls.isSupported()) {
          const hls = new Hls({
            enableWorker: true,
            lowLatencyMode: false,
            backBufferLength: 60,
            maxBufferLength: 30,
            maxMaxBufferLength: 60,
            fragLoadingTimeOut: 25000,
            manifestLoadingTimeOut: 15000,
            maxBufferHole: 0.5,
            highBufferWatchdogPeriod: 2,
            nudgeOffset: 0.1,
            nudgeMaxRetry: 5,
            maxFragLookUpTolerance: 0.25,
          });
          hlsInstance = hls;

          hls.loadSource(playlistUrl);
          if (videoRef.current) {
            hls.attachMedia(videoRef.current);
          }

          hls.on(Hls.Events.MANIFEST_PARSED, () => {
            console.log('[HLS] Manifest parsed successfully');
            if (!isCancelled) {
              setPlaybackStatus('idle');
            }
          });

          hls.on(Hls.Events.FRAG_LOADING, (_event, data) => {
            console.log('[HLS] Requesting segment:', data.frag.relurl);
          });

          hls.on(Hls.Events.FRAG_LOADED, (_event, data) => {
            const stats = (data as unknown as { stats?: { total?: number; loading?: { start: number; end: number } } }).stats;
            const kb = stats?.total ? (stats.total / 1024).toFixed(1) : '?';
            const ms = stats?.loading ? Math.round(stats.loading.end - stats.loading.start) : '?';
            console.log(`[HLS] Received segment: ${data.frag.relurl} (${kb} KB in ${ms}ms)`);
          });

          hls.on(Hls.Events.FRAG_BUFFERED, (_event, data) => {
            console.log('[HLS] Buffered segment:', data.frag.relurl);
          });

          hls.on(Hls.Events.ERROR, (_event, data) => {
            if (isCancelled) return;

            console.warn('[HLS Error]', data.type, data.details, 'fatal:', data.fatal);

            if (data.fatal) {
              switch (data.type) {
                case Hls.ErrorTypes.NETWORK_ERROR:
                  if (data.response?.code === 404) {
                    setPlaybackStatus('session_expired');
                    setErrorTitle('Session Expired');
                    setErrorDetail('The streaming session has expired or was removed. Please click Analyze to start a new session.');
                  } else {
                    console.log('[HLS] Fatal network error, attempting reload...');
                    hls.startLoad();
                  }
                  break;
                case Hls.ErrorTypes.MEDIA_ERROR:
                  if (isHevc) {
                    setPlaybackStatus('error');
                    setErrorTitle('Unsupported Codec: HEVC / H.265');
                    setErrorDetail(
                      'Your browser or GPU does not support decoding HEVC (H.265) video in HLS. The HLS delivery and seeking architecture is fully active, but playback requires native HEVC hardware/codec support. Server-side transcoding (HEVC → H.264) will be required for browsers lacking native HEVC decoding.'
                    );
                  } else {
                    console.log('[HLS] Fatal media error, attempting recovery...');
                    hls.recoverMediaError();
                  }
                  break;
                default:
                  setPlaybackStatus('error');
                  setErrorTitle('HLS Playback Error');
                  setErrorDetail(data.details || 'A fatal streaming error occurred.');
                  hls.destroy();
                  break;
              }
            } else if (data.details === Hls.ErrorDetails.BUFFER_STALLED_ERROR) {
              console.log('[HLS] Playback stall detected; hls.js nudge/watchdog will advance playhead');
            }
          });
        } else if (video && video.canPlayType('application/vnd.apple.mpegurl')) {
          // Native HLS support (Safari on macOS/iOS)
          video.src = playlistUrl;
        } else {
          setPlaybackStatus('error');
          setErrorTitle('HLS Not Supported');
          setErrorDetail('Your browser does not support HLS media playback.');
        }
      } catch (err: unknown) {
        if (!isCancelled) {
          setPlaybackStatus('error');
          setErrorTitle('Stream Initialization Failed');
          setErrorDetail(err instanceof Error ? err.message : 'Unknown streaming error');
        }
      }
    }

    initHlsSession();

    return () => {
      isCancelled = true;
      if (hlsInstance) {
        hlsInstance.destroy();
      }
      if (createdSessionId) {
        fetch(`/api/hls/${createdSessionId}`, { method: 'DELETE', keepalive: true }).catch(() => {});
      }
    };
  }, [url, metadata, isDirectPlay, isHevc]);

  const handleNativeVideoError = () => {
    const video = videoRef.current;
    const mediaError = video?.error;
    if (!mediaError) return;

    setPlaybackStatus('error');

    if (mediaError.code === MediaError.MEDIA_ERR_DECODE || mediaError.code === MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED) {
      if (!isDirectPlay && isHevc) {
        setErrorTitle('Codec Decode Error: HEVC Main 10');
        setErrorDetail(
          'Your browser cannot decode HEVC (H.265) video in HLS. The HLS delivery and seeking architecture is active and functioning, but the underlying video stream is HEVC Main 10. Most desktop browsers require H.264 video. Transcoding (HEVC → H.264) will be required for full compatibility.'
        );
        return;
      }
    }

    setErrorTitle('Media Playback Error');
    setErrorDetail(`Video error code: ${mediaError.code} (${mediaError.message || 'Format or decode issue'})`);
  };

  return (
    <section className="video-player">
      <div className="player-header">
        <h2 className="panel-title">Video Player</h2>

        <div className="player-badges">
          <span className={`badge ${isDirectPlay ? 'badge-direct' : 'badge-hls'}`}>
            {isDirectPlay ? 'Direct Play' : '⚡ On-Demand HLS'}
          </span>

          <span className={`status-pill status-${playbackStatus}`}>
            {(playbackStatus === 'preparing' || playbackStatus === 'loading_playlist' || playbackStatus === 'buffering') && (
              <span className="pill-spinner" />
            )}
            {playbackStatus === 'playing' && <span className="pill-dot active" />}
            {playbackStatus === 'seeking' && <span className="pill-dot seeking" />}
            {playbackStatus === 'paused' && <span className="pill-dot paused" />}
            {(playbackStatus === 'error' || playbackStatus === 'session_expired') && <span className="pill-dot error" />}

            {playbackStatus === 'preparing' && 'Preparing stream...'}
            {playbackStatus === 'loading_playlist' && 'Loading playlist...'}
            {playbackStatus === 'buffering' && 'Buffering...'}
            {playbackStatus === 'seeking' && 'Seeking...'}
            {playbackStatus === 'playing' && 'Playing'}
            {playbackStatus === 'paused' && 'Paused'}
            {playbackStatus === 'idle' && 'Ready to Play'}
            {playbackStatus === 'error' && 'Playback Error'}
            {playbackStatus === 'session_expired' && 'Session Expired'}
          </span>
        </div>
      </div>

      {!isDirectPlay && (
        <div className="hls-notice">
          <div className="hls-notice-header">
            <span className="notice-icon">⚡</span>
            <strong>HLS VOD Stream Active</strong>
            {activeSessionId && <span className="session-tag">Session: {activeSessionId.slice(0, 8)}...</span>}
          </div>

          <div className="hls-tech-specs">
            <div className="spec-item">
              <span className="spec-label">Video:</span>
              <span className="spec-val">
                {videoCodec ? videoCodec.toUpperCase() : 'UNKNOWN'} {videoProfile ? `(${videoProfile})` : ''}
              </span>
            </div>
            <div className="spec-item">
              <span className="spec-label">Audio:</span>
              <span className="spec-val">
                {audioCodec} {audioChannels ? `(${audioChannels})` : ''}
              </span>
            </div>
            <div className="spec-item">
              <span className="spec-label">Stream Copy:</span>
              <span className="spec-val spec-green">Enabled (no re-encoding)</span>
            </div>
            <div className="spec-item">
              <span className="spec-label">Transcoding:</span>
              <span className="spec-val spec-dim">Disabled</span>
            </div>
            {totalDuration && (
              <div className="spec-item">
                <span className="spec-label">VOD Duration:</span>
                <span className="spec-val">
                  {Math.floor(totalDuration / 60)}m {Math.floor(totalDuration % 60)}s
                </span>
              </div>
            )}
          </div>

          {isHevc && (
            <div className="codec-warning">
              <strong>Important Codec Note:</strong> Source video is encoded as{' '}
              <strong>HEVC / H.265 Main 10</strong>. HLS provides delivery, independent segment addressing, and arbitrary seeking, but does NOT re-encode the video. If your browser lacks native HEVC hardware decoding, playback will fail with a decode error below.
            </div>
          )}
        </div>
      )}

      <div className="video-container">
        <video
          ref={videoRef}
          className="video-element"
          controls
          playsInline
          preload="auto"
          onWaiting={() => {
            if (playbackStatus !== 'error') setPlaybackStatus('buffering');
          }}
          onSeeking={() => {
            if (playbackStatus !== 'error') setPlaybackStatus('seeking');
          }}
          onSeeked={() => {
            if (playbackStatus !== 'error') {
              setPlaybackStatus(videoRef.current?.paused ? 'paused' : 'playing');
            }
          }}
          onPlay={() => setPlaybackStatus('playing')}
          onPlaying={() => setPlaybackStatus('playing')}
          onPause={() => {
            if (playbackStatus !== 'error' && playbackStatus !== 'seeking') {
              setPlaybackStatus('paused');
            }
          }}
          onError={handleNativeVideoError}
        >
          Your browser does not support HTML5 video.
        </video>
      </div>

      {errorTitle && (
        <div className="playback-error">
          <div className="playback-error-header">
            <span className="error-icon">✕</span>
            <strong>{errorTitle}</strong>
          </div>
          {errorDetail && <p className="playback-error-detail">{errorDetail}</p>}
        </div>
      )}
    </section>
  );
}
