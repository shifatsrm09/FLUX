import Fastify from 'fastify';
import cors from '@fastify/cors';
import dotenv from 'dotenv';
import { analyzeRoutes } from './routes/analyze.js';
import { streamRoutes } from './routes/stream.js';

dotenv.config({ path: '../.env' });

const PORT = parseInt(process.env.PORT || '3001', 10);
const CORS_ORIGIN = process.env.CORS_ORIGIN || 'http://localhost:5173';

async function bootstrap() {
  const app = Fastify({ logger: true });

  await app.register(cors, { origin: CORS_ORIGIN });

  // --- Routes ---
  await app.register(analyzeRoutes, { prefix: '/api' });
  await app.register(streamRoutes, { prefix: '/api' });

  // Health check
  app.get('/api/health', async () => ({ status: 'ok' }));

  try {
    await app.listen({ port: PORT, host: '0.0.0.0' });
    console.log(`BDIXStream server listening on http://localhost:${PORT}`);
  } catch (err) {
    app.log.error(err);
    process.exit(1);
  }
}

bootstrap();
