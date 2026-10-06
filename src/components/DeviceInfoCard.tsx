import { useState } from 'react';
import type { DeviceInfo, CodecSupportMatrix } from '../types/diagnostics';

interface Props {
  device: DeviceInfo | null;
  codecs: CodecSupportMatrix | null;
}

export function DeviceInfoCard({ device, codecs }: Props) {
  const [showAllCodecs, setShowAllCodecs] = useState(false);

  if (!device) {
    return <div className="card loading-card">Probing device and media capabilities...</div>;
  }

  // Key format highlights
  const mkvDirect = codecs?.['video/x-matroska']?.canPlayType || 'unknown';
  const mkvHevc = codecs?.['video/x-matroska; codecs="hvc1.1.6.L93.B0"']?.canPlayType || 'unknown';
  const mp4Hevc = codecs?.['video/mp4; codecs="hvc1.1.6.L93.B0, mp4a.40.2"']?.canPlayType || 'unknown';
  const mp4Avc = codecs?.['video/mp4; codecs="avc1.640028, mp4a.40.2"']?.canPlayType || 'unknown';
  const audioAac = codecs?.['audio/mp4; codecs="mp4a.40.2"']?.canPlayType || 'unknown';
  const audioAc3 = codecs?.['audio/mp4; codecs="ac-3"']?.canPlayType || 'unknown';
  const audioEac3 = codecs?.['audio/mp4; codecs="ec-3"']?.canPlayType || 'unknown';
  const audioDts = codecs?.['audio/vnd.dts']?.canPlayType || 'unknown';

  const badgeClass = (val: string) => {
    if (val === 'probably') return 'badge-pill badge-green';
    if (val === 'maybe') return 'badge-pill badge-yellow';
    return 'badge-pill badge-red';
  };

  return (
    <div className="card device-card">
      <div className="card-header">
        <div className="card-title-group">
          <span className="card-icon">📱</span>
          <h2 className="card-title">Device & Browser Environment</h2>
        </div>
        <div className="header-badges">
          <span className="pill pill-os">{device.osName}</span>
          <span className="pill pill-browser">{device.browserName} {device.browserVersion}</span>
          <span className={`pill ${device.isMobile ? 'pill-mobile' : 'pill-desktop'}`}>
            {device.isMobile ? 'Mobile' : 'Desktop'}
          </span>
        </div>
      </div>

      <div className="card-body">
        {/* Core Capabilities */}
        <div className="capabilities-grid">
          <div className="cap-item">
            <span className="cap-label">MediaSource (MSE):</span>
            <span className={`cap-value ${device.hasMediaSource ? 'text-green' : 'text-red'}`}>
              {device.hasMediaSource ? '✓ Supported' : '✗ Unavailable'}
            </span>
          </div>
          <div className="cap-item">
            <span className="cap-label">Managed MSE (iOS):</span>
            <span className={`cap-value ${device.hasManagedMediaSource ? 'text-green' : 'text-dim'}`}>
              {device.hasManagedMediaSource ? '✓ Supported' : '✗ Not Available'}
            </span>
          </div>
          <div className="cap-item">
            <span className="cap-label">Web Audio API:</span>
            <span className={`cap-value ${device.hasWebAudio ? 'text-green' : 'text-red'}`}>
              {device.hasWebAudio ? '✓ Supported' : '✗ Unavailable'}
            </span>
          </div>
          <div className="cap-item">
            <span className="cap-label">Brave Shields:</span>
            <span className="cap-value">
              {device.isBrave ? '🛡️ Brave Detected' : 'Standard Chromium/Engine'}
            </span>
          </div>
        </div>

        {/* Quick Codec Matrix */}
        <div className="codec-summary-section">
          <h3 className="section-subtitle">Native Decoding Support Probe</h3>
          <div className="codec-pill-row">
            <div className="pill-metric">
              <span className="metric-tag">MKV Container</span>
              <span className={badgeClass(mkvDirect)}>{mkvDirect.toUpperCase()}</span>
            </div>
            <div className="pill-metric">
              <span className="metric-tag">MKV + HEVC</span>
              <span className={badgeClass(mkvHevc)}>{mkvHevc.toUpperCase()}</span>
            </div>
            <div className="pill-metric">
              <span className="metric-tag">MP4 + HEVC</span>
              <span className={badgeClass(mp4Hevc)}>{mp4Hevc.toUpperCase()}</span>
            </div>
            <div className="pill-metric">
              <span className="metric-tag">MP4 + H.264</span>
              <span className={badgeClass(mp4Avc)}>{mp4Avc.toUpperCase()}</span>
            </div>
            <div className="pill-metric">
              <span className="metric-tag">AAC Audio</span>
              <span className={badgeClass(audioAac)}>{audioAac.toUpperCase()}</span>
            </div>
            <div className="pill-metric">
              <span className="metric-tag">AC-3 (Dolby)</span>
              <span className={badgeClass(audioAc3)}>{audioAc3.toUpperCase()}</span>
            </div>
            <div className="pill-metric">
              <span className="metric-tag">E-AC-3 (DDP)</span>
              <span className={badgeClass(audioEac3)}>{audioEac3.toUpperCase()}</span>
            </div>
            <div className="pill-metric">
              <span className="metric-tag">DTS Audio</span>
              <span className={badgeClass(audioDts)}>{audioDts.toUpperCase()}</span>
            </div>
          </div>
        </div>

        {/* Detailed Codec Probe Toggle */}
        {codecs && (
          <div className="details-toggle-box">
            <button
              type="button"
              className="btn-text"
              onClick={() => setShowAllCodecs(!showAllCodecs)}
            >
              {showAllCodecs ? '▲ Hide Full Codec Matrix' : '▼ Show All Tested MIME Formats (' + Object.keys(codecs).length + ')'}
            </button>

            {showAllCodecs && (
              <div className="codecs-table-container">
                <table className="codecs-table">
                  <thead>
                    <tr>
                      <th>MIME Type & Codec Specification</th>
                      <th>canPlayType</th>
                      <th>Hardware / Smooth</th>
                    </tr>
                  </thead>
                  <tbody>
                    {Object.entries(codecs).map(([mime, res]) => (
                      <tr key={mime}>
                        <td className="mono">{mime}</td>
                        <td>
                          <span className={badgeClass(res.canPlayType)}>{res.canPlayType}</span>
                        </td>
                        <td>
                          {res.mediaCapabilities ? (
                            <span className="cap-mc">
                              {res.mediaCapabilities.supported ? '✓ Supported' : '✗ Unsupported'}
                              {res.mediaCapabilities.powerEfficient && ' (Power Efficient / HW)'}
                              {res.mediaCapabilities.smooth && ' (Smooth)'}
                            </span>
                          ) : (
                            <span className="text-dim">—</span>
                          )}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
}
