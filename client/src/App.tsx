import { useState } from 'react';
import './App.css';
import { UrlInput } from './components/UrlInput';
import { MetadataPanel } from './components/MetadataPanel';
import { VideoPlayer } from './components/VideoPlayer';
import type { MediaMetadata } from './types';

function App() {
  const [metadata, setMetadata] = useState<MediaMetadata | null>(null);
  const [videoUrl, setVideoUrl] = useState<string>('');
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const handleAnalyze = async (url: string) => {
    setLoading(true);
    setError(null);
    setMetadata(null);
    setVideoUrl('');

    try {
      const res = await fetch('/api/analyze', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ url }),
      });

      const data = await res.json();

      if (!res.ok) {
        throw new Error(data.error || data.detail || 'Analysis failed');
      }

      setMetadata(data.metadata);
      setVideoUrl(url);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'An unknown error occurred');
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="app">
      <header className="app-header">
        <div className="logo">
          <span className="logo-icon">▶</span>
          <h1>BDIXStream</h1>
        </div>
        <p className="tagline">Internal Media Streaming Platform</p>
      </header>

      <main className="app-main">
        <UrlInput onAnalyze={handleAnalyze} loading={loading} />

        {error && (
          <div className="error-banner">
            <span className="error-icon">✕</span>
            <span>{error}</span>
          </div>
        )}

        {metadata && (
          <div className="results">
            <MetadataPanel metadata={metadata} />
            <VideoPlayer url={videoUrl} metadata={metadata} />
          </div>
        )}
      </main>

      <footer className="app-footer">
        <span>BDIXStream v0.1.0 — Prototype</span>
      </footer>
    </div>
  );
}

export default App;
