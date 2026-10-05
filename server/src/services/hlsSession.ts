import { spawn, ChildProcess } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import fsp from 'node:fs/promises';
import path from 'node:path';
import { FastifyBaseLogger } from 'fastify';
import { HLS_CONFIG } from './hlsConfig.js';
import { probeMedia } from './ffprobe.js';
import { normalizeMetadata, MediaMetadata } from './metadata.js';
import { sanitizeUrlForLog } from './ffmpeg.js';

const FFMPEG_PATH = process.env.FFMPEG_PATH || 'ffmpeg';

export interface HlsWorkerInfo {
  workerId: string;
  process: ChildProcess;
  startSegment: number;
  endSegment: number;
  pid: number | undefined;
  startedAt: number;
  activeRequests: number;
  isFinished: boolean;
  exitCode: number | null;
  exitSignal: NodeJS.Signals | null;
  stderr: string;
}

export interface HlsSession {
  sessionId: string;
  sourceUrl: string;
  sessionDir: string;
  createdAt: number;
  lastActivityAt: number;
  metadata: MediaMetadata;
  duration: number; // in seconds
  segmentDuration: number;
  totalSegments: number;

  workers: Map<string, HlsWorkerInfo>;
  pendingSegmentPromises: Map<string, Promise<string>>;
  isReady: boolean;
  isCleaningUp: boolean;

  /** Backwards compatibility getter for single active worker */
  readonly activeWorker: HlsWorkerInfo | null;
}

export class HlsSessionManager {
  private sessions = new Map<string, HlsSession>();
  private sweepTimer: NodeJS.Timeout | null = null;
  private isInitialized = false;

  /**
   * Initialize HLS engine: clean up any stale temp directories and start TTL sweep.
   */
  async init(logger?: FastifyBaseLogger): Promise<void> {
    if (this.isInitialized) return;
    this.isInitialized = true;

    try {
      if (fs.existsSync(HLS_CONFIG.tempDir)) {
        logger?.info({ tempDir: HLS_CONFIG.tempDir }, 'Cleaning up stale HLS temporary directories from previous runs');
        const entries = await fsp.readdir(HLS_CONFIG.tempDir, { withFileTypes: true });
        for (const entry of entries) {
          if (entry.isDirectory()) {
            const dirPath = path.join(HLS_CONFIG.tempDir, entry.name);
            await fsp.rm(dirPath, { recursive: true, force: true }).catch(() => {});
          }
        }
      } else {
        await fsp.mkdir(HLS_CONFIG.tempDir, { recursive: true });
      }
    } catch (err) {
      logger?.warn({ err }, 'Warning during initial HLS temp directory cleanup');
    }

    // Run periodic sweep every 60 seconds
    this.sweepTimer = setInterval(() => {
      this.sweepStaleSessions(logger);
    }, 60_000);
    this.sweepTimer.unref();
  }

  /**
   * Graceful shutdown: terminate all FFmpeg processes and delete temp files.
   */
  async shutdown(logger?: FastifyBaseLogger): Promise<void> {
    if (this.sweepTimer) {
      clearInterval(this.sweepTimer);
      this.sweepTimer = null;
    }

    logger?.info({ sessionCount: this.sessions.size }, 'Shutting down HLS session manager');

    const sessionList = Array.from(this.sessions.values());
    for (const session of sessionList) {
      await this.deleteSession(session.sessionId, logger).catch(() => {});
    }

    try {
      if (fs.existsSync(HLS_CONFIG.tempDir)) {
        await fsp.rm(HLS_CONFIG.tempDir, { recursive: true, force: true }).catch(() => {});
      }
    } catch {
      // ignore
    }
  }

  /**
   * Create or return an existing HLS session for an authorized media URL.
   */
  async createSession(url: string, logger: FastifyBaseLogger): Promise<HlsSession> {
    await this.init(logger);

    // Reuse existing active session for this URL if available and healthy
    for (const s of this.sessions.values()) {
      if (s.sourceUrl === url && s.isReady && !s.isCleaningUp) {
        s.lastActivityAt = Date.now();
        logger.info({ sessionId: s.sessionId, sourceUrl: sanitizeUrlForLog(url) }, 'Reusing existing HLS session');
        return s;
      }
    }

    // Enforce max active sessions by cleaning oldest inactive session if needed
    if (this.sessions.size >= HLS_CONFIG.maxActiveSessions) {
      this.pruneOldestSession(logger);
    }

    const sessionId = crypto.randomUUID();
    const sessionDir = path.join(HLS_CONFIG.tempDir, sessionId);
    await fsp.mkdir(sessionDir, { recursive: true });

    logger.info({ sessionId, sourceUrl: sanitizeUrlForLog(url) }, 'Creating new HLS VOD session');

    // Probe media for metadata and exact duration
    const rawProbe = await probeMedia(url);
    const metadata = normalizeMetadata(rawProbe, url);

    const duration = metadata.durationSeconds;
    if (!duration || duration <= 0) {
      await fsp.rm(sessionDir, { recursive: true, force: true }).catch(() => {});
      throw new Error('Unable to determine media duration; cannot construct HLS VOD playlist');
    }

    const segmentDuration = HLS_CONFIG.segmentDuration;
    const totalSegments = Math.ceil(duration / segmentDuration);

    const workers = new Map<string, HlsWorkerInfo>();
    const pendingSegmentPromises = new Map<string, Promise<string>>();

    const session: HlsSession = {
      sessionId,
      sourceUrl: url,
      sessionDir,
      createdAt: Date.now(),
      lastActivityAt: Date.now(),
      metadata,
      duration,
      segmentDuration,
      totalSegments,
      workers,
      pendingSegmentPromises,
      isReady: false,
      isCleaningUp: false,
      get activeWorker(): HlsWorkerInfo | null {
        for (const w of workers.values()) {
          if (!w.isFinished && !w.process.killed) return w;
        }
        return null;
      },
    };

    this.sessions.set(sessionId, session);

    try {
      // 1. Generate master init.mp4 and initial segment(s)
      await this.generateInitialHlsAssets(session, logger);

      // 2. Generate complete VOD index.m3u8 playlist with full duration
      await this.writeMasterVodPlaylist(session);

      session.isReady = true;
      logger.info(
        { sessionId, duration, totalSegments, segmentDuration },
        'HLS VOD session initialized successfully'
      );
      return session;
    } catch (err) {
      logger.error({ err, sessionId }, 'Failed to initialize HLS session assets');
      await this.deleteSession(sessionId, logger).catch(() => {});
      throw err;
    }
  }

  /**
   * Get an active session by ID.
   */
  getSession(sessionId: string): HlsSession | undefined {
    const session = this.sessions.get(sessionId);
    if (session && !session.isCleaningUp) {
      session.lastActivityAt = Date.now();
      return session;
    }
    return undefined;
  }

  /**
   * Get all active sessions (for status/debugging).
   */
  getAllSessions(): HlsSession[] {
    return Array.from(this.sessions.values());
  }

  /**
   * Terminate and delete a session.
   */
  async deleteSession(sessionId: string, logger?: FastifyBaseLogger): Promise<void> {
    const session = this.sessions.get(sessionId);
    if (!session) return;

    session.isCleaningUp = true;
    this.sessions.delete(sessionId);

    logger?.info({ sessionId }, 'Deleting HLS session');

    // Terminate all workers
    for (const worker of session.workers.values()) {
      this.terminateWorker(worker, logger);
    }
    session.workers.clear();
    session.pendingSegmentPromises.clear();

    // Delete session files (with retry for Windows process file handle release)
    try {
      await fsp.rm(session.sessionDir, { recursive: true, force: true, maxRetries: 5, retryDelay: 100 });
    } catch (err) {
      logger?.warn({ err, sessionId }, 'Error removing session directory');
    }
  }

  /**
   * Ensure a specific segment is available and return its absolute path.
   * Concurrent requests for the same segment are deduplicated.
   * Neighboring segment requests utilize running workers without termination.
   */
  async getOrGenerateSegment(
    session: HlsSession,
    segmentFilename: string,
    logger: FastifyBaseLogger
  ): Promise<string> {
    session.lastActivityAt = Date.now();

    // Check if it's the initialization segment
    if (segmentFilename === 'init.mp4') {
      const initPath = path.join(session.sessionDir, 'init.mp4');
      if (this.isFileReady(initPath)) {
        return initPath;
      }
      throw new Error('HLS initialization segment not found');
    }

    // Parse segment index (e.g. "segment_0042.m4s" -> 42)
    const match = segmentFilename.match(/^segment_(\d{4})\.m4s$/);
    if (!match) {
      throw new Error(`Invalid segment filename format: ${segmentFilename}`);
    }

    const segmentIndex = parseInt(match[1], 10);
    if (segmentIndex < 0 || segmentIndex >= session.totalSegments) {
      throw new Error(`Segment index out of range (0-${session.totalSegments - 1}): ${segmentIndex}`);
    }

    const segmentPath = path.join(session.sessionDir, segmentFilename);

    // Fast path: file already exists and is fully written
    if (this.isFileReady(segmentPath)) {
      return segmentPath;
    }

    // Deduplication path: if this segment is already being generated/awaited, join the pending promise
    const existingPromise = session.pendingSegmentPromises.get(segmentFilename);
    if (existingPromise) {
      logger.debug(
        { sessionId: session.sessionId, segmentFilename, segmentIndex },
        'Attaching to existing pending segment promise'
      );
      return existingPromise;
    }

    // Create a new promise to produce/wait for this segment
    const segmentPromise = (async () => {
      try {
        // Check if ANY active worker has a forward window covering this segment
        let targetWorker = Array.from(session.workers.values()).find(
          (w) =>
            w.startSegment <= segmentIndex &&
            segmentIndex < w.endSegment &&
            !w.isFinished &&
            !w.process.killed
        );

        if (!targetWorker) {
          // No active worker covers this segment. Clean up idle workers before spawning a new one.
          this.reapIdleWorkers(session, logger);

          logger.info(
            { sessionId: session.sessionId, requestedSegment: segmentIndex },
            'Spawning new seek worker for segment window'
          );

          targetWorker = await this.startSegmentWorker(session, segmentIndex, logger);
        } else {
          logger.debug(
            {
              sessionId: session.sessionId,
              requestedSegment: segmentIndex,
              workerId: targetWorker.workerId,
              workerStart: targetWorker.startSegment,
              workerEnd: targetWorker.endSegment,
            },
            'Reusing running worker covering segment window'
          );
        }

        // Increment in-flight request counter for this worker
        targetWorker.activeRequests++;

        try {
          await this.waitForSegmentFile(segmentPath, targetWorker, HLS_CONFIG.segmentWaitTimeoutMs, logger);
          return segmentPath;
        } finally {
          targetWorker.activeRequests = Math.max(0, targetWorker.activeRequests - 1);
        }
      } finally {
        session.pendingSegmentPromises.delete(segmentFilename);
      }
    })();

    session.pendingSegmentPromises.set(segmentFilename, segmentPromise);

    const finalPath = await segmentPromise;

    // Prune distant cached segments asynchronously
    this.pruneDistantSegments(session, segmentIndex, logger).catch(() => {});

    return finalPath;
  }

  // -------------------------------------------------------------------------
  // Internal Helpers
  // -------------------------------------------------------------------------

  /**
   * Generate master init.mp4 and first segment using FFmpeg.
   */
  private async generateInitialHlsAssets(session: sessionType, logger: FastifyBaseLogger): Promise<void> {
    const initPath = path.join(session.sessionDir, 'init.mp4');
    const tempM3u8 = path.join(session.sessionDir, 'init_worker.m3u8');
    const isHevc = /hevc|h265/i.test(session.metadata.video?.codec || '');

    // Generate first 2 segments to seed playback immediately
    const stopTime = session.segmentDuration * 2;

    const args = [
      '-nostdin',
      '-loglevel', 'warning',
      '-ss', '0',
      '-i', session.sourceUrl,
      '-map', '0:v:0',
      '-map', '0:a:0?',
      '-c:v', 'copy',
      ...(isHevc ? ['-tag:v', 'hvc1'] : []),
      '-c:a', 'copy',
      '-f', 'hls',
      '-hls_time', String(session.segmentDuration),
      '-hls_segment_type', 'fmp4',
      '-hls_fmp4_init_filename', 'init.mp4',
      '-start_number', '0',
      '-hls_segment_filename', 'segment_%04d.m4s',
      '-hls_flags', 'independent_segments+temp_file',
      '-to', String(stopTime),
      'init_worker.m3u8',
    ];

    logger.info({ sessionId: session.sessionId, isHevc, stopTime }, 'Generating master init.mp4 and initial segments');

    await new Promise<void>((resolve, reject) => {
      let child: ChildProcess;
      try {
        child = spawn(FFMPEG_PATH, args, {
          cwd: session.sessionDir,
          stdio: ['ignore', 'ignore', 'pipe'],
        });
      } catch (err) {
        return reject(new Error(`Failed to spawn FFmpeg for init segment: ${err}`));
      }

      let stderr = '';
      child.stderr?.on('data', (d) => {
        stderr = (stderr + d.toString()).slice(-4096);
      });

      child.on('error', (err) => {
        reject(new Error(`FFmpeg init error: ${err.message}`));
      });

      child.on('close', (code) => {
        if (code === 0 && this.isFileReady(initPath)) {
          resolve();
        } else {
          reject(new Error(`FFmpeg init failed with code ${code}: ${stderr.trim() || 'init.mp4 missing'}`));
        }
      });
    });

    // Remove the temporary init_worker.m3u8
    await fsp.rm(tempM3u8, { force: true }).catch(() => {});
  }

  /**
   * Write full VOD index.m3u8 playlist covering the complete duration from FFprobe.
   */
  private async writeMasterVodPlaylist(session: HlsSession): Promise<void> {
    const lines: string[] = [
      '#EXTM3U',
      '#EXT-X-VERSION:7',
      `#EXT-X-TARGETDURATION:${Math.ceil(session.segmentDuration)}`,
      '#EXT-X-MEDIA-SEQUENCE:0',
      '#EXT-X-PLAYLIST-TYPE:VOD',
      '#EXT-X-INDEPENDENT-SEGMENTS',
      '#EXT-X-MAP:URI="init.mp4"',
    ];

    let remainingDuration = session.duration;

    for (let i = 0; i < session.totalSegments; i++) {
      const segDur = i === session.totalSegments - 1 ? remainingDuration : session.segmentDuration;
      const filename = `segment_${String(i).padStart(4, '0')}.m4s`;
      lines.push(`#EXTINF:${segDur.toFixed(6)},`);
      lines.push(filename);
      remainingDuration = Math.max(0, remainingDuration - session.segmentDuration);
    }

    lines.push('#EXT-X-ENDLIST');
    lines.push('');

    const playlistPath = path.join(session.sessionDir, 'index.m3u8');
    await fsp.writeFile(playlistPath, lines.join('\n'), 'utf8');
  }

  /**
   * Spawn a new FFmpeg worker starting at startSegment, generating a contiguous window.
   */
  private async startSegmentWorker(
    session: HlsSession,
    startSegment: number,
    logger: FastifyBaseLogger
  ): Promise<HlsWorkerInfo> {
    const startTime = startSegment * session.segmentDuration;
    const endSegment = Math.min(startSegment + HLS_CONFIG.windowSegments, session.totalSegments);
    const stopTime = endSegment * session.segmentDuration;

    const workerId = `w_${startSegment}_${Date.now()}`;
    const workerM3u8 = `worker_${workerId}.m3u8`;
    const workerInit = `worker_${workerId}_init.mp4`;
    const isHevc = /hevc|h265/i.test(session.metadata.video?.codec || '');

    const args = [
      '-nostdin',
      '-loglevel', 'warning',
      '-ss', String(startTime),
      '-copyts',
      '-i', session.sourceUrl,
      '-map', '0:v:0',
      '-map', '0:a:0?',
      '-c:v', 'copy',
      ...(isHevc ? ['-tag:v', 'hvc1'] : []),
      '-c:a', 'copy',
      '-f', 'hls',
      '-hls_time', String(session.segmentDuration),
      '-hls_segment_type', 'fmp4',
      '-hls_fmp4_init_filename', workerInit,
      '-start_number', String(startSegment),
      '-hls_segment_filename', 'segment_%04d.m4s',
      '-hls_flags', 'independent_segments+temp_file',
      '-to', String(stopTime),
      workerM3u8,
    ];

    logger.info(
      { sessionId: session.sessionId, workerId, startSegment, endSegment, startTime, stopTime, isHevc },
      'Spawning seek/playback segment worker'
    );

    let child: ChildProcess;
    try {
      child = spawn(FFMPEG_PATH, args, {
        cwd: session.sessionDir,
        stdio: ['ignore', 'ignore', 'pipe'],
      });
    } catch (err) {
      logger.error({ err, sessionId: session.sessionId }, 'Failed to spawn segment worker');
      throw err;
    }

    const workerInfo: HlsWorkerInfo = {
      workerId,
      process: child,
      startSegment,
      endSegment,
      pid: child.pid,
      startedAt: Date.now(),
      activeRequests: 0,
      isFinished: false,
      exitCode: null,
      exitSignal: null,
      stderr: '',
    };

    session.workers.set(workerId, workerInfo);

    child.stderr?.on('data', (d) => {
      workerInfo.stderr = (workerInfo.stderr + d.toString()).slice(-4096);
    });

    child.on('close', (code, signal) => {
      workerInfo.isFinished = true;
      workerInfo.exitCode = code;
      workerInfo.exitSignal = signal;

      logger.info(
        {
          sessionId: session.sessionId,
          workerId,
          pid: workerInfo.pid,
          startSegment,
          endSegment,
          code,
          signal,
          stderr: workerInfo.stderr.trim(),
        },
        'Segment worker finished'
      );

      // Clean up temporary files associated with this worker
      fsp.rm(path.join(session.sessionDir, workerM3u8), { force: true }).catch(() => {});
      fsp.rm(path.join(session.sessionDir, workerInit), { force: true }).catch(() => {});
    });

    return workerInfo;
  }

  /**
   * Reap idle workers when maximum worker capacity is exceeded or workers have completed.
   * A worker is NEVER reaped if activeRequests > 0.
   */
  private reapIdleWorkers(session: HlsSession, logger?: FastifyBaseLogger): void {
    const workers = Array.from(session.workers.values());
    const runningWorkers = workers.filter((w) => !w.isFinished && !w.process.killed);

    // If running workers reach or exceed per-session limit, terminate oldest idle workers
    if (runningWorkers.length >= HLS_CONFIG.maxConcurrentWorkersPerSession) {
      const candidates = runningWorkers
        .filter((w) => w.activeRequests === 0)
        .sort((a, b) => a.startedAt - b.startedAt);

      for (const idle of candidates) {
        if (session.workers.size < HLS_CONFIG.maxConcurrentWorkersPerSession) break;
        logger?.info(
          { sessionId: session.sessionId, workerId: idle.workerId, pid: idle.pid, startSegment: idle.startSegment },
          'Terminating idle worker to free capacity'
        );
        this.terminateWorker(idle, logger);
        session.workers.delete(idle.workerId);
      }
    }

    // Clean up any finished workers that have no active requests
    for (const [id, w] of session.workers.entries()) {
      if (w.isFinished && w.activeRequests === 0) {
        session.workers.delete(id);
      }
    }
  }

  /**
   * Terminate a worker process cleanly.
   */
  private terminateWorker(worker: HlsWorkerInfo, logger?: FastifyBaseLogger): void {
    if (worker.process.killed) return;
    const pid = worker.pid;
    logger?.info({ pid, workerId: worker.workerId, startSegment: worker.startSegment }, 'Terminating segment worker');

    try {
      worker.process.kill('SIGTERM');
      setTimeout(() => {
        if (!worker.process.killed) {
          worker.process.kill('SIGKILL');
        }
      }, 1000).unref();
    } catch (err) {
      logger?.warn({ err, pid }, 'Error while killing worker process');
    }
  }

  /**
   * Wait for a segment file to exist, have no .tmp extension, and have size > 100 bytes.
   */
  private async waitForSegmentFile(
    filePath: string,
    worker: HlsWorkerInfo,
    timeoutMs: number,
    logger?: FastifyBaseLogger
  ): Promise<void> {
    const start = Date.now();

    while (Date.now() - start < timeoutMs) {
      if (this.isFileReady(filePath)) {
        return;
      }

      // If worker died prematurely without creating the file, fail fast
      if (worker.isFinished && !this.isFileReady(filePath)) {
        const detail = worker.stderr ? ` FFmpeg stderr: ${worker.stderr.trim()}` : '';
        throw new Error(
          `Worker exited with code ${worker.exitCode} before producing ${path.basename(filePath)}.${detail}`
        );
      }

      await new Promise((r) => setTimeout(r, 60));
    }

    if (this.isFileReady(filePath)) return;

    logger?.warn({ filePath, timeoutMs }, 'Timeout waiting for segment file');
    throw new Error(`Timeout (${timeoutMs}ms) waiting for segment to generate: ${path.basename(filePath)}`);
  }

  /**
   * Check if a file is ready to be served:
   * - Must exist
   * - Must NOT have a .tmp file alongside it (FFmpeg is still writing/renaming)
   * - Must have non-trivial size (> 100 bytes)
   */
  private isFileReady(filePath: string): boolean {
    try {
      if (!fs.existsSync(filePath)) return false;
      if (fs.existsSync(filePath + '.tmp')) return false;
      const stats = fs.statSync(filePath);
      return stats.size > 100;
    } catch {
      return false;
    }
  }

  /**
   * Prune segments furthest from current playhead to respect maxSessionSegments.
   */
  private async pruneDistantSegments(
    session: HlsSession,
    currentSegment: number,
    logger?: FastifyBaseLogger
  ): Promise<void> {
    try {
      const files = await fsp.readdir(session.sessionDir);
      const segmentFiles = files.filter((f) => /^segment_\d{4}\.m4s$/.test(f));

      if (segmentFiles.length <= HLS_CONFIG.maxSessionSegments) {
        return;
      }

      // Sort segments by distance from currentSegment (descending)
      const sortedByDistance = segmentFiles
        .map((f) => {
          const idx = parseInt(f.slice(8, 12), 10);
          return { filename: f, distance: Math.abs(idx - currentSegment), idx };
        })
        .sort((a, b) => b.distance - a.distance);

      const toRemove = sortedByDistance.slice(0, segmentFiles.length - HLS_CONFIG.maxSessionSegments);
      for (const item of toRemove) {
        // Protect segments close to current playhead
        if (item.distance > 4) {
          await fsp.rm(path.join(session.sessionDir, item.filename), { force: true }).catch(() => {});
        }
      }

      logger?.debug(
        { sessionId: session.sessionId, prunedCount: toRemove.length, remaining: segmentFiles.length - toRemove.length },
        'Pruned distant HLS segments to respect cache limit'
      );
    } catch {
      // ignore pruning errors
    }
  }

  /**
   * Prune the oldest inactive session when max active sessions limit is reached.
   */
  private pruneOldestSession(logger: FastifyBaseLogger): void {
    let oldest: HlsSession | null = null;
    for (const session of this.sessions.values()) {
      if (!session.isCleaningUp) {
        if (!oldest || session.lastActivityAt < oldest.lastActivityAt) {
          oldest = session;
        }
      }
    }

    if (oldest) {
      logger.info({ sessionId: oldest.sessionId }, 'Pruning oldest inactive session due to capacity limit');
      this.deleteSession(oldest.sessionId, logger).catch(() => {});
    }
  }

  /**
   * Periodic sweep: remove sessions with no activity past HLS_SESSION_TTL.
   */
  private sweepStaleSessions(logger?: FastifyBaseLogger): void {
    const now = Date.now();
    for (const session of this.sessions.values()) {
      if (!session.isCleaningUp && now - session.lastActivityAt > HLS_CONFIG.sessionTtlMs) {
        logger?.info({ sessionId: session.sessionId, idleMs: now - session.lastActivityAt }, 'Session expired by TTL; cleaning up');
        this.deleteSession(session.sessionId, logger).catch(() => {});
      }
    }
  }
}

type sessionType = HlsSession;

export const hlsSessionManager = new HlsSessionManager();
