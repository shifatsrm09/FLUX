import { FFprobeResult } from './ffprobe.js';
import path from 'node:path';

/**
 * Normalized media metadata returned to the frontend.
 * Structured for display and future playback-decision logic.
 */
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

  /** Whether the browser can likely play this directly via <video> */
  directPlayCandidate: boolean;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function extractFilename(url: string): string {
  try {
    const parsed = new URL(url);
    return path.basename(decodeURIComponent(parsed.pathname)) || 'unknown';
  } catch {
    return 'unknown';
  }
}

function formatDuration(seconds: number): string {
  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = Math.floor(seconds % 60);
  return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
}

function formatFileSize(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 ** 2) return `${(bytes / 1024).toFixed(1)} KB`;
  if (bytes < 1024 ** 3) return `${(bytes / 1024 ** 2).toFixed(1)} MB`;
  return `${(bytes / 1024 ** 3).toFixed(2)} GB`;
}

function formatBitrate(bps: number): string {
  if (bps < 1000) return `${bps} bps`;
  if (bps < 1_000_000) return `${(bps / 1000).toFixed(0)} Kbps`;
  return `${(bps / 1_000_000).toFixed(1)} Mbps`;
}

function safeNum(val: unknown): number | null {
  if (val === undefined || val === null || val === '' || val === 'N/A') return null;
  const n = Number(val);
  return Number.isFinite(n) ? n : null;
}

function safeStr(val: unknown): string | null {
  if (val === undefined || val === null || val === '' || val === 'N/A') return null;
  return String(val);
}

/**
 * Determine container name from ffprobe format_name.
 */
function containerName(formatName: string | null): string {
  if (!formatName) return 'Unknown';
  const lower = formatName.toLowerCase();
  if (lower.includes('matroska')) return 'MKV';
  if (lower.includes('mp4') || lower.includes('mov')) return 'MP4';
  if (lower.includes('avi')) return 'AVI';
  if (lower.includes('webm')) return 'WebM';
  if (lower.includes('mpegts')) return 'MPEG-TS';
  if (lower.includes('flv')) return 'FLV';
  return formatName.toUpperCase();
}

/**
 * Heuristic: Can the browser probably play this directly?
 * Browsers generally support MP4(H.264+AAC), WebM(VP8/VP9+Opus/Vorbis).
 */
function estimateDirectPlay(container: string, videoCodec: string | null, audioCodec: string | null): boolean {
  const vc = (videoCodec || '').toLowerCase();
  const ac = (audioCodec || '').toLowerCase();

  // MP4 container with H.264/H.265 video and AAC/MP3 audio
  if (container === 'MP4') {
    const videoOk = ['h264', 'avc', 'h265', 'hevc'].some(c => vc.includes(c));
    const audioOk = !ac || ['aac', 'mp3', 'mp4a'].some(c => ac.includes(c));
    return videoOk && audioOk;
  }

  // WebM container with VP8/VP9/AV1 and Opus/Vorbis
  if (container === 'WebM') {
    const videoOk = ['vp8', 'vp9', 'av1'].some(c => vc.includes(c));
    const audioOk = !ac || ['opus', 'vorbis'].some(c => ac.includes(c));
    return videoOk && audioOk;
  }

  // MKV and other containers are generally not directly playable
  return false;
}

// ---------------------------------------------------------------------------
// Main normalizer
// ---------------------------------------------------------------------------

export function normalizeMetadata(probe: FFprobeResult, url: string): MediaMetadata {
  const fmt = probe.format || {};
  const streams = probe.streams || [];

  const videoStream = streams.find((s: Record<string, unknown>) => s.codec_type === 'video');
  const audioStream = streams.find((s: Record<string, unknown>) => s.codec_type === 'audio');
  const audioStreams = streams.filter((s: Record<string, unknown>) => s.codec_type === 'audio');
  const subtitleStreams = streams.filter((s: Record<string, unknown>) => s.codec_type === 'subtitle');

  const durationSec = safeNum(fmt.duration);
  const fileSizeBytes = safeNum(fmt.size);
  const overallBitrate = safeNum(fmt.bit_rate);

  const container = containerName(safeStr(fmt.format_name));
  const videoCodec = videoStream ? safeStr(videoStream.codec_name) : null;
  const audioCodec = audioStream ? safeStr(audioStream.codec_name) : null;

  return {
    filename: extractFilename(url),
    container,
    format: safeStr(fmt.format_long_name) || safeStr(fmt.format_name) || 'Unknown',
    duration: durationSec !== null ? formatDuration(durationSec) : null,
    durationSeconds: durationSec,
    fileSize: fileSizeBytes !== null ? formatFileSize(fileSizeBytes) : null,
    fileSizeBytes,
    bitrate: overallBitrate !== null ? formatBitrate(overallBitrate) : null,

    video: videoStream
      ? {
          codec: safeStr(videoStream.codec_name),
          profile: safeStr(videoStream.profile),
          width: safeNum(videoStream.width),
          height: safeNum(videoStream.height),
          frameRate: safeStr(videoStream.r_frame_rate),
          bitrate: safeNum(videoStream.bit_rate) !== null ? formatBitrate(safeNum(videoStream.bit_rate)!) : null,
          pixelFormat: safeStr(videoStream.pix_fmt),
        }
      : null,

    audio: audioStream
      ? {
          codec: safeStr(audioStream.codec_name),
          profile: safeStr(audioStream.profile),
          channels: safeNum(audioStream.channels),
          channelLayout: safeStr(audioStream.channel_layout),
          sampleRate: safeStr(audioStream.sample_rate),
          bitrate: safeNum(audioStream.bit_rate) !== null ? formatBitrate(safeNum(audioStream.bit_rate)!) : null,
          language: safeStr((audioStream.tags as Record<string, unknown>)?.language ?? null),
        }
      : null,

    audioTracks: audioStreams.map((s: Record<string, unknown>, i: number) => ({
      index: i,
      codec: safeStr(s.codec_name),
      channels: safeNum(s.channels),
      channelLayout: safeStr(s.channel_layout),
      sampleRate: safeStr(s.sample_rate),
      language: safeStr((s.tags as Record<string, unknown>)?.language ?? null),
    })),

    subtitles: subtitleStreams.map((s: Record<string, unknown>, i: number) => ({
      index: i,
      codec: safeStr(s.codec_name),
      language: safeStr((s.tags as Record<string, unknown>)?.language ?? null),
      title: safeStr((s.tags as Record<string, unknown>)?.title ?? null),
    })),

    directPlayCandidate: estimateDirectPlay(container, videoCodec, audioCodec),
  };
}
