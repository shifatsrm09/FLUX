import { useRef, useState } from 'react';
import type { MediaMetadata } from '../types';
import './VideoPlayer.css';

interface VideoPlayerProps {
  url: string;
  metadata: MediaMetadata;
}

export function VideoPlayer({ url, metadata }: VideoPlayerProps) {
  const videoRef = useRef<HTMLVideoElement>(null);
  const [playbackError, setPlaybackError] = useState<string | null>(null);

  const handleError = () => {
    const video = videoRef.current;
    if (!video) return;

    const mediaError = video.error;
    let msg = 'Playback failed.';
    if (mediaError) {
      switch (mediaError.code) {
        case MediaError.MEDIA_ERR_ABORTED:
          msg = 'Playback aborted.';
          break;
        case MediaError.MEDIA_ERR_NETWORK:
          msg = 'Network error during playback.';
          break;
        case MediaError.MEDIA_ERR_DECODE:
          msg = 'Media decoding error — this format may not be supported by your browser.';
          break;
        case MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED:
          msg = 'This media format is not supported for direct browser playback. Transcoding or remuxing may be required.';
          break;
      }
    }
    setPlaybackError(msg);
  };

  return (
    <section className="video-player">
      <h2 className="panel-title">Video Player</h2>

      {!metadata.directPlayCandidate && (
        <div className="playback-notice">
          <span className="notice-icon">⚠</span>
          <span>
            This media ({metadata.container} / {metadata.video?.codec?.toUpperCase()}) may not play
            directly in the browser. Remuxing or transcoding support will be added in a future update.
          </span>
        </div>
      )}

      <div className="video-container">
        <video
          ref={videoRef}
          className="video-element"
          controls
          preload="metadata"
          onError={handleError}
        >
          <source src={url} />
          Your browser does not support HTML5 video.
        </video>
      </div>

      {playbackError && (
        <div className="playback-error">
          <span className="error-icon">✕</span>
          <span>{playbackError}</span>
        </div>
      )}
    </section>
  );
}
