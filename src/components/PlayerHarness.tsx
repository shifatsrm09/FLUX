import { useRef, useEffect, useState } from 'react';
import type { PlaybackMetrics, MediaEventLog, PlaybackStage } from '../types/diagnostics';

interface Props {
  url: string;
  onEvent: (event: MediaEventLog) => void;
  onMetricsUpdate: (metrics: PlaybackMetrics) => void;
}

export function PlayerHarness({ url, onEvent, onMetricsUpdate }: Props) {
  const videoRef = useRef<HTMLVideoElement>(null);
  const audioContextRef = useRef<AudioContext | null>(null);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const rafRef = useRef<number | null>(null);

  const [metrics, setMetrics] = useState<PlaybackMetrics>({
    stage: 'idle',
    timeToMetadataMs: null,
    timeToFirstFrameMs: null,
    timeToPlayingMs: null,
    seekLatencyMs: null,
    videoWidth: null,
    videoHeight: null,
    duration: null,
    currentTime: 0,
    bufferedEnd: null,
    hasAudioActivity: false,
    audioDecibelLevel: -100,
    nativeError: null,
  });

  const loadStartRef = useRef<number>(0);
  const seekStartRef = useRef<number>(0);

  // Initialize playback test whenever URL changes
  useEffect(() => {
    const video = videoRef.current;
    if (!video || !url) return;

    loadStartRef.current = performance.now();

    const initialMetrics: PlaybackMetrics = {
      stage: 'loadstart',
      timeToMetadataMs: null,
      timeToFirstFrameMs: null,
      timeToPlayingMs: null,
      seekLatencyMs: null,
      videoWidth: null,
      videoHeight: null,
      duration: null,
      currentTime: 0,
      bufferedEnd: null,
      hasAudioActivity: false,
      audioDecibelLevel: -100,
      nativeError: null,
    };
    setMetrics(initialMetrics);
    onMetricsUpdate(initialMetrics);

    onEvent({
      timestamp: Date.now(),
      eventName: 'loadstart',
      stage: 'loadstart',
      details: { url },
    });

    video.src = url;
    video.load();

    const handleLoadedMetadata = () => {
      const elapsed = Math.round(performance.now() - loadStartRef.current);
      setMetrics((prev) => {
        const next = {
          ...prev,
          stage: 'metadata_loaded' as PlaybackStage,
          timeToMetadataMs: elapsed,
          videoWidth: video.videoWidth,
          videoHeight: video.videoHeight,
          duration: video.duration,
        };
        onMetricsUpdate(next);
        return next;
      });

      onEvent({
        timestamp: Date.now(),
        eventName: 'loadedmetadata',
        stage: 'metadata_loaded',
        details: {
          videoWidth: video.videoWidth,
          videoHeight: video.videoHeight,
          duration: video.duration,
          elapsedMs: elapsed,
        },
      });
    };

    const handleLoadedData = () => {
      const elapsed = Math.round(performance.now() - loadStartRef.current);
      setMetrics((prev) => {
        const next = {
          ...prev,
          stage: 'first_frame_ready' as PlaybackStage,
          timeToFirstFrameMs: elapsed,
        };
        onMetricsUpdate(next);
        return next;
      });

      onEvent({
        timestamp: Date.now(),
        eventName: 'loadeddata',
        stage: 'first_frame_ready',
        details: { elapsedMs: elapsed },
      });
    };

    const handleCanPlay = () => {
      onEvent({
        timestamp: Date.now(),
        eventName: 'canplay',
        stage: 'canplay',
      });
    };

    const handlePlaying = () => {
      const elapsed = Math.round(performance.now() - loadStartRef.current);
      setMetrics((prev) => {
        const next = {
          ...prev,
          stage: 'playing' as PlaybackStage,
          timeToPlayingMs: prev.timeToPlayingMs ?? elapsed,
        };
        onMetricsUpdate(next);
        return next;
      });

      onEvent({
        timestamp: Date.now(),
        eventName: 'playing',
        stage: 'playing',
        details: { elapsedMs: elapsed, currentTime: video.currentTime },
      });

      // Try setting up Web Audio to check for audio waveform energy
      setupWebAudio();
    };

    const handleWaiting = () => {
      setMetrics((prev) => ({ ...prev, stage: 'stalled' }));
      onEvent({
        timestamp: Date.now(),
        eventName: 'waiting',
        stage: 'stalled',
        details: { currentTime: video.currentTime },
      });
    };

    const handleSeeking = () => {
      seekStartRef.current = performance.now();
      setMetrics((prev) => ({ ...prev, stage: 'seeking' }));
      onEvent({
        timestamp: Date.now(),
        eventName: 'seeking',
        stage: 'seeking',
        details: { toTime: video.currentTime },
      });
    };

    const handleSeeked = () => {
      const seekDuration = seekStartRef.current > 0 ? Math.round(performance.now() - seekStartRef.current) : 0;
      setMetrics((prev) => {
        const next = {
          ...prev,
          stage: 'seeked' as PlaybackStage,
          seekLatencyMs: seekDuration,
          currentTime: video.currentTime,
        };
        onMetricsUpdate(next);
        return next;
      });

      onEvent({
        timestamp: Date.now(),
        eventName: 'seeked',
        stage: 'seeked',
        details: { currentTime: video.currentTime, latencyMs: seekDuration },
      });
    };

    const handleError = () => {
      const err = video.error;
      const errorMap: Record<number, string> = {
        1: 'MEDIA_ERR_ABORTED',
        2: 'MEDIA_ERR_NETWORK',
        3: 'MEDIA_ERR_DECODE (Decoder failed / unsupported codec)',
        4: 'MEDIA_ERR_SRC_NOT_SUPPORTED (Container/format rejected by browser)',
      };

      const nativeError = err
        ? {
            code: err.code,
            name: errorMap[err.code] || 'UNKNOWN_ERROR',
            message: err.message || errorMap[err.code] || 'Media playback error',
          }
        : {
            code: -1,
            name: 'UNKNOWN',
            message: 'An unknown media playback error occurred',
          };

      setMetrics((prev) => {
        const next = {
          ...prev,
          stage: 'error' as PlaybackStage,
          nativeError,
        };
        onMetricsUpdate(next);
        return next;
      });

      onEvent({
        timestamp: Date.now(),
        eventName: 'error',
        stage: 'error',
        details: { nativeError },
      });
    };

    const handleTimeUpdate = () => {
      let bufEnd: number | null = null;
      if (video.buffered.length > 0) {
        bufEnd = video.buffered.end(video.buffered.length - 1);
      }
      setMetrics((prev) => ({
        ...prev,
        currentTime: video.currentTime,
        bufferedEnd: bufEnd,
      }));
    };

    video.addEventListener('loadedmetadata', handleLoadedMetadata);
    video.addEventListener('loadeddata', handleLoadedData);
    video.addEventListener('canplay', handleCanPlay);
    video.addEventListener('playing', handlePlaying);
    video.addEventListener('waiting', handleWaiting);
    video.addEventListener('seeking', handleSeeking);
    video.addEventListener('seeked', handleSeeked);
    video.addEventListener('error', handleError);
    video.addEventListener('timeupdate', handleTimeUpdate);

    return () => {
      video.removeEventListener('loadedmetadata', handleLoadedMetadata);
      video.removeEventListener('loadeddata', handleLoadedData);
      video.removeEventListener('canplay', handleCanPlay);
      video.removeEventListener('playing', handlePlaying);
      video.removeEventListener('waiting', handleWaiting);
      video.removeEventListener('seeking', handleSeeking);
      video.removeEventListener('seeked', handleSeeked);
      video.removeEventListener('error', handleError);
      video.removeEventListener('timeupdate', handleTimeUpdate);

      if (rafRef.current) cancelAnimationFrame(rafRef.current);
      if (audioContextRef.current && audioContextRef.current.state !== 'closed') {
        audioContextRef.current.close().catch(() => {});
      }
    };
  }, [url]);

  const setupWebAudio = () => {
    const video = videoRef.current;
    if (!video || audioContextRef.current) return;

    try {
      const AudioCtx = window.AudioContext || (window as unknown as { webkitAudioContext: typeof AudioContext }).webkitAudioContext;
      if (!AudioCtx) return;

      const ctx = new AudioCtx();
      audioContextRef.current = ctx;

      const source = ctx.createMediaElementSource(video);
      const analyser = ctx.createAnalyser();
      analyser.fftSize = 256;
      source.connect(analyser);
      analyser.connect(ctx.destination);
      analyserRef.current = analyser;

      const buffer = new Uint8Array(analyser.frequencyBinCount);

      const checkAudio = () => {
        if (!analyserRef.current) return;
        analyserRef.current.getByteFrequencyData(buffer);

        let sum = 0;
        for (let i = 0; i < buffer.length; i++) {
          sum += buffer[i];
        }
        const avg = sum / buffer.length;

        if (avg > 2) {
          setMetrics((prev) => ({
            ...prev,
            hasAudioActivity: true,
            audioDecibelLevel: Math.round(avg),
          }));
        }

        rafRef.current = requestAnimationFrame(checkAudio);
      };

      checkAudio();
    } catch {
      // Note: createMediaElementSource can be restricted if origin lacks CORS headers
    }
  };

  const handleSeekTest = (seconds: number) => {
    const video = videoRef.current;
    if (!video) return;
    video.currentTime = Math.min(seconds, video.duration || seconds);
  };

  const handleSeekRelative = (delta: number) => {
    const video = videoRef.current;
    if (!video) return;
    video.currentTime = Math.max(0, video.currentTime + delta);
  };

  const stageBadgeClass = (s: PlaybackStage) => {
    switch (s) {
      case 'playing': return 'pill pill-success';
      case 'first_frame_ready':
      case 'metadata_loaded':
      case 'canplay':
      case 'seeked': return 'pill pill-green';
      case 'seeking':
      case 'loadstart': return 'pill pill-yellow';
      case 'stalled': return 'pill pill-warning';
      case 'error': return 'pill pill-error';
      default: return 'pill pill-dim';
    }
  };

  return (
    <div className="card player-card">
      <div className="card-header">
        <div className="card-title-group">
          <span className="card-icon">🎬</span>
          <h2 className="card-title">Native Playback & Seek Harness</h2>
        </div>
        <div className="header-badges">
          <span className={stageBadgeClass(metrics.stage)}>
            {metrics.stage.toUpperCase().replace(/_/g, ' ')}
          </span>
        </div>
      </div>

      <div className="card-body">
        {/* Native Video Element */}
        <div className="video-viewport">
          <video
            ref={videoRef}
            className="test-video"
            controls
            playsInline
            preload="auto"
          />
        </div>

        {/* Playback Controls & Seek Tests */}
        <div className="playback-actions">
          <span className="action-title">Seek Latency Tests:</span>
          <button type="button" className="btn btn-sm" onClick={() => handleSeekRelative(-10)}>
            ⏪ -10s
          </button>
          <button type="button" className="btn btn-sm" onClick={() => handleSeekRelative(30)}>
            ⏩ +30s
          </button>
          <button type="button" className="btn btn-sm" onClick={() => handleSeekTest(60)}>
            Seek to 1:00
          </button>
          <button type="button" className="btn btn-sm" onClick={() => handleSeekTest(300)}>
            Seek to 5:00
          </button>
          <button type="button" className="btn btn-sm" onClick={() => handleSeekTest(1800)}>
            Seek to 30:00
          </button>
        </div>

        {/* Live Metrics Grid */}
        <div className="metrics-grid">
          <div className="metric-box">
            <span className="m-label">Time to Metadata</span>
            <span className="m-value">
              {metrics.timeToMetadataMs !== null ? `${metrics.timeToMetadataMs} ms` : '—'}
            </span>
          </div>

          <div className="metric-box">
            <span className="m-label">Time to First Frame</span>
            <span className="m-value">
              {metrics.timeToFirstFrameMs !== null ? `${metrics.timeToFirstFrameMs} ms` : '—'}
            </span>
          </div>

          <div className="metric-box">
            <span className="m-label">Time to Play</span>
            <span className="m-value">
              {metrics.timeToPlayingMs !== null ? `${metrics.timeToPlayingMs} ms` : '—'}
            </span>
          </div>

          <div className="metric-box">
            <span className="m-label">Last Seek Latency</span>
            <span className="m-value">
              {metrics.seekLatencyMs !== null ? `${metrics.seekLatencyMs} ms` : '—'}
            </span>
          </div>

          <div className="metric-box">
            <span className="m-label">Video Dimensions</span>
            <span className="m-value">
              {metrics.videoWidth && metrics.videoHeight
                ? `${metrics.videoWidth} × ${metrics.videoHeight}`
                : '—'}
            </span>
          </div>

          <div className="metric-box">
            <span className="m-label">Audio Output Check</span>
            <span className={`m-value ${metrics.hasAudioActivity ? 'text-green' : 'text-yellow'}`}>
              {metrics.hasAudioActivity
                ? '🔊 Audio Signal Detected'
                : metrics.stage === 'playing'
                  ? '⚠️ Silent / Unknown Track'
                  : '—'}
            </span>
          </div>
        </div>

        {/* Error Callout */}
        {metrics.nativeError && (
          <div className="alert-box alert-error">
            <strong>Native HTML5 Media Error Detected:</strong>
            <div className="mono mt-1">Code {metrics.nativeError.code}: {metrics.nativeError.name}</div>
            <div className="error-tip mt-1">
              {metrics.nativeError.code === 4 && (
                <span>
                  🚫 <strong>Format / Container Unsupported:</strong> This browser/device cannot demux or decode this container/codec directly in a <code>&lt;video&gt;</code> element.
                </span>
              )}
              {metrics.nativeError.code === 3 && (
                <span>
                  🚫 <strong>Hardware / Codec Decode Failure:</strong> The container was parsed, but the video or audio codec (e.g. HEVC Main 10 or DTS) could not be decoded by hardware.
                </span>
              )}
              {metrics.nativeError.code === 2 && (
                <span>
                  🚫 <strong>Network / Range Error:</strong> Connection to media host was aborted or failed.
                </span>
              )}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
