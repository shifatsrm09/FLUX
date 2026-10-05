import { FastifyInstance, FastifyRequest, FastifyReply } from 'fastify';
import { startRemuxStream, sanitizeUrlForLog } from '../services/ffmpeg.js';
import { isValidMediaUrl } from '../utils/validation.js';

interface StreamQuery {
  url: string;
}

export async function streamRoutes(app: FastifyInstance) {
  app.get<{ Querystring: StreamQuery }>(
    '/stream',
    async (request: FastifyRequest<{ Querystring: StreamQuery }>, reply: FastifyReply) => {
      const { url } = request.query ?? {};

      if (!url || typeof url !== 'string') {
        return reply.status(400).send({
          error: 'Missing or invalid "url" query parameter.',
        });
      }

      if (!isValidMediaUrl(url)) {
        return reply.status(400).send({
          error: 'The provided URL is not a valid HTTP/HTTPS media URL.',
        });
      }

      const safeUrl = sanitizeUrlForLog(url);
      request.log.info({ sourceUrl: safeUrl }, 'Incoming media stream request');

      try {
        const { mediaStream, cleanup, pid } = await startRemuxStream(url, request.log);

        // When the client disconnects, terminates playback, or navigates away
        request.raw.on('close', () => {
          request.log.info({ pid, sourceUrl: safeUrl }, 'Client HTTP connection closed; terminating stream');
          cleanup();
        });

        return reply
          .type('video/mp4')
          .header('Cache-Control', 'no-cache, no-store, must-revalidate')
          .header('Pragma', 'no-cache')
          .header('Expires', '0')
          .header('Connection', 'keep-alive')
          .send(mediaStream);
      } catch (err: unknown) {
        const message = err instanceof Error ? err.message : 'Unknown FFmpeg streaming error';
        request.log.error({ err, sourceUrl: safeUrl }, 'FFmpeg remux streaming failed');
        return reply.status(502).send({
          error: 'Failed to start media stream.',
          detail: message,
        });
      }
    }
  );
}
