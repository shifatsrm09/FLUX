import { useState, type FormEvent } from 'react';
import './UrlInput.css';

interface UrlInputProps {
  onAnalyze: (url: string) => void;
  loading: boolean;
}

export function UrlInput({ onAnalyze, loading }: UrlInputProps) {
  const [url, setUrl] = useState('');

  const handleSubmit = (e: FormEvent) => {
    e.preventDefault();
    const trimmed = url.trim();
    if (trimmed) {
      onAnalyze(trimmed);
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
          onChange={(e) => setUrl(e.target.value)}
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
