import Fastify from 'fastify';
import cors from '@fastify/cors';
import dotenv from 'dotenv';
import { analyzeRoutes } from './routes/analyze.js';
import { streamRoutes } from './routes/stream.js';
import { hlsRoutes } from './routes/hls.js';
import { hlsSessionManager } from './services/hlsSession.js';

dotenv.config({ path: '../.env' });

const PORT = parseInt(process.env.PORT || '3001', 10);
const CORS_ORIGIN = process.env.CORS_ORIGIN || 'http://localhost:5173';

async function bootstrap() {
  const app = Fastify({ logger: true });

  await app.register(cors, { origin: CORS_ORIGIN });

  // Initialize HLS session manager and clean up any stale temp directories
  await hlsSessionManager.init(app.log);

  // --- Routes ---
  await app.register(analyzeRoutes, { prefix: '/api' });
  await app.register(streamRoutes, { prefix: '/api' });
  await app.register(hlsRoutes, { prefix: '/api/hls' });

  // Health check
  app.get('/api/health', async () => ({ status: 'ok' }));

  // Graceful shutdown
  const signals: NodeJS.Signals[] = ['SIGINT', 'SIGTERM'];
  for (const signal of signals) {
    process.on(signal, async () => {
      app.log.info({ signal }, 'Received shutdown signal; cleaning up active HLS sessions and workers');
      try {
        await hlsSessionManager.shutdown(app.log);
        await app.close();
        app.log.info('Server shutdown complete');
        process.exit(0);
      } catch (err) {
        app.log.error({ err }, 'Error during shutdown');
        process.exit(1);
      }
    });
  }

  try {
    await app.listen({ port: PORT, host: '0.0.0.0' });
    console.log(`BDIXStream server listening on http://localhost:${PORT}`);
  } catch (err) {
    app.log.error(err);
    process.exit(1);
  }
}

bootstrap();

