# BDIXStream

Internal ISP media streaming platform — technical prototype.

BDIXStream is designed to inspect and play media files available on an ISP's internal network. Given a media URL, it analyzes the file using FFprobe, extracts detailed metadata, and selects the playback method: direct playback for natively supported media (MP4/WebM), or seekable on-demand HLS VOD (MKV → fragmented MP4 HLS) via FFmpeg stream copy without re-encoding or permanent disk conversion.

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
              │          POST /api/hls/session
              │                 │
              │           BDIXStream HLS
              │           Session Manager
              │                 │
              │       ┌─────────┴─────────┐
              │       ▼                   ▼
              │  index.m3u8            init.mp4
              │  (full duration)    (track headers)
              │       │                   │
              │       └─────────┬─────────┘
              │                 ▼
              │          Seekable Worker
              │        (-ss seek on-demand)
              │                 │
              │          segment_XXXX.m4s
              │                 │
              └────────┬────────┘
                       ▼
             Browser Video Player
            (hls.js / native HLS)
```

## Features

- **Media Analysis**: Enter any authorized HTTP/HTTPS media URL → backend runs FFprobe → returns normalized structured metadata
- **Seekable HLS VOD Architecture**:
  - Full duration known immediately from FFprobe
  - Complete `#EXT-X-PLAYLIST-TYPE:VOD` playlist generated at session creation
  - Arbitrary, instantaneous seeking to any timestamp (e.g. 10%, 50%, 90%, or jumping 40 minutes ahead) without waiting to stream earlier media
  - On-demand segment generation: FFmpeg seeks directly to the target timestamp (`-ss <timestamp> -copyts -start_number <index>`) to produce requested segments in milliseconds
  - Proactive buffer-ahead: Workers generate a small window ahead of the playhead for seamless continuous playback
- **Stream Copy (No Transcoding)**:
  - Video and audio streams are copied as-is (`-c:v copy -c:a copy`) with zero quality loss and near-zero CPU overhead
  - Video codec remains original (e.g. HEVC Main 10)
  - Audio codec remains original (e.g. AAC 5.1)
- **Bounded Temporary Storage & Lifecycle Management**:
  - Dedicated isolated directories per session (`os.tmpdir()/bdixstream/<sessionId>/`)
  - No permanent 20 GB conversions; temporary segments are pruned when cache limits are reached
  - Automatic TTL cleanup (default: 30 minutes of inactivity)
  - Automatic cleanup of stale directories on server startup and shutdown (`SIGINT`, `SIGTERM`)
- **Robust Process Management**:
  - Every FFmpeg worker is tracked per session with PID logging
  - Re-anchoring a seek position cleanly terminates the previous forward worker (`SIGTERM` → `SIGKILL`)
  - No orphan FFmpeg processes or leaked file descriptors
- **Frontend Player**:
  - Uses `hls.js` for universal MSE playback (Chrome, Firefox, Edge) and native HLS on Safari
  - Accurate status UI: "⚡ On-Demand HLS", "Video: HEVC / H.265 Main 10", "Audio: AAC 5.1", "Stream copy: enabled", "Transcoding: disabled"
  - Real-time states: `Preparing stream...`, `Loading playlist...`, `Buffering...`, `Playing`, `Seeking...`, `Playback Error`, `Session Expired`
  - Codec diagnostics: Clearly explains if the browser lacks native HEVC hardware decoding

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| **Node.js** | 18+ | LTS recommended |
| **FFmpeg** | 5+ | Required for on-demand HLS packaging (`ffmpeg`) |
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

# HLS VOD Configuration
HLS_SEGMENT_DURATION=6              # Segment duration in seconds (default: 6)
HLS_SESSION_TTL_MINUTES=30          # Inactivity cleanup timeout in minutes (default: 30)
HLS_TEMP_DIR=                       # Temp storage directory (default: system tmpdir/bdixstream)
MAX_ACTIVE_SESSIONS=10              # Maximum concurrent streaming sessions
MAX_CONCURRENT_FFMPEG_PROCESSES=5   # Maximum concurrent FFmpeg workers
```

## Running in Development

```bash
# Starts backend (port 3001) and frontend (port 5173) concurrently
npm run dev
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
   - **Direct Play**: If the file is natively supported (MP4 / WebM)
   - **On-Demand HLS**: If the file is MKV, creates a session via `POST /api/hls/session` and mounts the HLS stream
6. **Arbitrary Seeking Test**:
   - The timeline shows the full duration immediately
   - Seek to 50%, 90%, 10%, or 40:00
   - Playback re-anchors directly to the requested position without decoding from 00:00

> **Important Codec Rule**:
> HLS provides segment delivery and seeking, but does NOT re-encode the video (`-c:v copy`).
> If the source file is **HEVC / H.265 Main 10**, desktop browsers without hardware HEVC decoding (such as standard Chrome/Firefox on Windows/Linux) will fail to decode the video stream. The UI explicitly detects and explains this limitation. True cross-browser playback for HEVC files requires video transcoding (HEVC → H.264), planned for the next milestone.

## API Endpoints

### 1. `POST /api/analyze`

Analyzes remote media with FFprobe.

### 2. `POST /api/hls/session`

Initializes an HLS VOD streaming session for an authorized media URL.

**Request:**

```json
{
  "url": "http://172.16.50.4/path/to/movie.mkv"
}
```

**Response:**

```json
{
  "sessionId": "49557fc2-b8f1-4db8-b4b6-455b8e986064",
  "playlistUrl": "/api/hls/49557fc2-b8f1-4db8-b4b6-455b8e986064/index.m3u8",
  "duration": 2635.0,
  "segmentDuration": 6,
  "totalSegments": 440
}
```

### 3. `GET /api/hls/:sessionId/index.m3u8`

Returns the full HLS VOD playlist (`application/vnd.apple.mpegurl`).

### 4. `GET /api/hls/:sessionId/init.mp4`

Returns the fragmented MP4 initialization segment (`video/mp4`).

### 5. `GET /api/hls/:sessionId/:segment`

Returns the requested media segment (e.g. `segment_0400.m4s`), generating on-demand or returning from cache.

### 6. `GET /api/hls/:sessionId/status`

Returns session diagnostics, cache size, active FFmpeg worker state, and idle time.

### 7. `DELETE /api/hls/:sessionId`

Explicitly terminates all FFmpeg processes and removes temporary files for the session.

## Production Build

```bash
# Build both frontend and backend
npm run build

# Start backend production server
npm start
```

## Security Design

- **Safe Process Invocation**: Uses `child_process.spawn` exclusively with argument arrays — no shell string interpolation
- **URL Whitelist**: Rejects any protocol other than `http:` and `https:`
- **Path Traversal Protection**: Session IDs and segment names are validated with strict regular expressions (`^[0-9a-fA-F-]{36}$` and `^segment_\d{4}\.m4s$`); arbitrary paths cannot be accessed
- **Process & Storage Isolation**: Each session runs in an isolated directory with bounded segment caching and automatic cleanup
