import { FastifyInstance, FastifyRequest, FastifyReply } from 'fastify';
import { probeMedia } from '../services/ffprobe.js';
import { normalizeMetadata } from '../services/metadata.js';
import { isValidMediaUrl } from '../utils/validation.js';

interface AnalyzeBody {
  url: string;
}

export async function analyzeRoutes(app: FastifyInstance) {
  app.post<{ Body: AnalyzeBody }>('/analyze', async (request: FastifyRequest<{ Body: AnalyzeBody }>, reply: FastifyReply) => {
    const { url } = request.body ?? {};

    if (!url || typeof url !== 'string') {
      return reply.status(400).send({
        error: 'Missing or invalid "url" field in request body.',
      });
    }

    if (!isValidMediaUrl(url)) {
      return reply.status(400).send({
        error: 'The provided URL is not a valid HTTP/HTTPS media URL.',
      });
    }

    try {
      const rawProbe = await probeMedia(url);
      const metadata = normalizeMetadata(rawProbe, url);
      return reply.send({ success: true, metadata });
    } catch (err: unknown) {
      const message = err instanceof Error ? err.message : 'Unknown FFprobe error';
      request.log.error({ err }, 'FFprobe analysis failed');
      return reply.status(422).send({
        error: 'Failed to analyze media.',
        detail: message,
      });
    }
  });
}
