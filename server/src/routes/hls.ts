import { FastifyInstance, FastifyRequest, FastifyReply } from 'fastify';
import fs from 'node:fs';
import path from 'node:path';
import { hlsSessionManager } from '../services/hlsSession.js';
import { isValidMediaUrl } from '../utils/validation.js';

interface CreateSessionBody {
  url: string;
}

interface SessionParams {
  sessionId: string;
}

interface SegmentParams {
  sessionId: string;
  segment: string;
}

const UUID_REGEX = /^[0-9a-fA-F-]{36}$/;
const SEGMENT_REGEX = /^(init\.mp4|segment_\d{4}\.m4s)$/;

export async function hlsRoutes(app: FastifyInstance) {
  /**
   * POST /api/hls/session
   * Create or retrieve an HLS streaming session for an authorized media URL.
   */
  app.post<{ Body: CreateSessionBody }>(
    '/session',
    async (request: FastifyRequest<{ Body: CreateSessionBody }>, reply: FastifyReply) => {
      const { url } = request.body ?? {};

      if (!url || typeof url !== 'string') {
        return reply.status(400).send({
          error: 'Missing or invalid "url" in request body.',
          code: 'INVALID_URL',
        });
      }

      if (!isValidMediaUrl(url)) {
        return reply.status(400).send({
          error: 'The provided URL is not a valid HTTP/HTTPS media URL.',
          code: 'INVALID_URL',
        });
      }

      try {
        const session = await hlsSessionManager.createSession(url, request.log);

        return reply.send({
          sessionId: session.sessionId,
          playlistUrl: `/api/hls/${session.sessionId}/index.m3u8`,
          duration: session.duration,
          segmentDuration: session.segmentDuration,
          totalSegments: session.totalSegments,
          metadata: session.metadata,
        });
      } catch (err: unknown) {
        const message = err instanceof Error ? err.message : 'Unknown session creation error';
        request.log.error({ err, url }, 'Failed to create HLS session');
        return reply.status(502).send({
          error: 'Failed to initialize HLS streaming session.',
          detail: message,
          code: 'FFMPEG_FAILED',
        });
      }
    }
  );

  /**
   * GET /api/hls/:sessionId/index.m3u8
   * Serve the VOD HLS playlist.
   */
  app.get<{ Params: SessionParams }>(
    '/:sessionId/index.m3u8',
    async (request: FastifyRequest<{ Params: SessionParams }>, reply: FastifyReply) => {
      const { sessionId } = request.params;

      if (!UUID_REGEX.test(sessionId)) {
        return reply.status(400).send({ error: 'Invalid session ID format.', code: 'INVALID_SESSION' });
      }

      const session = hlsSessionManager.getSession(sessionId);
      if (!session) {
        return reply.status(404).send({ error: 'HLS session not found or expired.', code: 'SESSION_EXPIRED' });
      }

      const playlistPath = path.join(session.sessionDir, 'index.m3u8');
      if (!fs.existsSync(playlistPath)) {
        return reply.status(503).send({ error: 'HLS playlist not yet generated.', code: 'PLAYLIST_NOT_READY' });
      }

      const playlistContent = await fs.promises.readFile(playlistPath, 'utf8');

      return reply
        .type('application/vnd.apple.mpegurl')
        .header('Cache-Control', 'no-cache, no-store, must-revalidate')
        .header('Access-Control-Allow-Origin', '*')
        .send(playlistContent);
    }
  );

  /**
   * GET /api/hls/:sessionId/init.mp4
   * Serve the initialization segment.
   */
  app.get<{ Params: SessionParams }>(
    '/:sessionId/init.mp4',
    async (request: FastifyRequest<{ Params: SessionParams }>, reply: FastifyReply) => {
      const { sessionId } = request.params;

      if (!UUID_REGEX.test(sessionId)) {
        return reply.status(400).send({ error: 'Invalid session ID format.', code: 'INVALID_SESSION' });
      }

      const session = hlsSessionManager.getSession(sessionId);
      if (!session) {
        return reply.status(404).send({ error: 'HLS session not found or expired.', code: 'SESSION_EXPIRED' });
      }

      const initPath = path.join(session.sessionDir, 'init.mp4');
      if (!fs.existsSync(initPath)) {
        return reply.status(404).send({ error: 'Initialization segment not found.', code: 'SEGMENT_NOT_FOUND' });
      }

      return reply
        .type('video/mp4')
        .header('Cache-Control', 'public, max-age=86400')
        .header('Access-Control-Allow-Origin', '*')
        .send(fs.createReadStream(initPath));
    }
  );

  /**
   * GET /api/hls/:sessionId/:segment
   * Serve a media segment, generating on-demand / seeking if needed.
   */
  app.get<{ Params: SegmentParams }>(
    '/:sessionId/:segment',
    async (request: FastifyRequest<{ Params: SegmentParams }>, reply: FastifyReply) => {
      const { sessionId, segment } = request.params;

      if (!UUID_REGEX.test(sessionId)) {
        return reply.status(400).send({ error: 'Invalid session ID format.', code: 'INVALID_SESSION' });
      }

      if (!SEGMENT_REGEX.test(segment)) {
        return reply.status(400).send({ error: 'Invalid segment filename format.', code: 'INVALID_SEGMENT' });
      }

      const session = hlsSessionManager.getSession(sessionId);
      if (!session) {
        return reply.status(404).send({ error: 'HLS session not found or expired.', code: 'SESSION_EXPIRED' });
      }

      const startTime = Date.now();
      try {
        const segmentPath = await hlsSessionManager.getOrGenerateSegment(session, segment, request.log);
        const elapsedMs = Date.now() - startTime;

        request.log.info(
          { sessionId, segment, elapsedMs },
          'Served HLS segment'
        );

        return reply
          .type('video/iso.segment')
          .header('Cache-Control', 'public, max-age=86400')
          .header('Access-Control-Allow-Origin', '*')
          .send(fs.createReadStream(segmentPath));
      } catch (err: unknown) {
        const message = err instanceof Error ? err.message : 'Unknown segment retrieval error';
        request.log.error({ err, sessionId, segment, elapsedMs: Date.now() - startTime }, 'Failed to serve HLS segment');
        return reply.status(404).send({
          error: 'Segment not found or generation failed.',
          detail: message,
          code: 'SEGMENT_NOT_FOUND',
        });
      }
    }
  );

  /**
   * GET /api/hls/:sessionId/status
   * Debugging / session diagnostics.
   */
  app.get<{ Params: SessionParams }>(
    '/:sessionId/status',
    async (request: FastifyRequest<{ Params: SessionParams }>, reply: FastifyReply) => {
      const { sessionId } = request.params;

      if (!UUID_REGEX.test(sessionId)) {
        return reply.status(400).send({ error: 'Invalid session ID format.' });
      }

      const session = hlsSessionManager.getSession(sessionId);
      if (!session) {
        return reply.status(404).send({ error: 'Session not found or expired.' });
      }

      const files = await fs.promises.readdir(session.sessionDir).catch(() => []);
      const cachedSegments = files.filter((f) => /^segment_\d{4}\.m4s$/.test(f));

      const workersList = Array.from(session.workers.values()).map((w) => ({
        workerId: w.workerId,
        pid: w.pid,
        startSegment: w.startSegment,
        endSegment: w.endSegment,
        activeRequests: w.activeRequests,
        isFinished: w.isFinished,
        runningTimeMs: Date.now() - w.startedAt,
      }));

      return reply.send({
        sessionId: session.sessionId,
        isReady: session.isReady,
        duration: session.duration,
        segmentDuration: session.segmentDuration,
        totalSegments: session.totalSegments,
        cachedSegmentCount: cachedSegments.length,
        workers: workersList,
        activeWorker: session.activeWorker
          ? {
              pid: session.activeWorker.pid,
              startSegment: session.activeWorker.startSegment,
              runningTimeMs: Date.now() - session.activeWorker.startedAt,
            }
          : null,
        idleMs: Date.now() - session.lastActivityAt,
      });
    }
  );

  /**
   * DELETE /api/hls/:sessionId
   * Explicitly destroy session and free all temporary resources.
   */
  app.delete<{ Params: SessionParams }>(
    '/:sessionId',
    async (request: FastifyRequest<{ Params: SessionParams }>, reply: FastifyReply) => {
      const { sessionId } = request.params;

      if (!UUID_REGEX.test(sessionId)) {
        return reply.status(400).send({ error: 'Invalid session ID format.' });
      }

      await hlsSessionManager.deleteSession(sessionId, request.log);
      return reply.send({ success: true });
    }
  );
}
