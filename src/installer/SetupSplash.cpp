#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <shellapi.h>
#include <objidl.h>
#include <gdiplus.h>
#undef min
#undef max
#include <tlhelp32.h>
#include <shellscalingapi.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cwchar>
#include <string>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shcore.lib")

using namespace Gdiplus;

namespace {

constexpr int kBaseWidth  = 300;
constexpr int kBaseHeight = 310;
constexpr UINT_PTR kTimerId = 1;
constexpr UINT kFrameIntervalMs = 16; // ~60 FPS

struct AppState {
    HWND hwnd = nullptr;
    HANDLE hParentProcess = nullptr;
    std::wstring progressFilePath;
    std::wstring versionText = L"v2.2.4";
    std::wstring statusStep  = L"INSTALLING UPDATE 1 OF 1";

    float scale = 1.0f;
    int width  = kBaseWidth;
    int height = kBaseHeight;

    float logoAngle = 0.0f;
    float targetProgress  = 0.12f;
    float displayProgress = 0.04f;
    bool finishing = false;
    int finishTicks = 0;
    HICON hAppIcon = nullptr;
    Gdiplus::Image *logoImage = nullptr;
};

AppState g_state;

DWORD FindParentProcessId() {
    const DWORD selfPid = GetCurrentProcessId();
    DWORD parentPid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == selfPid) {
                parentPid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return parentPid;
}

void AddRoundedRect(GraphicsPath &path, REAL x, REAL y, REAL w, REAL h, REAL radius) {
    REAL d = radius * 2.0f;
    if (d > w) d = w;
    if (d > h) d = h;
    path.Reset();
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void PollProgressFile() {
    if (g_state.progressFilePath.empty()) return;

    HANDLE hFile = CreateFileW(
        g_state.progressFilePath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    char buf[512]{};
    DWORD bytesRead = 0;
    const BOOL ok = ReadFile(hFile, buf, sizeof(buf) - 1, &bytesRead, nullptr);
    CloseHandle(hFile);
    if (!ok || bytesRead == 0) return;
    buf[bytesRead] = '\0';

    std::string content(buf, bytesRead);
    while (!content.empty() && (content.back() == '\r' || content.back() == '\n' || content.back() == ' ')) {
        content.pop_back();
    }

    const size_t sep = content.find('|');
    if (sep == std::string::npos) return;

    const std::string left = content.substr(0, sep);
    const std::string right = content.substr(sep + 1);

    if (left == "DONE") {
        g_state.targetProgress = 1.0f;
        g_state.finishing = true;
        return;
    }

    const int pct = std::atoi(left.c_str());
    if (pct > 0) {
        const float norm = std::fmin(1.0f, std::fmax(0.0f, static_cast<float>(pct) / 100.0f));
        if (norm > g_state.targetProgress) {
            g_state.targetProgress = norm;
        }
    }

    if (!right.empty()) {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, right.c_str(), static_cast<int>(right.size()), nullptr, 0);
        if (wlen > 0) {
            std::wstring wstr(static_cast<size_t>(wlen), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, right.c_str(), static_cast<int>(right.size()), &wstr[0], wlen);
            g_state.statusStep = wstr;
        }
    }
}

void RenderSplashFrame() {
    if (!g_state.hwnd) return;

    const int w = g_state.width;
    const int h = g_state.height;
    const float s = g_state.scale;

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    HBITMAP hBmp = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(memDc, hBmp));

    {
        Graphics g(memDc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.Clear(Color(0, 0, 0, 0));

        // 1. Exact Discord Card Surface (#282B30) with 8px radius and 1px border (#383C42)
        GraphicsPath cardPath;
        AddRoundedRect(cardPath, 0.5f * s, 0.5f * s, w - 1.0f * s, h - 1.0f * s, 8.0f * s);

        SolidBrush bgBrush(Color(255, 40, 43, 48)); // #282B30
        g.FillPath(&bgBrush, &cardPath);

        Pen borderPen(Color(255, 56, 60, 66), 1.0f * s); // #383C42
        g.DrawPath(&borderPen, &cardPath);

        // 2. Centered Still FLUX v2.png Logo (exact Discord Clyde position and size: 72x72 at top margin ~82)
        const REAL iconSize = 72.0f * s;
        const REAL iconX = (static_cast<REAL>(w) - iconSize) / 2.0f;
        const REAL iconY = 82.0f * s;

        if (g_state.logoImage) {
            g.DrawImage(g_state.logoImage, iconX, iconY, iconSize, iconSize);
        } else if (g_state.hAppIcon) {
            HDC gHdc = g.GetHDC();
            DrawIconEx(gHdc, static_cast<int>(iconX), static_cast<int>(iconY), g_state.hAppIcon, static_cast<int>(iconSize), static_cast<int>(iconSize), 0, nullptr, DI_NORMAL);
            g.ReleaseHDC(gHdc);
        }

        // 3. Status Text ("INSTALLING UPDATE 1 OF 1", bold italic, #FAFBFC)
        FontFamily segoeFamily(L"Segoe UI");
        StringFormat centerFormat;
        centerFormat.SetAlignment(StringAlignmentCenter);
        centerFormat.SetLineAlignment(StringAlignmentCenter);

        Font statusFont(&segoeFamily, 13.0f * s, FontStyleBoldItalic, UnitPixel);
        SolidBrush textBrush(Color(255, 250, 251, 252)); // #FAFBFC

        const wchar_t *statusText = (g_state.displayProgress >= 0.98f)
                                    ? L"STARTING\u2026"
                                    : g_state.statusStep.c_str();

        RectF textRect(10.0f * s, 192.0f * s, (kBaseWidth - 20.0f) * s, 24.0f * s);
        g.DrawString(statusText, -1, &statusFont, textRect, &centerFormat, &textBrush);

        // 4. Exact Discord Progress Bar
        // Track: 180px width, 8px height, 4px radius, #34383E
        // Fill: #4E545E (matching Discord's actual update fill)
        const REAL barW = 180.0f * s;
        const REAL barH = 8.0f * s;
        const REAL barR = 4.0f * s;
        const REAL barX = (static_cast<REAL>(w) - barW) / 2.0f;
        const REAL barY = 232.0f * s;

        GraphicsPath trackPath;
        AddRoundedRect(trackPath, barX, barY, barW, barH, barR);
        SolidBrush trackBrush(Color(255, 52, 56, 62)); // #34383E
        g.FillPath(&trackBrush, &trackPath);

        const REAL fillFraction = std::fmin(1.0f, std::fmax(0.04f, g_state.displayProgress));
        const REAL fillW = std::fmax(barH, barW * fillFraction);
        GraphicsPath fillPath;
        AddRoundedRect(fillPath, barX, barY, fillW, barH, barR);
        SolidBrush fillBrush(Color(255, 78, 84, 94)); // #4E545E
        g.FillPath(&fillBrush, &fillPath);
    }

    POINT ptSrc{ 0, 0 };
    SIZE wndSize{ w, h };
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(g_state.hwnd, screenDc, nullptr, &wndSize, memDc, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(memDc, oldBmp);
    DeleteObject(hBmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

LRESULT CALLBACK SplashWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_LBUTTONDOWN:
        SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;

    case WM_TIMER:
        if (wParam == kTimerId) {
            if (g_state.hParentProcess) {
                if (WaitForSingleObject(g_state.hParentProcess, 0) == WAIT_OBJECT_0) {
                    g_state.finishing = true;
                    g_state.targetProgress = 1.0f;
                }
            }

            static int tickCounter = 0;
            if (++tickCounter % 6 == 0) {
                PollProgressFile();
            }

            if (!g_state.finishing && g_state.targetProgress < 0.94f) {
                g_state.targetProgress += 0.0018f;
            }

            const float diff = g_state.targetProgress - g_state.displayProgress;
            g_state.displayProgress += diff * (g_state.finishing ? 0.22f : 0.08f);

            RenderSplashFrame();

            if (g_state.finishing && g_state.displayProgress >= 0.99f) {
                if (++g_state.finishTicks > 18) { // ~300ms at 100%
                    PostQuitMessage(0);
                }
            }
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, kTimerId);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        if (argc >= 2 && argv[1][0] != L'\0') {
            g_state.progressFilePath = argv[1];
        }
        if (argc >= 3 && argv[2][0] != L'\0') {
            std::wstring ver = argv[2];
            if (!ver.empty() && ver[0] != L'v' && ver[0] != L'V') {
                ver = L"v" + ver;
            }
            g_state.versionText = ver;
        }
        LocalFree(argv);
    }

    const DWORD parentPid = FindParentProcessId();
    if (parentPid != 0) {
        g_state.hParentProcess = OpenProcess(SYNCHRONIZE, FALSE, parentPid);
    }

    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken = 0;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    // Load FLUX v2.png logo (try embedded resource first, then disk fallback)
    HRSRC hRes = FindResourceW(hInstance, L"IDR_V2_PNG", RT_RCDATA);
    if (hRes) {
        DWORD resSize = SizeofResource(hInstance, hRes);
        HGLOBAL hResData = LoadResource(hInstance, hRes);
        if (hResData && resSize > 0) {
            const void *pData = LockResource(hResData);
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, resSize);
            if (hMem) {
                void *pMem = GlobalLock(hMem);
                if (pMem) {
                    std::memcpy(pMem, pData, resSize);
                    GlobalUnlock(hMem);
                    IStream *pStream = nullptr;
                    if (SUCCEEDED(CreateStreamOnHGlobal(hMem, TRUE, &pStream)) && pStream) {
                        g_state.logoImage = Image::FromStream(pStream);
                        pStream->Release();
                    }
                }
            }
        }
    }
    if (!g_state.logoImage) {
        g_state.logoImage = Image::FromFile(L"resources\\v2.png");
        if (g_state.logoImage && g_state.logoImage->GetLastStatus() != Ok) {
            delete g_state.logoImage;
            g_state.logoImage = nullptr;
        }
    }

    // Load FLUX window icon from embedded resource IDI_ICON1
    g_state.hAppIcon = (HICON)LoadImageW(hInstance, L"IDI_ICON1", IMAGE_ICON, 128, 128, LR_DEFAULTCOLOR);
    if (!g_state.hAppIcon) {
        g_state.hAppIcon = (HICON)LoadIconW(hInstance, L"IDI_ICON1");
    }

    // Compute DPI scale for primary monitor
    HDC screenDc = GetDC(nullptr);
    const int dpiX = GetDeviceCaps(screenDc, LOGPIXELSX);
    ReleaseDC(nullptr, screenDc);
    g_state.scale = (dpiX > 0) ? (static_cast<float>(dpiX) / 96.0f) : 1.0f;
    g_state.width  = static_cast<int>(std::round(kBaseWidth * g_state.scale));
    g_state.height = static_cast<int>(std::round(kBaseHeight * g_state.scale));

    const int screenW = GetSystemMetrics(SM_CXSCREEN);
    const int screenH = GetSystemMetrics(SM_CYSCREEN);
    const int posX = (screenW - g_state.width) / 2;
    const int posY = (screenH - g_state.height) / 2;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = SplashWndProc;
    wc.hInstance = hInstance;
    wc.hIcon = g_state.hAppIcon;
    wc.hIconSm = g_state.hAppIcon;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"FluxDiscordSetupSplashClass";
    RegisterClassExW(&wc);

    g_state.hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_APPWINDOW,
        wc.lpszClassName,
        L"FLUX Setup",
        WS_POPUP | WS_VISIBLE,
        posX, posY, g_state.width, g_state.height,
        nullptr, nullptr, hInstance, nullptr);

    if (g_state.hwnd) {
        PollProgressFile();
        RenderSplashFrame();
        ShowWindow(g_state.hwnd, SW_SHOW);
        SetTimer(g_state.hwnd, kTimerId, kFrameIntervalMs, nullptr);

        MSG msg{};
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_state.logoImage) {
        delete g_state.logoImage;
        g_state.logoImage = nullptr;
    }

    if (g_state.hAppIcon) {
        DestroyIcon(g_state.hAppIcon);
        g_state.hAppIcon = nullptr;
    }

    GdiplusShutdown(gdiplusToken);
    if (g_state.hParentProcess) {
        CloseHandle(g_state.hParentProcess);
    }
    return 0;
}
