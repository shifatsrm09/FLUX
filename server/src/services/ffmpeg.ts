import { spawn } from 'node:child_process';
import { Readable, PassThrough } from 'node:stream';
import { FastifyBaseLogger } from 'fastify';

const FFMPEG_PATH = process.env.FFMPEG_PATH || 'ffmpeg';

export interface RemuxStreamResult {
  mediaStream: Readable;
  cleanup: () => void;
  pid: number | undefined;
}

/**
 * Remove credentials from URL for safe logging.
 */
export function sanitizeUrlForLog(url: string): string {
  try {
    const parsed = new URL(url);
    if (parsed.password) {
      parsed.password = '***';
    }
    return parsed.toString();
  } catch {
    return url;
  }
}

/**
 * Launch FFmpeg to remux remote media into fragmented MP4 on the fly.
 *
 * Security:
 * - Uses child_process.spawn with an argument array (no shell interpolation).
 * - Remote URL is passed strictly as an argument to `-i`.
 * - No intermediate files are created; output is streamed via stdout (pipe:1).
 *
 * Stream copy:
 * - `-c:v copy` - stream copy first video stream
 * - `-c:a copy` - stream copy first audio stream (optional via 0:a:0?)
 * - `-movflags frag_keyframe+empty_moov+default_base_moof` - progressive fragmented MP4
 */
export function startRemuxStream(
  url: string,
  logger: FastifyBaseLogger
): Promise<RemuxStreamResult> {
  return new Promise((resolve, reject) => {
    const safeUrl = sanitizeUrlForLog(url);

    const args = [
      '-nostdin',
      '-loglevel', 'warning',
      '-i', url,
      '-map', '0:v:0',
      '-map', '0:a:0?',
      '-c:v', 'copy',
      '-c:a', 'copy',
      '-movflags', 'frag_keyframe+empty_moov+default_base_moof',
      '-f', 'mp4',
      'pipe:1',
    ];

    logger.info({ sourceUrl: safeUrl }, 'Spawning FFmpeg for remux streaming');

    let ffmpeg: ReturnType<typeof spawn>;
    try {
      ffmpeg = spawn(FFMPEG_PATH, args, {
        stdio: ['ignore', 'pipe', 'pipe'],
      });
    } catch (err) {
      logger.error({ err, sourceUrl: safeUrl }, 'Failed to spawn FFmpeg process');
      return reject(new Error(`Failed to spawn FFmpeg: ${err instanceof Error ? err.message : String(err)}`));
    }

    const pid = ffmpeg.pid;
    logger.info({ pid, sourceUrl: safeUrl }, 'FFmpeg remux process started');

    let hasStartedStreaming = false;
    let isCleanedUp = false;
    let stderrBuffer = '';
    const maxStderr = 16 * 1024; // keep last 16KB of stderr for error reporting

    const cleanup = () => {
      if (isCleanedUp) return;
      isCleanedUp = true;

      if (!ffmpeg.killed) {
        logger.info({ pid }, 'Terminating FFmpeg process');
        try {
          ffmpeg.kill('SIGTERM');
          // If FFmpeg doesn't exit promptly, force kill
          setTimeout(() => {
            if (!ffmpeg.killed) {
              logger.warn({ pid }, 'FFmpeg did not exit on SIGTERM; sending SIGKILL');
              ffmpeg.kill('SIGKILL');
            }
          }, 1500).unref();
        } catch (err) {
          logger.error({ err, pid }, 'Error terminating FFmpeg');
        }
      }
    };

    ffmpeg.stderr?.on('data', (chunk: Buffer) => {
      const text = chunk.toString();
      stderrBuffer = (stderrBuffer + text).slice(-maxStderr);
      logger.debug({ pid, stderr: text.trim() }, 'FFmpeg stderr output');
    });

    const passThrough = new PassThrough();

    const onFirstData = (firstChunk: Buffer) => {
      if (hasStartedStreaming) return;
      hasStartedStreaming = true;
      clearTimeout(startupTimer);

      logger.info(
        { pid, firstChunkSize: firstChunk.length },
        'FFmpeg outputting fragmented MP4 stream; piping to response'
      );

      passThrough.write(firstChunk);
      ffmpeg.stdout?.pipe(passThrough);

      resolve({
        mediaStream: passThrough,
        cleanup,
        pid,
      });
    };

    ffmpeg.stdout?.once('data', onFirstData);

    ffmpeg.stdout?.on('error', (err) => {
      logger.error({ err, pid }, 'FFmpeg stdout error');
      cleanup();
    });

    passThrough.on('error', (err: unknown) => {
      const code = (err as { code?: string })?.code;
      if (code === 'ERR_STREAM_PREMATURE_CLOSE' || code === 'EPIPE') {
        logger.info({ pid, code }, 'Downstream client disconnected / broken pipe');
      } else {
        logger.warn({ err, pid }, 'PassThrough downstream stream error');
      }
      cleanup();
    });

    ffmpeg.on('error', (err) => {
      logger.error({ err, pid }, 'FFmpeg process error');
      cleanup();
      if (!hasStartedStreaming) {
        clearTimeout(startupTimer);
        reject(new Error(`FFmpeg error: ${err.message}`));
      }
    });

    ffmpeg.on('close', (code, signal) => {
      logger.info({ pid, code, signal, stderr: stderrBuffer.trim() }, 'FFmpeg process exited');
      if (!hasStartedStreaming) {
        clearTimeout(startupTimer);
        const detail = stderrBuffer.trim() || `Exit code ${code ?? signal ?? 'unknown'}`;
        reject(new Error(`FFmpeg failed before streaming: ${detail}`));
      }
    });

    // Timeout if upstream media server never responds
    const startupTimer = setTimeout(() => {
      if (!hasStartedStreaming) {
        logger.error({ pid, sourceUrl: safeUrl }, 'FFmpeg stream startup timed out');
        cleanup();
        reject(new Error('FFmpeg startup timed out (upstream server did not produce data within 30s)'));
      }
    }, 30_000);
  });
}
