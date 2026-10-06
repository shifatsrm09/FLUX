import { useState } from 'react';
import type { MediaEventLog, CompleteDiagnosticReport } from '../types/diagnostics';

interface Props {
  events: MediaEventLog[];
  reportGenerator: () => CompleteDiagnosticReport;
  onClear: () => void;
}

export function DiagnosticLog({ events, reportGenerator, onClear }: Props) {
  const [copied, setCopied] = useState(false);
  const [showJsonModal, setShowJsonModal] = useState(false);
  const [jsonText, setJsonText] = useState('');

  const handleCopyReport = async () => {
    try {
      const report = reportGenerator();
      const text = JSON.stringify(report, null, 2);
      await navigator.clipboard.writeText(text);
      setCopied(true);
      setTimeout(() => setCopied(false), 2500);
    } catch {
      // fallback
      const report = reportGenerator();
      setJsonText(JSON.stringify(report, null, 2));
      setShowJsonModal(true);
    }
  };

  const handleViewReport = () => {
    const report = reportGenerator();
    setJsonText(JSON.stringify(report, null, 2));
    setShowJsonModal(true);
  };

  return (
    <div className="card log-card">
      <div className="card-header">
        <div className="card-title-group">
          <span className="card-icon">📋</span>
          <h2 className="card-title">Media Lifecycle Event Log ({events.length})</h2>
        </div>
        <div className="header-actions">
          <button type="button" className="btn btn-sm" onClick={handleViewReport}>
            🔍 View JSON
          </button>
          <button type="button" className="btn btn-sm btn-primary" onClick={handleCopyReport}>
            {copied ? '✓ Copied Report!' : '📋 Copy Diagnostic JSON'}
          </button>
          <button type="button" className="btn btn-sm btn-ghost" onClick={onClear} disabled={events.length === 0}>
            Clear
          </button>
        </div>
      </div>

      <div className="card-body">
        {events.length === 0 ? (
          <p className="empty-text">No media lifecycle events recorded yet. Enter a media URL above and click Test.</p>
        ) : (
          <div className="event-stream">
            {events.map((e, idx) => {
              const dateStr = new Date(e.timestamp).toLocaleTimeString();
              const isError = e.eventName === 'error';
              const isSuccess = e.eventName === 'playing' || e.eventName === 'first_frame_ready';
              const isWarn = e.eventName === 'waiting' || e.eventName === 'stalled';

              let rowClass = 'event-row';
              if (isError) rowClass += ' event-error';
              else if (isSuccess) rowClass += ' event-success';
              else if (isWarn) rowClass += ' event-warn';

              return (
                <div key={idx} className={rowClass}>
                  <span className="event-time mono">{dateStr}</span>
                  <span className="event-name mono font-bold">{e.eventName}</span>
                  <span className="event-stage pill pill-dim">{e.stage}</span>
                  {e.details && (
                    <span className="event-details mono">
                      {JSON.stringify(e.details)}
                    </span>
                  )}
                </div>
              );
            })}
          </div>
        )}
      </div>

      {showJsonModal && (
        <div className="modal-backdrop" onClick={() => setShowJsonModal(false)}>
          <div className="modal-card" onClick={(e) => e.stopPropagation()}>
            <div className="modal-header">
              <h3>Diagnostic JSON Report</h3>
              <button type="button" className="btn-close" onClick={() => setShowJsonModal(false)}>✕</button>
            </div>
            <div className="modal-body">
              <textarea
                className="json-textarea mono"
                readOnly
                value={jsonText}
                onClick={(e) => (e.target as HTMLTextAreaElement).select()}
              />
            </div>
            <div className="modal-footer">
              <button
                type="button"
                className="btn btn-primary"
                onClick={async () => {
                  await navigator.clipboard.writeText(jsonText);
                  setCopied(true);
                  setTimeout(() => setCopied(false), 2000);
                }}
              >
                {copied ? '✓ Copied!' : 'Copy to Clipboard'}
              </button>
              <button type="button" className="btn" onClick={() => setShowJsonModal(false)}>
                Close
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
