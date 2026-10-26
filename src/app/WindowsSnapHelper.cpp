#include "WindowsSnapHelper.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QQuickWindow>
#include <QPointer>
#include "../core/Logger.h"

namespace Flux {

static WindowsSnapHelper *s_snapInstance = nullptr;

class WindowsSnapNativeFilter : public QAbstractNativeEventFilter {
public:
    explicit WindowsSnapNativeFilter(QQuickWindow *window, WindowsSnapHelper *helper)
        : m_window(window), m_helper(helper)
    {
        if (!m_window) return;
        m_hwnd = reinterpret_cast<HWND>(m_window->winId());
        applyWin32Styles();

        // Re-apply styles if visibility changes (e.g. from hidden startup popup to normal window)
        QObject::connect(m_window, &QWindow::visibilityChanged, m_window, [this](QWindow::Visibility visibility) {
            if (visibility != QWindow::Hidden && visibility != QWindow::Minimized) {
                applyWin32Styles();
            }
        });
    }

    void applyWin32Styles() {
        if (!m_hwnd || !m_window) return;
        if (IsIconic(m_hwnd)) return;

        bool isFullScreen = (m_window->visibility() == QWindow::FullScreen);
        LONG_PTR style = GetWindowLongPtr(m_hwnd, GWL_STYLE);

        if (isFullScreen) {
            // In fullscreen, remove framing and caption styles so DWM / OS doesn't render borders or margins
            LONG_PTR frameStyles = WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
            LONG_PTR targetStyle = (style & ~frameStyles) | WS_POPUP;
            if (style != targetStyle) {
                SetWindowLongPtr(m_hwnd, GWL_STYLE, targetStyle);
            }
            MARGINS margins = { 0, 0, 0, 0 };
            DwmExtendFrameIntoClientArea(m_hwnd, &margins);
            SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                         SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            return;
        }

        // Windowed / Maximized:
        LONG_PTR required = WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
        LONG_PTR targetStyle = (style | required) & ~WS_POPUP;
        if (style != targetStyle) {
            SetWindowLongPtr(m_hwnd, GWL_STYLE, targetStyle);
        }

        MARGINS margins = { 0, 0, 0, 1 };
        DwmExtendFrameIntoClientArea(m_hwnd, &margins);

        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, 0,
                     SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        if (eventType != "windows_generic_MSG" || !message || !m_hwnd || !m_window) {
            return false;
        }

        MSG *msg = static_cast<MSG *>(message);
        if (msg->hwnd != m_hwnd) {
            return false;
        }

        switch (msg->message) {
        case WM_NCCALCSIZE: {
            if (msg->wParam == TRUE) {
                if (m_window && m_window->visibility() == QWindow::FullScreen) {
                    HMONITOR hMonitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
                    if (hMonitor) {
                        MONITORINFO mi;
                        mi.cbSize = sizeof(MONITORINFO);
                        if (GetMonitorInfo(hMonitor, &mi)) {
                            NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                            params->rgrc[0] = mi.rcMonitor;
                        }
                    }
                } else if (IsZoomed(m_hwnd)) {
                    HMONITOR hMonitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
                    if (hMonitor) {
                        MONITORINFO mi;
                        mi.cbSize = sizeof(MONITORINFO);
                        if (GetMonitorInfo(hMonitor, &mi)) {
                            NCCALCSIZE_PARAMS *params = reinterpret_cast<NCCALCSIZE_PARAMS *>(msg->lParam);
                            params->rgrc[0] = mi.rcWork;
                        }
                    }
                }
                // Returning 0 ensures the entire client area is given to Qt Quick
                // so no default OS title bar or border artifacts are drawn.
                *result = 0;
                return true;
            }
            break;
        }

        case WM_GETMINMAXINFO: {
            // Clamp maximized window size to monitor's work area
            MINMAXINFO *mmi = reinterpret_cast<MINMAXINFO *>(msg->lParam);
            HMONITOR hMonitor = MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST);
            if (hMonitor) {
                MONITORINFO mi;
                mi.cbSize = sizeof(MONITORINFO);
                if (GetMonitorInfo(hMonitor, &mi)) {
                    if (m_window && m_window->visibility() == QWindow::FullScreen) {
                        RECT rcMonitor = mi.rcMonitor;
                        mmi->ptMaxPosition.x = 0;
                        mmi->ptMaxPosition.y = 0;
                        mmi->ptMaxSize.x = rcMonitor.right - rcMonitor.left;
                        mmi->ptMaxSize.y = rcMonitor.bottom - rcMonitor.top;
                        mmi->ptMaxTrackSize.x = mmi->ptMaxSize.x;
                        mmi->ptMaxTrackSize.y = mmi->ptMaxSize.y;
                    } else {
                        RECT rcWork = mi.rcWork;
                        RECT rcMonitor = mi.rcMonitor;
                        mmi->ptMaxPosition.x = rcWork.left - rcMonitor.left;
                        mmi->ptMaxPosition.y = rcWork.top - rcMonitor.top;
                        mmi->ptMaxSize.x = rcWork.right - rcWork.left;
                        mmi->ptMaxSize.y = rcWork.bottom - rcWork.top;
                        mmi->ptMaxTrackSize.x = mmi->ptMaxSize.x;
                        mmi->ptMaxTrackSize.y = mmi->ptMaxSize.y;
                    }
                }
            }
            *result = 0;
            return true;
        }

        case WM_NCLBUTTONDOWN:
        case WM_NCRBUTTONDOWN: {
            if (m_helper && m_helper->menuOpen()) {
                emit m_helper->requestCloseMenus();
            }
            break;
        }

        case WM_NCHITTEST: {
            if (m_window->visibility() == QWindow::FullScreen) {
                return false;
            }

            if (m_helper && m_helper->menuOpen()) {
                *result = HTCLIENT;
                return true;
            }

            POINT pt = { GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam) };
            ScreenToClient(m_hwnd, &pt);

            qreal dpr = m_window->devicePixelRatio();
            int logicalX = static_cast<int>(pt.x / dpr);
            int logicalY = static_cast<int>(pt.y / dpr);
            int winWidth = m_window->width();
            int winHeight = m_window->height();

            // 1. Handle native resizing borders when not maximized
            if (!IsZoomed(m_hwnd)) {
                int border = 6;
                if (logicalY < border) {
                    if (logicalX < border * 2) { *result = HTTOPLEFT; return true; }
                    if (logicalX > winWidth - border * 2) { *result = HTTOPRIGHT; return true; }
                    *result = HTTOP;
                    return true;
                }
                if (logicalY > winHeight - border) {
                    if (logicalX < border * 2) { *result = HTBOTTOMLEFT; return true; }
                    if (logicalX > winWidth - border * 2) { *result = HTBOTTOMRIGHT; return true; }
                    *result = HTBOTTOM;
                    return true;
                }
                if (logicalX < border) {
                    *result = HTLEFT;
                    return true;
                }
                if (logicalX > winWidth - border) {
                    *result = HTRIGHT;
                    return true;
                }
            }

            // 2. Title bar area (top 48 pixels)
            if (logicalY >= 0 && logicalY < 48) {

                // If cursor is in the empty caption areas:
                // a) Left margin before logo (x >= 6 && x < 30)
                // b) Open center space between left nav and right controls
                int leftBound = m_helper ? m_helper->leftInteractiveWidth() : 460;
                int rightBound = winWidth - (m_helper ? m_helper->rightInteractiveWidth() : 240);

                if ((logicalX >= 6 && logicalX < 30) || (logicalX >= leftBound && logicalX < rightBound)) {
                    *result = HTCAPTION;
                    return true;
                }
            }

            break;
        }

        case WM_NCACTIVATE: {
            // Prevent Windows from redrawing a non-client frame upon activation / deactivation
            *result = TRUE;
            return true;
        }
        }

        // Return false to let Qt and Windows DefWindowProc process all other messages naturally
        return false;
    }

private:
    QPointer<QQuickWindow> m_window;
    WindowsSnapHelper *m_helper{nullptr};
    HWND m_hwnd{nullptr};
};

WindowsSnapHelper* WindowsSnapHelper::instance() {
    if (!s_snapInstance) {
        s_snapInstance = new WindowsSnapHelper();
    }
    return s_snapInstance;
}

WindowsSnapHelper::WindowsSnapHelper(QObject *parent)
    : QObject(parent)
{
}

void WindowsSnapHelper::setLeftInteractiveWidth(int w) {
    if (m_leftInteractiveWidth != w) {
        m_leftInteractiveWidth = w;
        emit leftInteractiveWidthChanged();
    }
}

void WindowsSnapHelper::setRightInteractiveWidth(int w) {
    if (m_rightInteractiveWidth != w) {
        m_rightInteractiveWidth = w;
        emit rightInteractiveWidthChanged();
    }
}

void WindowsSnapHelper::setMenuOpen(bool open) {
    if (m_menuOpen != open) {
        m_menuOpen = open;
        emit menuOpenChanged();
    }
}

void WindowsSnapHelper::setWindow(QQuickWindow *window) {
    if (!window) return;
    auto *filter = new WindowsSnapNativeFilter(window, this);
    QCoreApplication::instance()->installNativeEventFilter(filter);
    FLUX_LOG_INFO("WindowsSnap", "Windows snap & layout helper installed successfully.");
}

void installWindowsSnapHelper(QQuickWindow *window) {
    WindowsSnapHelper::instance()->setWindow(window);
}

} // namespace Flux

#else

namespace Flux {

WindowsSnapHelper* WindowsSnapHelper::instance() {
    static WindowsSnapHelper dummy;
    return &dummy;
}

WindowsSnapHelper::WindowsSnapHelper(QObject *parent) : QObject(parent) {}
void WindowsSnapHelper::setLeftInteractiveWidth(int) {}
void WindowsSnapHelper::setRightInteractiveWidth(int) {}
void WindowsSnapHelper::setWindow(QQuickWindow *) {}
void installWindowsSnapHelper(QQuickWindow *) {}

} // namespace Flux

#endif
