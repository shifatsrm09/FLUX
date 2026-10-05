import path from 'node:path';
import os from 'node:os';

/**
 * Configuration for the HLS VOD streaming engine.
 * Values can be configured via environment variables.
 */
export const HLS_CONFIG = {
  /** Target duration for each HLS media segment in seconds (default: 6s) */
  segmentDuration: parseInt(process.env.HLS_SEGMENT_DURATION || '6', 10),

  /** Temporary directory for storing session segments */
  tempDir: process.env.HLS_TEMP_DIR || path.join(os.tmpdir(), 'bdixstream'),

  /** Session inactivity time-to-live in milliseconds (default: 30 minutes) */
  sessionTtlMs: parseInt(process.env.HLS_SESSION_TTL_MINUTES || '30', 10) * 60 * 1000,

  /** Maximum active concurrent sessions allowed */
  maxActiveSessions: parseInt(process.env.MAX_ACTIVE_SESSIONS || '10', 10),

  /** Maximum concurrent FFmpeg segment workers allowed across all sessions */
  maxConcurrentFfmpegProcesses: parseInt(process.env.MAX_CONCURRENT_FFMPEG_PROCESSES || '5', 10),

  /** Maximum concurrent FFmpeg segment workers allowed per session */
  maxConcurrentWorkersPerSession: parseInt(process.env.MAX_WORKERS_PER_SESSION || '3', 10),

  /** Maximum segments kept in temporary cache per session before pruning oldest */
  maxSessionSegments: parseInt(process.env.MAX_SESSION_SEGMENTS || '60', 10),

  /** Number of segments to proactively generate ahead of the current playhead */
  bufferAheadSegments: parseInt(process.env.BUFFER_AHEAD_SEGMENTS || '8', 10),

  /** Forward window size in segments for each seek worker */
  windowSegments: parseInt(process.env.HLS_WINDOW_SEGMENTS || '12', 10),

  /** Timeout in ms waiting for a segment file to be produced by FFmpeg (default: 25s) */
  segmentWaitTimeoutMs: 25_000,
};
