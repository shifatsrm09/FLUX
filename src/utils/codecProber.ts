import type { CodecSupportMatrix } from '../types/diagnostics';

const PROBE_MIME_TYPES = [
  // MKV Container
  'video/x-matroska',
  'video/x-matroska; codecs="avc1.640028"',
  'video/x-matroska; codecs="hvc1.1.6.L93.B0"',
  'video/x-matroska; codecs="hev1.1.6.L93.B0"',
  'video/x-matroska; codecs="vp9"',
  'video/x-matroska; codecs="av01.0.08M.08"',
  // MKV with Audio Codecs
  'video/x-matroska; codecs="avc1, mp4a.40.2"',
  'video/x-matroska; codecs="hvc1, mp4a.40.2"',
  'video/x-matroska; codecs="avc1, ac-3"',
  'video/x-matroska; codecs="avc1, ec-3"',
  'video/x-matroska; codecs="avc1, dts"',
  // MP4 Container
  'video/mp4; codecs="avc1.640028, mp4a.40.2"',
  'video/mp4; codecs="hvc1.1.6.L93.B0, mp4a.40.2"',
  'video/mp4; codecs="hev1.1.6.L93.B0, mp4a.40.2"',
  'video/mp4; codecs="av01.0.08M.08, mp4a.40.2"',
  // WebM Container
  'video/webm; codecs="vp9, opus"',
  'video/webm; codecs="av01.0.08M.08, opus"',
  // Audio Codecs Direct
  'audio/mp4; codecs="mp4a.40.2"', // AAC-LC
  'audio/mp4; codecs="ac-3"',      // Dolby Digital
  'audio/mp4; codecs="ec-3"',      // Dolby Digital Plus
  'audio/vnd.dts',
  'audio/ogg; codecs="opus"',
  'audio/ogg; codecs="vorbis"',
];

export async function probeCodecSupport(): Promise<CodecSupportMatrix> {
  const matrix: CodecSupportMatrix = {};
  const testVideo = document.createElement('video');

  for (const mime of PROBE_MIME_TYPES) {
    const rawResult = testVideo.canPlayType(mime);
    let canPlay: 'probably' | 'maybe' | 'no' = 'no';
    if (rawResult === 'probably') canPlay = 'probably';
    else if (rawResult === 'maybe') canPlay = 'maybe';

    matrix[mime] = {
      canPlayType: canPlay,
    };
  }

  // Probe MediaCapabilities API if supported (modern Chromium, Safari, Firefox)
  if (typeof navigator !== 'undefined' && navigator.mediaCapabilities?.decodingInfo) {
    try {
      // 1. HEVC in MP4
      const hevcConfig: MediaDecodingConfiguration = {
        type: 'file',
        video: {
          contentType: 'video/mp4; codecs="hvc1.1.6.L93.B0"',
          width: 1920,
          height: 1080,
          bitrate: 5_000_000,
          framerate: 24,
        },
      };
      const hevcInfo = await navigator.mediaCapabilities.decodingInfo(hevcConfig);
      if (matrix['video/mp4; codecs="hvc1.1.6.L93.B0, mp4a.40.2"']) {
        matrix['video/mp4; codecs="hvc1.1.6.L93.B0, mp4a.40.2"'].mediaCapabilities = {
          supported: hevcInfo.supported,
          smooth: hevcInfo.smooth,
          powerEfficient: hevcInfo.powerEfficient,
        };
      }

      // 2. H.264 in MP4
      const h264Config: MediaDecodingConfiguration = {
        type: 'file',
        video: {
          contentType: 'video/mp4; codecs="avc1.640028"',
          width: 1920,
          height: 1080,
          bitrate: 5_000_000,
          framerate: 24,
        },
      };
      const h264Info = await navigator.mediaCapabilities.decodingInfo(h264Config);
      if (matrix['video/mp4; codecs="avc1.640028, mp4a.40.2"']) {
        matrix['video/mp4; codecs="avc1.640028, mp4a.40.2"'].mediaCapabilities = {
          supported: h264Info.supported,
          smooth: h264Info.smooth,
          powerEfficient: h264Info.powerEfficient,
        };
      }
    } catch {
      // MediaCapabilities check optional fallback
    }
  }

  return matrix;
}
