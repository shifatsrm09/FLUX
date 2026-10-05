export interface MediaMetadata {
  filename: string;
  container: string;
  format: string;
  duration: string | null;
  durationSeconds: number | null;
  fileSize: string | null;
  fileSizeBytes: number | null;
  bitrate: string | null;

  video: {
    codec: string | null;
    profile: string | null;
    width: number | null;
    height: number | null;
    frameRate: string | null;
    bitrate: string | null;
    pixelFormat: string | null;
  } | null;

  audio: {
    codec: string | null;
    profile: string | null;
    channels: number | null;
    channelLayout: string | null;
    sampleRate: string | null;
    bitrate: string | null;
    language: string | null;
  } | null;

  audioTracks: {
    index: number;
    codec: string | null;
    channels: number | null;
    channelLayout: string | null;
    sampleRate: string | null;
    language: string | null;
  }[];

  subtitles: {
    index: number;
    codec: string | null;
    language: string | null;
    title: string | null;
  }[];

  directPlayCandidate: boolean;
}

export interface HlsSessionResponse {
  sessionId: string;
  playlistUrl: string;
  duration: number;
  segmentDuration: number;
  totalSegments: number;
  metadata?: MediaMetadata;
}

