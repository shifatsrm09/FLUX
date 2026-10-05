import { useRef, useState, useEffect } from 'react';
import type { MediaMetadata } from '../types';
import './VideoPlayer.css';

interface VideoPlayerProps {
  url: string;
  metadata: MediaMetadata;
}

type PlaybackStatus = 'idle' | 'preparing' | 'playing' | 'paused' | 'error';

export function VideoPlayer({ url, metadata }: VideoPlayerProps) {
  const videoRef = useRef<HTMLVideoElement>(null);
  const [playbackStatus, setPlaybackStatus] = useState<PlaybackStatus>('idle');
  const [errorTitle, setErrorTitle] = useState<string | null>(null);
  const [errorDetail, setErrorDetail] = useState<string | null>(null);

  const isDirectPlay = metadata.directPlayCandidate;
  const videoCodec = (metadata.video?.codec || '').toLowerCase();
  const isHevc = videoCodec.includes('hevc') || videoCodec.includes('h265');

  // Direct play uses the original URL; otherwise stream via on-demand remux endpoint
  const streamUrl = isDirectPlay
    ? url
    : `/api/stream?url=${encodeURIComponent(url)}`;

  // Reset state when media URL or metadata changes
  useEffect(() => {
    setPlaybackStatus('idle');
    setErrorTitle(null);
    setErrorDetail(null);
    if (videoRef.current) {
      videoRef.current.load();
    }
  }, [url, metadata]);

  const handleError = () => {
    setPlaybackStatus('error');
    const video = videoRef.current;
    const mediaError = video?.error;

    let title = 'Playback Error';
    let detail = 'An unknown error occurred during media playback.';

    if (mediaError) {
      switch (mediaError.code) {
        case MediaError.MEDIA_ERR_ABORTED:
          title = 'Playback Aborted';
          detail = 'The playback was aborted by the browser or user.';
          break;
        case MediaError.MEDIA_ERR_NETWORK:
          title = 'Network Error';
          detail = 'A network error occurred while streaming media from the server.';
          break;
        case MediaError.MEDIA_ERR_DECODE:
          if (!isDirectPlay && isHevc) {
            title = 'Codec Decode Error: HEVC / H.265';
            detail =
              'The browser attempted to decode the stream but failed. Your browser or GPU lacks native decoding support for HEVC (H.265) Main 10. Server-side transcoding (HEVC → H.264) will be required.';
          } else {
            title = 'Media Decoding Error';
            detail =
              'The media could not be decoded. The stream format or codec profile is not supported by your browser.';
          }
          break;
        case MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED:
          if (!isDirectPlay && isHevc) {
            title = 'Unsupported Codec: HEVC Main 10';
            detail =
              'Your browser does not support HEVC (H.265) video in MP4. On-demand remuxing successfully converted the container to fragmented MP4, but the underlying video codec is unchanged. Most desktop browsers require H.264 video. Full playback for this file will require transcoding (HEVC → H.264) in a future milestone.';
          } else if (!isDirectPlay) {
            title = 'Stream Format Not Supported';
            detail = `The remuxed stream (${metadata.video?.codec?.toUpperCase() || 'Unknown codec'} in MP4) could not be loaded. Server-side transcoding may be required.`;
          } else {
            title = 'Direct Playback Unsupported';
            detail =
              'This media format is not supported for direct browser playback. Remuxing or transcoding is required.';
          }
          break;
      }
    }

    setErrorTitle(title);
    setErrorDetail(detail);
  };

  return (
    <section className="video-player">
      <div className="player-header">
        <h2 className="panel-title">Video Player</h2>

        <div className="player-badges">
          <span className={`badge ${isDirectPlay ? 'badge-direct' : 'badge-remux'}`}>
            {isDirectPlay ? 'Direct Play' : '⚡ On-Demand Remux (fMP4)'}
          </span>

          <span className={`status-pill status-${playbackStatus}`}>
            {playbackStatus === 'preparing' && <span className="pill-spinner" />}
            {playbackStatus === 'playing' && <span className="pill-dot active" />}
            {playbackStatus === 'paused' && <span className="pill-dot paused" />}
            {playbackStatus === 'error' && <span className="pill-dot error" />}
            {playbackStatus === 'preparing' && 'Preparing stream...'}
            {playbackStatus === 'playing' && 'Playing'}
            {playbackStatus === 'paused' && 'Paused'}
            {playbackStatus === 'error' && 'Playback Error'}
            {playbackStatus === 'idle' && 'Ready to Play'}
          </span>
        </div>
      </div>

      {!isDirectPlay && (
        <div className="remux-notice">
          <div className="remux-notice-title">
            <span className="notice-icon">⚡</span>
            <span>On-Demand Remux Stream Active</span>
          </div>
          <p className="remux-notice-desc">
            Streaming via <code>/api/stream</code> using stream copy (video:{' '}
            <strong>{metadata.video?.codec?.toUpperCase() || 'copy'}</strong>, audio:{' '}
            <strong>{metadata.audio?.codec?.toUpperCase() || 'copy'}</strong>) into fragmented MP4.
            The file is not stored on disk.
          </p>

          {isHevc && (
            <div className="codec-warning">
              <strong>Codec Note:</strong> Source video is <strong>HEVC / H.265 Main 10</strong>.
              Remuxing repackages the container to MP4 without re-encoding. If your browser lacks
              hardware HEVC support, playback will report an unsupported codec error below.
            </div>
          )}
        </div>
      )}

      <div className="video-container">
        <video
          ref={videoRef}
          key={streamUrl}
          src={streamUrl}
          className="video-element"
          controls
          preload="metadata"
          onLoadStart={() => setPlaybackStatus('preparing')}
          onWaiting={() => {
            if (playbackStatus !== 'error') setPlaybackStatus('preparing');
          }}
          onCanPlay={() => {
            if (playbackStatus === 'preparing') setPlaybackStatus('idle');
          }}
          onPlay={() => setPlaybackStatus('playing')}
          onPlaying={() => setPlaybackStatus('playing')}
          onPause={() => {
            if (playbackStatus !== 'error') setPlaybackStatus('paused');
          }}
          onError={handleError}
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
