import type { MediaMetadata } from '../types';
import './MetadataPanel.css';

interface MetadataPanelProps {
  metadata: MediaMetadata;
}

function InfoRow({ label, value }: { label: string; value: string | null | undefined }) {
  if (!value) return null;
  return (
    <div className="info-row">
      <span className="info-label">{label}</span>
      <span className="info-value">{value}</span>
    </div>
  );
}

function formatChannels(channels: number | null): string | null {
  if (channels === null) return null;
  if (channels === 1) return 'Mono';
  if (channels === 2) return 'Stereo';
  if (channels === 6) return '5.1';
  if (channels === 8) return '7.1';
  return `${channels}ch`;
}

function formatFrameRate(fr: string | null): string | null {
  if (!fr) return null;
  // ffprobe often returns "24000/1001" style
  const parts = fr.split('/');
  if (parts.length === 2) {
    const num = parseFloat(parts[0]);
    const den = parseFloat(parts[1]);
    if (den > 0) return `${(num / den).toFixed(3)} fps`;
  }
  return `${fr} fps`;
}

export function MetadataPanel({ metadata }: MetadataPanelProps) {
  const resolution =
    metadata.video?.width && metadata.video?.height
      ? `${metadata.video.width} × ${metadata.video.height}`
      : null;

  return (
    <section className="metadata-panel">
      <h2 className="panel-title">Media Information</h2>

      <div className="badge-row">
        <span className="badge badge-container">{metadata.container}</span>
        {metadata.video?.codec && (
          <span className="badge badge-video">{metadata.video.codec.toUpperCase()}</span>
        )}
        {metadata.audio?.codec && (
          <span className="badge badge-audio">{metadata.audio.codec.toUpperCase()}</span>
        )}
        <span className={`badge ${metadata.directPlayCandidate ? 'badge-success' : 'badge-warning'}`}>
          {metadata.directPlayCandidate ? '✓ Direct Play' : '✗ Needs Processing'}
        </span>
      </div>

      <div className="info-grid">
        <div className="info-section">
          <h3 className="section-title">General</h3>
          <InfoRow label="Filename" value={metadata.filename} />
          <InfoRow label="Format" value={metadata.format} />
          <InfoRow label="Duration" value={metadata.duration} />
          <InfoRow label="Size" value={metadata.fileSize} />
          <InfoRow label="Bitrate" value={metadata.bitrate} />
        </div>

        {metadata.video && (
          <div className="info-section">
            <h3 className="section-title">Video</h3>
            <InfoRow label="Codec" value={metadata.video.codec?.toUpperCase()} />
            <InfoRow label="Profile" value={metadata.video.profile} />
            <InfoRow label="Resolution" value={resolution} />
            <InfoRow label="Frame Rate" value={formatFrameRate(metadata.video.frameRate)} />
            <InfoRow label="Bitrate" value={metadata.video.bitrate} />
            <InfoRow label="Pixel Format" value={metadata.video.pixelFormat} />
          </div>
        )}

        {metadata.audio && (
          <div className="info-section">
            <h3 className="section-title">Audio</h3>
            <InfoRow label="Codec" value={metadata.audio.codec?.toUpperCase()} />
            <InfoRow label="Profile" value={metadata.audio.profile} />
            <InfoRow label="Channels" value={formatChannels(metadata.audio.channels)} />
            <InfoRow label="Layout" value={metadata.audio.channelLayout} />
            <InfoRow label="Sample Rate" value={metadata.audio.sampleRate ? `${metadata.audio.sampleRate} Hz` : null} />
            <InfoRow label="Bitrate" value={metadata.audio.bitrate} />
            <InfoRow label="Language" value={metadata.audio.language} />
          </div>
        )}
      </div>

      {metadata.audioTracks.length > 1 && (
        <div className="extra-section">
          <h3 className="section-title">Audio Tracks ({metadata.audioTracks.length})</h3>
          <div className="track-list">
            {metadata.audioTracks.map((t) => (
              <div key={t.index} className="track-item">
                <span className="track-badge">#{t.index + 1}</span>
                <span>{t.codec?.toUpperCase()}</span>
                <span>{formatChannels(t.channels)}</span>
                {t.language && <span className="track-lang">{t.language}</span>}
              </div>
            ))}
          </div>
        </div>
      )}

      {metadata.subtitles.length > 0 && (
        <div className="extra-section">
          <h3 className="section-title">Subtitles ({metadata.subtitles.length})</h3>
          <div className="track-list">
            {metadata.subtitles.map((s) => (
              <div key={s.index} className="track-item">
                <span className="track-badge">#{s.index + 1}</span>
                <span>{s.codec?.toUpperCase()}</span>
                {s.language && <span className="track-lang">{s.language}</span>}
                {s.title && <span className="track-title">{s.title}</span>}
              </div>
            ))}
          </div>
        </div>
      )}
    </section>
  );
}
