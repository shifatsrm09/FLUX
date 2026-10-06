import { useState, type FormEvent } from 'react';

interface Props {
  initialUrl: string;
  onRunTest: (url: string) => void;
  isRunning: boolean;
}

const STORAGE_KEY = 'bdixstream_media_test_url';

export function UrlInputBar({ initialUrl, onRunTest, isRunning }: Props) {
  const [url, setUrl] = useState(() => {
    try {
      return localStorage.getItem(STORAGE_KEY) || initialUrl || 'http://172.16.50.14/Barbie.2023.1080p.x265.mkv';
    } catch {
      return initialUrl;
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

  const handleQuickSelect = (preset: string) => {
    setUrl(preset);
    try {
      localStorage.setItem(STORAGE_KEY, preset);
    } catch {
      // ignore
    }
    onRunTest(preset);
  };

  return (
    <div className="card input-card">
      <form onSubmit={handleSubmit} className="input-form">
        <div className="input-header">
          <label htmlFor="media-url" className="input-label">
            <span className="card-icon">🎯</span> Media Test Target (Direct BDIX URL)
          </label>
          <span className="input-hint">Direct client-to-origin streaming (no proxy)</span>
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
              '⚡ Test Playback & Codecs'
            )}
          </button>
        </div>

        <div className="quick-presets">
          <span className="preset-label">Quick Targets:</span>
          <button
            type="button"
            className="preset-btn"
            disabled={isRunning}
            onClick={() => handleQuickSelect('http://172.16.50.14/Barbie.2023.1080p.x265.mkv')}
          >
            Barbie (172.16.50.14 MKV)
          </button>
          <button
            type="button"
            className="preset-btn"
            disabled={isRunning}
            onClick={() => handleQuickSelect('http://172.16.50.4/movie.mkv')}
          >
            172.16.50.4 MKV
          </button>
          <button
            type="button"
            className="preset-btn"
            disabled={isRunning}
            onClick={() => handleQuickSelect('https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4')}
          >
            Public MP4 (Control Test)
          </button>
        </div>
      </form>
    </div>
  );
}
