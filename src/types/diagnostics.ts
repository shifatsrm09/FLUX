export interface DeviceInfo {
  userAgent: string;
  browserName: string;
  browserVersion: string;
  osName: string;
  isMobile: boolean;
  isIOS: boolean;
  isAndroid: boolean;
  isSafari: boolean;
  isChromeOrChromium: boolean;
  isBrave: boolean;
  isFirefox: boolean;
  hasMediaSource: boolean;
  hasManagedMediaSource: boolean;
  hasWebAudio: boolean;
}

export interface CodecSupportMatrix {
  [mimeType: string]: {
    canPlayType: 'probably' | 'maybe' | 'no';
    mediaCapabilities?: {
      supported: boolean;
      smooth?: boolean;
      powerEfficient?: boolean;
    };
  };
}

export interface NetworkProbeResult {
  reachable: boolean;
  testedAt: number;
  httpStatus: number | null;
  statusText: string | null;
  acceptRanges: string | null;
  contentLength: number | null;
  contentRange: string | null;
  contentType: string | null;
  corsAllowed: boolean;
  mixedContentBlocked: boolean;
  latencyMs: number;
  error?: string;
}

export interface EbmlTrack {
  trackNumber: number;
  trackType: 'video' | 'audio' | 'subtitle' | 'unknown';
  codecId: string;
  codecName: string;
  name?: string;
  language?: string;
  isDefault?: boolean;
  // Video specific
  pixelWidth?: number;
  pixelHeight?: number;
  displayAspectRatio?: string;
  // Audio specific
  channels?: number;
  samplingFrequency?: number;
  bitDepth?: number;
}

export interface EbmlHeaderInfo {
  docType: string;
  docTypeVersion: number;
  parsedAt: number;
  bytesRead: number;
  tracks: EbmlTrack[];
  title?: string;
  durationSeconds?: number;
  rawHeaderDump?: string;
}

export type PlaybackStage =
  | 'idle'
  | 'loadstart'
  | 'metadata_loaded'
  | 'first_frame_ready'
  | 'canplay'
  | 'playing'
  | 'seeking'
  | 'seeked'
  | 'stalled'
  | 'error';

export interface MediaEventLog {
  timestamp: number;
  eventName: string;
  stage: PlaybackStage;
  details?: Record<string, unknown>;
}

export interface PlaybackMetrics {
  stage: PlaybackStage;
  timeToMetadataMs: number | null;
  timeToFirstFrameMs: number | null;
  timeToPlayingMs: number | null;
  seekLatencyMs: number | null;
  videoWidth: number | null;
  videoHeight: number | null;
  duration: number | null;
  currentTime: number;
  bufferedEnd: number | null;
  hasAudioActivity: boolean; // Detected non-zero audio samples via Web Audio Analyser
  audioDecibelLevel: number;
  nativeError: {
    code: number;
    name: string;
    message: string;
  } | null;
}

export interface CompleteDiagnosticReport {
  timestamp: string;
  url: string;
  device: DeviceInfo;
  codecs: CodecSupportMatrix;
  network: NetworkProbeResult | null;
  container: EbmlHeaderInfo | null;
  playback: PlaybackMetrics;
  events: MediaEventLog[];
}
