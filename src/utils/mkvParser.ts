import type { EbmlHeaderInfo, EbmlTrack } from '../types/diagnostics';

// Common EBML Element IDs
const ID_EBML = 0x1a45dfa3;
const ID_DOCTYPE = 0x4282;
const ID_DOCTYPE_VERSION = 0x4287;
const ID_SEGMENT = 0x18538067;
const ID_SEEK_HEAD = 0x114d9b74;
const ID_INFO = 0x1549a966;
const ID_TIMECODE_SCALE = 0x2ad7b1;
const ID_DURATION = 0x4489;
const ID_TITLE = 0x7ba9;
const ID_TRACKS = 0x1654ae6b;
const ID_TRACK_ENTRY = 0xae;
const ID_TRACK_NUMBER = 0xd7;
const ID_TRACK_TYPE = 0x83;
const ID_CODEC_ID = 0x86;
const ID_TRACK_NAME = 0x536e;
const ID_LANGUAGE = 0x22b59c;
const ID_FLAG_DEFAULT = 0x88;
const ID_VIDEO = 0xe0;
const ID_PIXEL_WIDTH = 0xb0;
const ID_PIXEL_HEIGHT = 0xba;
const ID_AUDIO = 0xe1;
const ID_SAMPLING_FREQUENCY = 0xb5;
const ID_CHANNELS = 0x9f;
const ID_BIT_DEPTH = 0x6264;

class EbmlReader {
  private view: DataView;
  private offset = 0;

  constructor(buffer: ArrayBuffer) {
    this.view = new DataView(buffer);
  }

  get remaining(): number {
    return this.view.byteLength - this.offset;
  }

  get currentOffset(): number {
    return this.offset;
  }

  seek(pos: number) {
    this.offset = Math.min(pos, this.view.byteLength);
  }

  // Read EBML Variable Length Integer (VINT) for Element ID
  readElementId(): { id: number; length: number } | null {
    if (this.offset >= this.view.byteLength) return null;
    const firstByte = this.view.getUint8(this.offset);
    if (firstByte === 0) return null;

    let length = 1;
    let mask = 0x80;
    while (length <= 4 && !(firstByte & mask)) {
      length++;
      mask >>= 1;
    }

    if (length > 4 || this.offset + length > this.view.byteLength) return null;

    let id = 0;
    for (let i = 0; i < length; i++) {
      id = (id << 8) | this.view.getUint8(this.offset + i);
    }

    this.offset += length;
    return { id, length };
  }

  // Read EBML Variable Length Integer (VINT) for Data Size
  readDataSize(): number | null {
    if (this.offset >= this.view.byteLength) return null;
    const firstByte = this.view.getUint8(this.offset);
    if (firstByte === 0) return null;

    let length = 1;
    let mask = 0x80;
    while (length <= 8 && !(firstByte & mask)) {
      length++;
      mask >>= 1;
    }

    if (this.offset + length > this.view.byteLength) return null;

    // Remove the leading length marker bit
    let size = firstByte & (mask - 1);
    for (let i = 1; i < length; i++) {
      size = size * 256 + this.view.getUint8(this.offset + i);
    }

    this.offset += length;
    return size;
  }

  readString(size: number): string {
    const end = Math.min(this.offset + size, this.view.byteLength);
    const bytes = new Uint8Array(this.view.buffer, this.view.byteOffset + this.offset, end - this.offset);
    this.offset = end;
    // Strip trailing null characters if present
    const decoder = new TextDecoder('utf-8');
    return decoder.decode(bytes).replace(/\0+$/, '').trim();
  }

  readUint(size: number): number {
    let val = 0;
    const end = Math.min(this.offset + size, this.view.byteLength);
    for (let i = this.offset; i < end; i++) {
      val = val * 256 + this.view.getUint8(i);
    }
    this.offset = end;
    return val;
  }

  readFloat(size: number): number {
    if (size === 4 && this.offset + 4 <= this.view.byteLength) {
      const v = this.view.getFloat32(this.offset);
      this.offset += 4;
      return v;
    } else if (size === 8 && this.offset + 8 <= this.view.byteLength) {
      const v = this.view.getFloat64(this.offset);
      this.offset += 8;
      return v;
    }
    this.offset += size;
    return 0;
  }

  skip(bytes: number) {
    this.offset = Math.min(this.offset + bytes, this.view.byteLength);
  }
}

function humanizeCodecId(id: string): string {
  const map: Record<string, string> = {
    'V_MPEGH/ISO/HEVC': 'HEVC / H.265',
    'V_MPEG4/ISO/AVC': 'H.264 / AVC',
    'V_VP8': 'VP8',
    'V_VP9': 'VP9',
    'V_AV1': 'AV1',
    'A_AAC': 'AAC',
    'A_AAC/MPEG4/LC': 'AAC-LC',
    'A_DTS': 'DTS',
    'A_DTS/EXPRESS': 'DTS Express',
    'A_AC3': 'Dolby Digital (AC-3)',
    'A_EAC3': 'Dolby Digital Plus (E-AC-3)',
    'A_TRUEHD': 'Dolby TrueHD',
    'A_FLAC': 'FLAC',
    'A_OPUS': 'Opus',
    'A_VORBIS': 'Vorbis',
    'S_TEXT/UTF8': 'SubRip (SRT)',
    'S_TEXT/ASS': 'Advanced SubStation Alpha (ASS)',
    'S_TEXT/SSA': 'SubStation Alpha (SSA)',
    'S_HDMV/PGS': 'Blu-ray PGS Subtitle',
  };
  return map[id] || id;
}

export function parseMkvEbml(buffer: ArrayBuffer): EbmlHeaderInfo {
  const reader = new EbmlReader(buffer);

  let docType = 'unknown';
  let docTypeVersion = 1;
  let title: string | undefined;
  let timecodeScale = 1_000_000; // default 1ms
  let rawDuration = 0;
  const tracks: EbmlTrack[] = [];

  while (reader.remaining > 4) {
    const elem = reader.readElementId();
    if (!elem) break;

    const size = reader.readDataSize();
    if (size === null) break;

    const elemStart = reader.currentOffset;
    const elemEnd = elemStart + size;

    if (elem.id === ID_EBML) {
      // Inside EBML Header
      while (reader.currentOffset < elemEnd && reader.remaining > 2) {
        const sub = reader.readElementId();
        if (!sub) break;
        const subSize = reader.readDataSize();
        if (subSize === null) break;

        if (sub.id === ID_DOCTYPE) {
          docType = reader.readString(subSize);
        } else if (sub.id === ID_DOCTYPE_VERSION) {
          docTypeVersion = reader.readUint(subSize);
        } else {
          reader.skip(subSize);
        }
      }
    } else if (elem.id === ID_SEGMENT) {
      // Root Segment: Don't skip, step inside
      continue;
    } else if (elem.id === ID_SEEK_HEAD) {
      // SeekHead can be skipped or parsed
      reader.skip(size);
    } else if (elem.id === ID_INFO) {
      // Segment Information
      while (reader.currentOffset < elemEnd && reader.remaining > 2) {
        const sub = reader.readElementId();
        if (!sub) break;
        const subSize = reader.readDataSize();
        if (subSize === null) break;

        if (sub.id === ID_TIMECODE_SCALE) {
          timecodeScale = reader.readUint(subSize);
        } else if (sub.id === ID_DURATION) {
          rawDuration = reader.readFloat(subSize);
        } else if (sub.id === ID_TITLE) {
          title = reader.readString(subSize);
        } else {
          reader.skip(subSize);
        }
      }
    } else if (elem.id === ID_TRACKS) {
      // Tracks Element
      while (reader.currentOffset < elemEnd && reader.remaining > 2) {
        const trackEntryElem = reader.readElementId();
        if (!trackEntryElem) break;
        const trackEntrySize = reader.readDataSize();
        if (trackEntrySize === null) break;

        if (trackEntryElem.id === ID_TRACK_ENTRY) {
          const trackEnd = reader.currentOffset + trackEntrySize;
          const track: Partial<EbmlTrack> = {
            trackType: 'unknown',
            codecId: 'unknown',
          };

          while (reader.currentOffset < trackEnd && reader.remaining > 1) {
            const field = reader.readElementId();
            if (!field) break;
            const fieldSize = reader.readDataSize();
            if (fieldSize === null) break;

            if (field.id === ID_TRACK_NUMBER) {
              track.trackNumber = reader.readUint(fieldSize);
            } else if (field.id === ID_TRACK_TYPE) {
              const typeVal = reader.readUint(fieldSize);
              if (typeVal === 1) track.trackType = 'video';
              else if (typeVal === 2) track.trackType = 'audio';
              else if (typeVal === 17) track.trackType = 'subtitle';
              else track.trackType = 'unknown';
            } else if (field.id === ID_CODEC_ID) {
              track.codecId = reader.readString(fieldSize);
            } else if (field.id === ID_TRACK_NAME) {
              track.name = reader.readString(fieldSize);
            } else if (field.id === ID_LANGUAGE) {
              track.language = reader.readString(fieldSize);
            } else if (field.id === ID_FLAG_DEFAULT) {
              track.isDefault = reader.readUint(fieldSize) === 1;
            } else if (field.id === ID_VIDEO) {
              const vEnd = reader.currentOffset + fieldSize;
              while (reader.currentOffset < vEnd && reader.remaining > 1) {
                const vf = reader.readElementId();
                if (!vf) break;
                const vfSize = reader.readDataSize();
                if (vfSize === null) break;
                if (vf.id === ID_PIXEL_WIDTH) track.pixelWidth = reader.readUint(vfSize);
                else if (vf.id === ID_PIXEL_HEIGHT) track.pixelHeight = reader.readUint(vfSize);
                else reader.skip(vfSize);
              }
            } else if (field.id === ID_AUDIO) {
              const aEnd = reader.currentOffset + fieldSize;
              while (reader.currentOffset < aEnd && reader.remaining > 1) {
                const af = reader.readElementId();
                if (!af) break;
                const afSize = reader.readDataSize();
                if (afSize === null) break;
                if (af.id === ID_CHANNELS) track.channels = reader.readUint(afSize);
                else if (af.id === ID_SAMPLING_FREQUENCY) track.samplingFrequency = Math.round(reader.readFloat(afSize));
                else if (af.id === ID_BIT_DEPTH) track.bitDepth = reader.readUint(afSize);
                else reader.skip(afSize);
              }
            } else {
              reader.skip(fieldSize);
            }
          }

          if (track.trackNumber !== undefined) {
            tracks.push({
              trackNumber: track.trackNumber,
              trackType: track.trackType || 'unknown',
              codecId: track.codecId || 'unknown',
              codecName: humanizeCodecId(track.codecId || 'unknown'),
              name: track.name,
              language: track.language,
              isDefault: track.isDefault,
              pixelWidth: track.pixelWidth,
              pixelHeight: track.pixelHeight,
              channels: track.channels,
              samplingFrequency: track.samplingFrequency,
              bitDepth: track.bitDepth,
            });
          }
        } else {
          reader.skip(trackEntrySize);
        }
      }
    } else {
      // Unknown element, skip
      reader.skip(size);
    }

    // Safety: ensure monotonic forward progress
    if (reader.currentOffset <= elemStart) {
      reader.skip(Math.max(1, size));
    }
  }

  const durationSeconds = rawDuration > 0 ? (rawDuration * timecodeScale) / 1_000_000_000 : undefined;

  return {
    docType,
    docTypeVersion,
    parsedAt: Date.now(),
    bytesRead: buffer.byteLength,
    title,
    durationSeconds,
    tracks,
  };
}

export async function fetchAndParseMkvHeader(url: string, maxBytes = 512 * 1024): Promise<EbmlHeaderInfo> {
  const response = await fetch(url, {
    headers: {
      Range: `bytes=0-${maxBytes - 1}`,
    },
  });

  if (!response.ok && response.status !== 206) {
    throw new Error(`Failed to fetch MKV header: HTTP ${response.status} ${response.statusText}`);
  }

  const buffer = await response.arrayBuffer();
  return parseMkvEbml(buffer);
}
