# BDIXStream Compatibility Lab

A client-native media streaming diagnostic platform designed to establish empirical browser compatibility across **Android**, **iOS Safari**, **Desktop Brave/Chromium**, and **Firefox**.

## Architecture

```
       Vercel (Edge CDN)
               │
               │ 1. Public UI (React + TypeScript)
               ▼
         User's Browser (Mobile / Desktop)
               │
               │ 2. Direct HTTP Range Requests (LAN)
               ▼
  Private BDIX Media Server (e.g. 172.16.50.14)
               │
               ▼
     Original Media Files (MKV / MP4)
```

- **Zero-Backend Architecture**: The web application is 100% static, client-side TypeScript deployable directly to Vercel.
- **Direct LAN Streaming**: The browser requests byte ranges (`206 Partial Content`) directly from the internal BDIX media server. No media bytes flow through Vercel or cloud proxies.
- **In-Browser Demuxing**: Uses a client-side EBML parser to read Matroska headers, tracks, and codecs over HTTP Range without needing FFprobe.
- **Native-First Evaluation**: Measures real browser `<video>` playback lifecycle, seek latency, audio signal presence, and decode errors across mobile and desktop devices.

## Getting Started

### 1. Install Dependencies

```bash
npm install
```

### 2. Start Diagnostic Lab

```bash
npm run dev
```

The lab runs on port **3000** with LAN binding (`host: true`).

- **Desktop**: Open `http://localhost:3000`
- **Mobile (Phone on same Wi-Fi/LAN)**: Open `http://<YOUR_PC_LAN_IP>:3000`

### 3. Build for Production (Vercel)

```bash
npm run build
```

Build output is written to `dist/`.

## Running Compatibility Tests

1. Click any of the **Quick Test Presets** (Ant-Man, Civil War, Kingdom S1E1) or enter any authorized media URL.
2. The lab automatically executes:
   - **Device & Engine Probe**: Detects browser, OS, MediaSource, Managed MediaSource (iOS).
   - **Network & Range Probe**: Tests latency, HTTP 206 status, Range response, and CORS.
   - **EBML Header Demuxer**: Reads the first ~512 KB to identify container, video codec (HEVC/AVC), audio tracks (DTS, AAC, AC3), and channels.
   - **Native Video Test Harness**: Mounts the native `<video>` element, measuring time to metadata, time to first frame, seek latency, and Web Audio signal presence.
3. Click **📋 Copy Diagnostic JSON** to copy the complete report and compare across Android, iOS, and Desktop devices.
