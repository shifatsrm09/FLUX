import { useState, useEffect, useCallback, useRef } from 'react';
import type {
  DeviceInfo,
  CodecSupportMatrix,
  NetworkProbeResult,
  EbmlHeaderInfo,
  PlaybackMetrics,
  MediaEventLog,
  CompleteDiagnosticReport,
} from './types/diagnostics';
import { detectDevice } from './utils/deviceDetector';
import { probeCodecSupport } from './utils/codecProber';
import { probeNetworkAndRange } from './utils/networkProber';
import { fetchAndParseMkvHeader } from './utils/mkvParser';
import { DeviceInfoCard } from './components/DeviceInfoCard';
import { UrlInputBar } from './components/UrlInputBar';
import { NetworkProbeCard } from './components/NetworkProbeCard';
import { StreamMetadata } from './components/StreamMetadata';
import { PlayerHarness } from './components/PlayerHarness';
import { DiagnosticLog } from './components/DiagnosticLog';
import './App.css';

const DEFAULT_URL = 'http://172.16.50.14/Barbie.2023.1080p.x265.mkv';

export function App() {
  const [device, setDevice] = useState<DeviceInfo | null>(null);
  const [codecs, setCodecs] = useState<CodecSupportMatrix | null>(null);

  const [testUrl, setTestUrl] = useState<string>('');
  const [isProbing, setIsProbing] = useState<boolean>(false);

  const [networkProbe, setNetworkProbe] = useState<NetworkProbeResult | null>(null);
  const [ebmlHeader, setEbmlHeader] = useState<EbmlHeaderInfo | null>(null);
  const [ebmlError, setEbmlError] = useState<string | undefined>(undefined);

  const [playbackMetrics, setPlaybackMetrics] = useState<PlaybackMetrics>({
    stage: 'idle',
    timeToMetadataMs: null,
    timeToFirstFrameMs: null,
    timeToPlayingMs: null,
    seekLatencyMs: null,
    videoWidth: null,
    videoHeight: null,
    duration: null,
    currentTime: 0,
    bufferedEnd: null,
    hasAudioActivity: false,
    audioDecibelLevel: -100,
    nativeError: null,
  });

  const [events, setEvents] = useState<MediaEventLog[]>([]);

  // Refs for current diagnostic snapshot
  const stateRef = useRef({
    testUrl,
    device,
    codecs,
    networkProbe,
    ebmlHeader,
    playbackMetrics,
    events,
  });
  stateRef.current = {
    testUrl,
    device,
    codecs,
    networkProbe,
    ebmlHeader,
    playbackMetrics,
    events,
  };

  // 1. Initial Device & Codec Detection on Mount
  useEffect(() => {
    async function initEnvironment() {
      const devInfo = await detectDevice();
      setDevice(devInfo);

      const codecMatrix = await probeCodecSupport();
      setCodecs(codecMatrix);
    }
    initEnvironment();
  }, []);

  // 2. Run Test Pipeline on Submitted URL
  const handleRunTest = useCallback(async (url: string) => {
    setTestUrl(url);
    setIsProbing(true);
    setNetworkProbe(null);
    setEbmlHeader(null);
    setEbmlError(undefined);
    setEvents([]);

    // Step A: Probe Network & HTTP Range Support
    const netResult = await probeNetworkAndRange(url);
    setNetworkProbe(netResult);

    // Step B: In-Browser MKV EBML Header Demuxing
    try {
      const headerInfo = await fetchAndParseMkvHeader(url, 512 * 1024);
      setEbmlHeader(headerInfo);
    } catch (err: unknown) {
      setEbmlError(err instanceof Error ? err.message : String(err));
    } finally {
      setIsProbing(false);
    }
  }, []);

  const handleMediaEvent = useCallback((event: MediaEventLog) => {
    setEvents((prev) => [event, ...prev.slice(0, 49)]); // keep recent 50 events
  }, []);

  const handleMetricsUpdate = useCallback((metrics: PlaybackMetrics) => {
    setPlaybackMetrics(metrics);
  }, []);

  const handleClearLogs = useCallback(() => {
    setEvents([]);
  }, []);

  const generateReport = useCallback((): CompleteDiagnosticReport => {
    const s = stateRef.current;
    return {
      timestamp: new Date().toISOString(),
      url: s.testUrl,
      device: s.device || ({} as DeviceInfo),
      codecs: s.codecs || {},
      network: s.networkProbe,
      container: s.ebmlHeader,
      playback: s.playbackMetrics,
      events: s.events,
    };
  }, []);

  return (
    <div className="app-container">
      <header className="app-header">
        <div className="header-brand">
          <span className="brand-badge">BDIXStream Lab</span>
          <h1 className="brand-title">Client-Native Media Compatibility Tester</h1>
        </div>
        <p className="brand-subtitle">
          Direct client-to-origin range streaming diagnostic tool for Android, iOS Safari & Chromium
        </p>
      </header>

      <main className="app-main">
        {/* Device & Engine Status */}
        <DeviceInfoCard device={device} codecs={codecs} />

        {/* Target URL Input Bar */}
        <UrlInputBar
          initialUrl={DEFAULT_URL}
          onRunTest={handleRunTest}
          isRunning={isProbing}
        />

        {/* Diagnostics & Playback Viewport */}
        {testUrl && (
          <div className="test-workspace">
            {/* Network / Range Probe */}
            <NetworkProbeCard probe={networkProbe} isLoading={isProbing} />

            {/* In-Browser Container Demuxing */}
            <StreamMetadata
              header={ebmlHeader}
              isLoading={isProbing && !ebmlHeader && !ebmlError}
              error={ebmlError}
            />

            {/* Native Video Playback Harness */}
            <PlayerHarness
              url={testUrl}
              onEvent={handleMediaEvent}
              onMetricsUpdate={handleMetricsUpdate}
            />

            {/* Event Log & 1-Click Export */}
            <DiagnosticLog
              events={events}
              reportGenerator={generateReport}
              onClear={handleClearLogs}
            />
          </div>
        )}
      </main>

      <footer className="app-footer">
        <span>BDIXStream v0.2.0 • Native-First Architecture • 100% Client-Side & Vercel-Ready</span>
      </footer>
    </div>
  );
}

export default App;
