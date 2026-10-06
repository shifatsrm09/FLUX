import { useState, type FormEvent } from 'react';
import { HARDCODED_PRESETS, type MoviePreset } from '../constants/presets';

interface Props {
  initialUrl: string;
  onRunTest: (url: string) => void;
  isRunning: boolean;
}

const STORAGE_KEY = 'bdixstream_media_test_url';

export function UrlInputBar({ initialUrl, onRunTest, isRunning }: Props) {
  const [url, setUrl] = useState(() => {
    try {
      return localStorage.getItem(STORAGE_KEY) || initialUrl || HARDCODED_PRESETS[0].url;
    } catch {
      return initialUrl || HARDCODED_PRESETS[0].url;
    }
  });

  const handleSubmit = (e: FormEvent) => {
    e.preventDefault();
    const trimmed = url.trim();
    if (trimmed) {
      try {
        localStorage.setItem(STORAGE_KEY, trimmed);
      } catch {
        // ignore
      }
      onRunTest(trimmed);
    }
  };

  const handleSelectPreset = (preset: MoviePreset) => {
    setUrl(preset.url);
    try {
      localStorage.setItem(STORAGE_KEY, preset.url);
    } catch {
      // ignore
    }
    onRunTest(preset.url);
  };

  return (
    <div className="card input-card">
      {/* Quick Hardcoded Test Media Presets */}
      <div className="presets-container">
        <div className="presets-header">
          <span className="presets-title">
            <span className="card-icon">⚡</span> Quick Test Presets (BDIX Origin 172.16.50.14)
          </span>
          <span className="presets-subtitle">Click any title to load & probe immediately</span>
        </div>

        <div className="presets-grid">
          {HARDCODED_PRESETS.map((preset) => {
            const isSelected = url === preset.url;
            return (
              <button
                key={preset.id}
                type="button"
                className={`preset-card ${isSelected ? 'preset-card-selected' : ''}`}
                disabled={isRunning}
                onClick={() => handleSelectPreset(preset)}
              >
                <div className="preset-card-top">
                  <span className="preset-movie-name">{preset.name}</span>
                  {preset.year && <span className="preset-year-badge">{preset.year}</span>}
                </div>
                <div className="preset-badge-row">
                  <span className="preset-tech-badge">{preset.badge}</span>
                </div>
                <div className="preset-details-text">{preset.details}</div>
                {isSelected && <span className="preset-active-indicator">● Active Selection</span>}
              </button>
            );
          })}
        </div>
      </div>

      {/* Manual URL Input Form */}
      <form onSubmit={handleSubmit} className="input-form">
        <div className="input-header">
          <label htmlFor="media-url" className="input-label">
            <span className="card-icon">🔗</span> Active Media Stream URL
          </label>
          <span className="input-hint">Direct client-to-origin HTTP streaming</span>
        </div>

        <div className="input-row">
          <input
            id="media-url"
            type="url"
            className="text-input"
            placeholder="http://172.16.50.14/path/to/movie.mkv"
            value={url}
            onChange={(e) => setUrl(e.target.value)}
            disabled={isRunning}
            required
          />
          <button type="submit" className="btn btn-primary" disabled={isRunning || !url.trim()}>
            {isRunning ? (
              <span className="btn-content">
                <span className="spinner" /> Probing Media...
              </span>
            ) : (
              '⚡ Run Diagnostics'
            )}
          </button>
        </div>
      </form>
    </div>
  );
}
