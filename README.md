# BDIXStream

Internal ISP media streaming platform — technical prototype.

BDIXStream is designed to inspect and play media files available on an ISP's internal network. Given a media URL, it analyzes the file using FFprobe, extracts detailed metadata, and selects the playback method: direct playback for natively supported media, or on-demand stream-copy remuxing (MKV → fragmented MP4) via FFmpeg without re-encoding or temporary disk files.

## Architecture

```
                    Media URL
                       │
                   FFprobe
                       │
              ┌────────┴────────┐
              ▼                 ▼
       Direct Playable    Needs Processing
       (MP4 / WebM)            (MKV)
              │                 │
              │                 ▼
              │            /api/stream
              │                 │
              │              FFmpeg
              │        (stream copy: -c copy)
              │                 │
              │          fragmented MP4
              │           (pipe: stdout)
              │                 │
              └────────┬────────┘
                       ▼
             HTML5 <video> Player
```

## Current Prototype Capabilities

- **Media analysis**: Enter any authorized HTTP/HTTPS media URL → backend runs FFprobe → returns normalized structured metadata
- **On-demand remux streaming**: Endpoint `GET /api/stream?url=...` runs FFmpeg on-the-fly, reading directly from the remote media URL and streaming fragmented MP4 (`video/mp4`) to the HTTP response
- **Stream copy (no transcoding)**: Video and audio streams are copied as-is (`-c:v copy -c:a copy`) with zero quality loss and near-zero CPU overhead
- **No disk storage**: Media is never written to disk or fully buffered into memory
- **Client disconnect handling**: Terminating playback, pausing, or closing the tab immediately terminates the FFmpeg process
- **Detailed media display**: Container, video/audio codecs, resolution, frame rate, bitrate, duration, file size, multi-audio tracks, subtitle streams
- **Direct play vs remux selection**: Heuristic determines whether the browser can play directly or needs on-demand remuxing
- **Playback state & codec reporting**: Live player state indicators (Preparing stream..., Playing, Playback error) with clear diagnostics for unsupported codecs like HEVC / H.265 Main 10

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| **Node.js** | 18+ | LTS recommended |
| **FFmpeg** | 5+ | Required for on-demand remuxing (`ffmpeg`) |
| **FFprobe** | 5+ | Required for media inspection (`ffprobe`) |

Verify tools are available in PATH:

```bash
ffmpeg -version
ffprobe -version
```

## Installation

```bash
# Clone the repository
git clone <repo-url>
cd BDIXStream

# Install all dependencies (root, client, and server)
npm run install:all
```

Or install individually:

```bash
npm install          # root (concurrently)
cd client && npm install
cd ../server && npm install
```

## Configuration

Copy the environment template:

```bash
cp .env.example .env
```

Edit `.env` as needed:

```env
PORT=3001                           # Backend port
CORS_ORIGIN=http://localhost:5173   # Frontend dev server origin
FFPROBE_PATH=                       # Path to ffprobe (leave empty for PATH)
FFMPEG_PATH=                        # Path to ffmpeg (leave empty for PATH)
```

## Running in Development

### Option 1: Both simultaneously (recommended)

```bash
npm run dev
```

Starts the Fastify backend on port 3001 and Vite frontend on port 5173 concurrently.

### Option 2: Separately

```bash
# Terminal 1 — Backend
npm run dev:server

# Terminal 2 — Frontend
npm run dev:client
```

Open **http://localhost:5173** in your browser.

## Testing with an ISP Media URL

1. Open http://localhost:5173
2. Enter an authorized internal media URL, for example:
   ```
   http://172.16.50.4/path/to/movie.mkv
   ```
3. Click **Analyze**
4. View the extracted metadata (video codec, audio tracks, subtitles)
5. The VideoPlayer automatically routes the playback:
   - **Direct Play**: If the file is MP4 / WebM natively supported by browsers
   - **On-Demand Remux**: If the file is MKV, the video player points to `/api/stream?url=...`
6. FFmpeg begins remuxing immediately; fragmented MP4 streams progressively to the browser

> **Important Codec Rule**:
> Remuxing repackages the container (MKV → MP4) but does NOT change the underlying video codec (`-c:v copy`).
> If the source file is **HEVC / H.265 Main 10**, desktop browsers without hardware HEVC decoding (such as standard Chrome/Firefox on Windows/Linux) will fail to decode the video stream. The UI explicitly detects and explains this limitation. True cross-browser playback for HEVC files requires video transcoding (HEVC → H.264), planned for the next milestone.

## API Endpoints

### 1. `POST /api/analyze`

Analyzes remote media with FFprobe.

**Request:**

```json
{
  "url": "http://172.16.50.4/path/to/movie.mkv"
}
```

**Response:**

```json
{
  "success": true,
  "metadata": {
    "filename": "movie.mkv",
    "container": "MKV",
    "format": "Matroska / WebM",
    "duration": "43:55",
    "durationSeconds": 2635,
    "fileSize": "613.50 MB",
    "fileSizeBytes": 643297280,
    "bitrate": "1.9 Mbps",
    "video": {
      "codec": "hevc",
      "profile": "Main 10",
      "width": 1920,
      "height": 960,
      "frameRate": "23.976 fps",
      "bitrate": "1.6 Mbps",
      "pixelFormat": "yuv420p10le"
    },
    "audio": {
      "codec": "aac",
      "channels": 6,
      "channelLayout": "5.1",
      "sampleRate": "48000",
      "bitrate": "384 Kbps",
      "language": "eng"
    },
    "audioTracks": [...],
    "subtitles": [...],
    "directPlayCandidate": false
  }
}
```

### 2. `GET /api/stream?url=<encoded-url>`

Streams on-demand fragmented MP4 (`video/mp4`) by invoking FFmpeg with stream copy.

- **Query param**: `url` (URL-encoded remote media URL)
- **Response**: Chunked `video/mp4` stream
- **Headers**:
  - `Content-Type: video/mp4`
  - `Cache-Control: no-cache, no-store, must-revalidate`
- **Behavior**:
  - Starts streaming as soon as FFmpeg emits the first fragment
  - When the client disconnects, FFmpeg is immediately terminated via `SIGTERM` / `SIGKILL`

### 3. `GET /api/health`

Health check endpoint. Returns `{"status": "ok"}`.

## Production Build

```bash
# Build both frontend and backend
npm run build

# Start backend production server
npm start
```

## Project Structure

```
BDIXStream/
├── client/                     # React frontend (Vite + TypeScript)
│   ├── src/
│   │   ├── components/
│   │   │   ├── MetadataPanel.tsx / MetadataPanel.css
│   │   │   ├── UrlInput.tsx / UrlInput.css
│   │   │   └── VideoPlayer.tsx / VideoPlayer.css
│   │   ├── App.tsx / App.css
│   │   ├── index.css
│   │   ├── main.tsx
│   │   └── types.ts
│   ├── index.html
│   ├── package.json
│   └── tsconfig.json
│
├── server/                     # Fastify backend (Node.js + TypeScript)
│   ├── src/
│   │   ├── routes/
│   │   │   ├── analyze.ts      # POST /api/analyze
│   │   │   └── stream.ts       # GET /api/stream
│   │   ├── services/
│   │   │   ├── ffmpeg.ts       # FFmpeg remux process management
│   │   │   ├── ffprobe.ts      # FFprobe metadata probe
│   │   │   └── metadata.ts     # Metadata normalization & heuristics
│   │   ├── utils/
│   │   │   └── validation.ts   # Safe URL validation
│   │   └── server.ts           # Fastify server entry
│   ├── package.json
│   └── tsconfig.json
│
├── .env.example
├── .gitignore
├── package.json
└── README.md
```

## Security Design

- **Safe process invocation**: Uses `child_process.spawn` and `execFile` exclusively with argument arrays — no shell interpretation (`exec`) or command string concatenation
- **Protocol restriction**: Validates URLs to only allow `http:` and `https:`, blocking `file://`, `ftp://`, etc.
- **Log sanitization**: Passwords or authentication credentials in URLs are stripped before logging
- **Process lifecycle guarantee**: FFmpeg child processes are tied to the HTTP request lifecycle; if the client disconnects, FFmpeg is killed to prevent zombie processes

## Current Limitations

- **No transcoding** — stream copy retains original video/audio codecs; HEVC Main 10 requires browser/hardware HEVC decoding
- **No HTTP Range seek on remux stream** — progressive streaming begins from 0:00; random seeking in fragmented MP4 pipe requires transcoding/range proxy
- **Single stream selection** — remux maps the first video (`0:v:0`) and first audio track (`0:a:0`)
- **No subtitle muxing** — subtitle streams are detected in metadata but not converted to WebVTT or burned into video
- **No persistence or auth** — no database, user accounts, or watch history

## Planned Future Milestones

1. **FFmpeg transcoding pipeline** (HEVC → H.264, EAC3/DTS → AAC)
2. **HTTP Range support / timeline seeking**
3. **Subtitle extraction to WebVTT** for in-player selection
4. **Multi-audio track switching**
5. **HLS / DASH adaptive streaming**
6. **Media library indexing and PostgreSQL database**
