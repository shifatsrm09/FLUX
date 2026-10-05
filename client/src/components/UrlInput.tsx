import { useState, type FormEvent } from 'react';
import './UrlInput.css';

interface UrlInputProps {
  onAnalyze: (url: string) => void;
  loading: boolean;
}

const STORAGE_KEY = 'bdixstream_last_media_url';

export function UrlInput({ onAnalyze, loading }: UrlInputProps) {
  const [url, setUrl] = useState(() => {
    try {
      return localStorage.getItem(STORAGE_KEY) || '';
    } catch {
      return '';
    }
  });

  const handleSubmit = (e: FormEvent) => {
    e.preventDefault();
    const trimmed = url.trim();
    if (trimmed) {
      try {
        localStorage.setItem(STORAGE_KEY, trimmed);
      } catch {
        // ignore localStorage errors (e.g. incognito/disabled)
      }
      onAnalyze(trimmed);
    }
  };

  const handleChange = (newVal: string) => {
    setUrl(newVal);
    try {
      localStorage.setItem(STORAGE_KEY, newVal);
    } catch {
      // ignore
    }
  };

  return (
    <form className="url-input" onSubmit={handleSubmit}>
      <label htmlFor="media-url" className="url-label">Video URL</label>
      <div className="url-row">
        <input
          id="media-url"
          type="url"
          className="url-field"
          placeholder="http://172.16.50.4/path/to/movie.mkv"
          value={url}
          onChange={(e) => handleChange(e.target.value)}
          disabled={loading}
          autoFocus
        />
        <button type="submit" className="analyze-btn" disabled={loading || !url.trim()}>
          {loading ? (
            <span className="spinner" />
          ) : (
            'Analyze'
          )}
        </button>
      </div>
    </form>
  );
}
