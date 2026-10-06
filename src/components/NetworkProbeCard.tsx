import type { NetworkProbeResult } from '../types/diagnostics';

interface Props {
  probe: NetworkProbeResult | null;
  isLoading: boolean;
}

function formatBytes(bytes: number | null): string {
  if (!bytes) return 'Unknown';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 ** 2) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 ** 3) return `${(bytes / 1024 ** 2).toFixed(2)} MB`;
  return `${(bytes / 1024 ** 3).toFixed(2)} GB`;
}

export function NetworkProbeCard({ probe, isLoading }: Props) {
  if (isLoading) {
    return <div className="card loading-card">Probing network reachability and HTTP Range support...</div>;
  }

  if (!probe) return null;

  return (
    <div className="card network-card">
      <div className="card-header">
        <div className="card-title-group">
          <span className="card-icon">🌐</span>
          <h2 className="card-title">Network & Range Capabilities</h2>
        </div>
        <div className="header-badges">
          <span className={`pill ${probe.reachable ? 'pill-success' : 'pill-error'}`}>
            {probe.reachable ? '✓ Origin Reachable' : '✗ Unreachable'}
          </span>
          {probe.latencyMs > 0 && <span className="pill pill-dim">{probe.latencyMs} ms latency</span>}
        </div>
      </div>

      <div className="card-body">
        <div className="network-grid">
          <div className="net-item">
            <span className="net-label">HTTP Status:</span>
            <span className={`net-val ${probe.httpStatus === 206 || probe.httpStatus === 200 ? 'text-green' : 'text-red'}`}>
              {probe.httpStatus ? `${probe.httpStatus} ${probe.statusText || ''}` : 'No Response'}
            </span>
          </div>

          <div className="net-item">
            <span className="net-label">Range Requests:</span>
            <span className={`net-val ${probe.acceptRanges ? 'text-green' : 'text-yellow'}`}>
              {probe.acceptRanges ? `✓ Supported (${probe.acceptRanges})` : 'Not advertised'}
            </span>
          </div>

          <div className="net-item">
            <span className="net-label">Content Length:</span>
            <span className="net-val">
              {formatBytes(probe.contentLength)} {probe.contentLength ? `(${probe.contentLength.toLocaleString()} bytes)` : ''}
            </span>
          </div>

          <div className="net-item">
            <span className="net-label">Content Type:</span>
            <span className="net-val mono">{probe.contentType || 'Not provided'}</span>
          </div>

          <div className="net-item">
            <span className="net-label">CORS Status:</span>
            <span className={`net-val ${probe.corsAllowed ? 'text-green' : 'text-yellow'}`}>
              {probe.corsAllowed ? '✓ Allowed (Direct Fetch OK)' : 'Restricted (Native <video> may still work)'}
            </span>
          </div>

          <div className="net-item">
            <span className="net-label">Mixed Content:</span>
            <span className={`net-val ${probe.mixedContentBlocked ? 'text-red' : 'text-green'}`}>
              {probe.mixedContentBlocked ? '⚠️ Mixed Content Risk (HTTPS page calling HTTP)' : '✓ Safe'}
            </span>
          </div>
        </div>

        {probe.error && (
          <div className="alert-box alert-warning">
            <strong>Network Diagnostic Note:</strong> {probe.error}
            <div className="alert-tip">
              💡 <em>Tip for Mobile & Vercel:</em> Browsers allow native <code>&lt;video src="http://..."&gt;</code> elements even when <code>fetch()</code> range requests are restricted by CORS, because HTML5 video elements bypass strict CORS unless cross-origin attributes are enforced.
            </div>
          </div>
        )}
      </div>
    </div>
  );
}
