import type { EbmlHeaderInfo } from '../types/diagnostics';

interface Props {
  header: EbmlHeaderInfo | null;
  isLoading: boolean;
  error?: string;
}

function formatChannels(ch?: number): string {
  if (!ch) return 'Unknown';
  if (ch === 1) return '1.0 (Mono)';
  if (ch === 2) return '2.0 (Stereo)';
  if (ch === 6) return '5.1 Surround';
  if (ch === 8) return '7.1 Surround';
  return `${ch} Channels`;
}

function formatDuration(sec?: number): string {
  if (!sec) return 'Unknown';
  const h = Math.floor(sec / 3600);
  const m = Math.floor((sec % 3600) / 60);
  const s = Math.floor(sec % 60);
  return `${h > 0 ? `${h}h ` : ''}${m}m ${s}s`;
}

export function StreamMetadata({ header, isLoading, error }: Props) {
  if (isLoading) {
    return <div className="card loading-card">Demuxing MKV headers over HTTP Range...</div>;
  }

  if (error) {
    return (
      <div className="card metadata-card">
        <div className="card-header">
          <div className="card-title-group">
            <span className="card-icon">📦</span>
            <h2 className="card-title">In-Browser MKV Header Inspection</h2>
          </div>
        </div>
        <div className="card-body">
          <div className="alert-box alert-warning">
            <strong>EBML Header Probe Note:</strong> {error}
            <div className="alert-tip">
              If CORS is not enabled on the origin media server, browser <code>fetch()</code> Range calls are blocked, but native <code>&lt;video&gt;</code> elements may still stream and play the MKV directly below.
            </div>
          </div>
        </div>
      </div>
    );
  }

  if (!header) return null;

  const videoTracks = header.tracks.filter((t) => t.trackType === 'video');
  const audioTracks = header.tracks.filter((t) => t.trackType === 'audio');
  const subtitleTracks = header.tracks.filter((t) => t.trackType === 'subtitle');

  return (
    <div className="card metadata-card">
      <div className="card-header">
        <div className="card-title-group">
          <span className="card-icon">📦</span>
          <h2 className="card-title">In-Browser MKV Stream Metadata</h2>
        </div>
        <div className="header-badges">
          <span className="pill pill-mkv">{header.docType.toUpperCase()} v{header.docTypeVersion}</span>
          {header.durationSeconds && (
            <span className="pill pill-dim">{formatDuration(header.durationSeconds)}</span>
          )}
          <span className="pill pill-dim">Headers Read: {(header.bytesRead / 1024).toFixed(0)} KB</span>
        </div>
      </div>

      <div className="card-body">
        {header.title && (
          <div className="media-title-bar">
            <span className="title-label">Embedded Title:</span>
            <span className="title-text">{header.title}</span>
          </div>
        )}

        {/* Video Track */}
        <div className="tracks-section">
          <h3 className="section-subtitle">Video Track ({videoTracks.length})</h3>
          {videoTracks.length === 0 ? (
            <p className="empty-text">No video stream found in header</p>
          ) : (
            <div className="track-cards-list">
              {videoTracks.map((vt) => (
                <div key={vt.trackNumber} className="track-card video-track-card">
                  <div className="track-card-header">
                    <span className="track-badge">Track #{vt.trackNumber}</span>
                    <strong className="track-name">{vt.codecName}</strong>
                    <span className="codec-id-badge">{vt.codecId}</span>
                  </div>
                  <div className="track-details-row">
                    {vt.pixelWidth && vt.pixelHeight && (
                      <span className="detail-item">
                        Resolution: <strong>{vt.pixelWidth} × {vt.pixelHeight}</strong>
                      </span>
                    )}
                    {vt.name && <span className="detail-item">Title: {vt.name}</span>}
                    {vt.language && <span className="detail-item">Language: {vt.language.toUpperCase()}</span>}
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Audio Tracks */}
        <div className="tracks-section">
          <h3 className="section-subtitle">Audio Tracks ({audioTracks.length})</h3>
          {audioTracks.length === 0 ? (
            <p className="empty-text">No audio streams found in header</p>
          ) : (
            <div className="track-cards-list">
              {audioTracks.map((at) => {
                const isDts = at.codecId.includes('DTS');
                const isAac = at.codecId.includes('AAC');
                const isAc3 = at.codecId.includes('AC3') || at.codecId.includes('EAC3');

                return (
                  <div key={at.trackNumber} className={`track-card audio-track-card ${isDts ? 'track-warning' : ''}`}>
                    <div className="track-card-header">
                      <span className="track-badge">Track #{at.trackNumber}</span>
                      <strong className="track-name">{at.codecName}</strong>
                      <span className="codec-id-badge">{at.codecId}</span>
                      {at.isDefault && <span className="pill pill-default">Default</span>}
                      {isAac && <span className="pill pill-green">AAC Compatible</span>}
                      {isAc3 && <span className="pill pill-yellow">Dolby AC-3</span>}
                      {isDts && <span className="pill pill-warn-tag">⚠️ High Incompatibility Risk</span>}
                    </div>

                    <div className="track-details-row">
                      <span className="detail-item">Channels: <strong>{formatChannels(at.channels)}</strong></span>
                      {at.samplingFrequency && (
                        <span className="detail-item">Sample Rate: <strong>{at.samplingFrequency} Hz</strong></span>
                      )}
                      {at.language && (
                        <span className="detail-item">Language: <strong>{at.language.toUpperCase()}</strong></span>
                      )}
                      {at.name && (
                        <span className="detail-item">Title: <em>"{at.name}"</em></span>
                      )}
                    </div>

                    {isDts && (
                      <div className="track-note-dts">
                        ⚠️ <strong>DTS Codec Alert:</strong> Most web browsers (including Safari, Chrome, and Firefox) cannot decode DTS audio natively. If this is the primary or default track, video may play silently unless the browser or player supports track switching to an AAC/AC-3 track.
                      </div>
                    )}
                  </div>
                );
              })}
            </div>
          )}
        </div>

        {/* Subtitles */}
        {subtitleTracks.length > 0 && (
          <div className="tracks-section">
            <h3 className="section-subtitle">Subtitle Tracks ({subtitleTracks.length})</h3>
            <div className="subtitle-pills-row">
              {subtitleTracks.map((st) => (
                <span key={st.trackNumber} className="subtitle-chip">
                  #{st.trackNumber} {st.language ? st.language.toUpperCase() : 'UND'} ({st.codecName})
                  {st.name ? ` — ${st.name}` : ''}
                </span>
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
