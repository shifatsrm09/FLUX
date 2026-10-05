import { execFile } from 'node:child_process';

const FFPROBE_PATH = process.env.FFPROBE_PATH || 'ffprobe';

export interface FFprobeResult {
  format: Record<string, unknown>;
  streams: Record<string, unknown>[];
}

/**
 * Run ffprobe against a remote media URL and return parsed JSON output.
 *
 * Security: uses execFile (no shell interpolation) and passes the URL
 * strictly as an argument — never interpolated into a shell string.
 */
export function probeMedia(url: string): Promise<FFprobeResult> {
  return new Promise((resolve, reject) => {
    const args = [
      '-v', 'quiet',
      '-print_format', 'json',
      '-show_format',
      '-show_streams',
      url,
    ];

    execFile(FFPROBE_PATH, args, { timeout: 30_000, maxBuffer: 1024 * 1024 }, (error, stdout, stderr) => {
      if (error) {
        reject(new Error(`FFprobe failed: ${error.message}${stderr ? ` — ${stderr.trim()}` : ''}`));
        return;
      }

      try {
        const parsed = JSON.parse(stdout) as FFprobeResult;
        resolve(parsed);
      } catch {
        reject(new Error('Failed to parse FFprobe JSON output.'));
      }
    });
  });
}
