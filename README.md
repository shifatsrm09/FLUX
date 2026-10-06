# FLUX — Native Windows Desktop Media Player

FLUX is a native Windows desktop media player built with **C++20**, **Qt 6**, **Qt Quick / QML**, and **libVLC** for streaming media directly from private BDIX media servers (`http://172.16.50.14/...`) without downloading the entire file first.

## Architecture

```text
FLUX (Qt Quick UI)
       │
       ▼
FLUX Media Engine (VLCPlayer + VLCVideoItem)
       │
       ▼
libVLC (Direct Demuxing & Decoding)
       │
       │ HTTP / Range Requests
       ▼
Private BDIX Media Server (172.16.50.14)
       │
       ▼
Remote MKV Stream (Progressive Playback)
```

- **Zero Full-Download**: Streams media on-the-fly over HTTP Range requests (`206 Partial Content`).
- **Embedded Rendering**: Video is rendered directly inside the Qt Quick scene graph via double-buffered memory callbacks (`RV32`).
- **Multi-Track Support**: Audio track and subtitle selection for multi-language and dual-audio MKV files.
- **Diagnostic Logging**: Live playback state, buffering percentage, video resolution, and libVLC event log.

## Hardcoded Test Catalog

The application includes direct test presets for:
1. **Ant-Man and the Wasp: Quantumania (2023)** — HEVC 10-bit • AAC 5.1
2. **Civil War (2024)** — Dual Audio (Hindi 5.1 + English 5.1) • HEVC 1080p
3. **Kingdom S1E1 (2019)** — Dual Audio (English 5.1 + Korean 5.1) • HEVC 1080p

You can also paste any custom BDIX media URL into the address bar.

## Technology Stack

- **Language**: C++20
- **UI Framework**: Qt 6 (Qt Quick, QML, Qt Quick Controls)
- **Media Engine**: libVLC 3.0.x (with libvlcpp headers)
- **Build System**: CMake (>= 3.20)
- **Compiler**: MSVC 2022 x64
- **Target OS**: Windows 10 x64 / Windows 11 x64

## Building with CMake & MSVC 2022

### Prerequisites

- Visual Studio 2022 (with Desktop development with C++)
- CMake 3.20+
- Qt 6 (6.5+ with Qt Quick modules)
- VLC 3.0.x installed (development SDK headers and import libraries are already included in `third_party/vlc/`)

### Configure & Build

```powershell
# In x64 Native Tools Command Prompt for VS 2022
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="C:/Qt/6.7.2/msvc2022_64"
cmake --build build --config Release
```

### Running

The CMake build system automatically copies `libvlc.dll`, `libvlccore.dll`, and VLC `plugins/` from `C:/Program Files/VideoLAN/VLC` into the build output directory on post-build.

Run:
```powershell
.\build\Release\FLUX.exe
```

### Packaging / Deployment

To produce a self-contained release directory:
```powershell
windeployqt --qmldir qml .\build\Release\FLUX.exe
```
This bundles all required Qt runtime DLLs, QML plugins, and libVLC modules into the folder, requiring zero external installations on the end user's machine.
