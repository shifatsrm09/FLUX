#include "SystemManager.h"
#include "../core/UserStore.h"
#include "../core/Logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QTimer>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <objidl.h>

static void setWindowsStartupDelayZero() {
    HKEY hSerialize = NULL;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Serialize", 
                        0, NULL, REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, NULL, &hSerialize, NULL) == ERROR_SUCCESS) {
        DWORD delay = 0;
        RegSetValueExW(hSerialize, L"StartupDelayInMSec", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&delay), sizeof(DWORD));
        RegCloseKey(hSerialize);
    }
}

static QString getStartupShortcutPath() {
    WCHAR startupPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_STARTUP, NULL, 0, startupPath))) {
        return QString::fromWCharArray(startupPath) + QStringLiteral("\\FLUX.lnk");
    }
    return QString();
}

static void createStartupShortcut(const QString &exePath) {
    const QString linkPath = getStartupShortcutPath();
    if (linkPath.isEmpty()) return;

    CoInitialize(NULL);
    IShellLinkW *psl = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (LPVOID *)&psl))) {
        const std::wstring wExe = exePath.toStdWString();
        psl->SetPath(wExe.c_str());
        psl->SetArguments(L"--autostart");
        const std::wstring wWorkDir = QDir::toNativeSeparators(QCoreApplication::applicationDirPath()).toStdWString();
        psl->SetWorkingDirectory(wWorkDir.c_str());

        IPersistFile *ppf = nullptr;
        if (SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (LPVOID *)&ppf))) {
            const std::wstring wLink = linkPath.toStdWString();
            ppf->Save(wLink.c_str(), TRUE);
            ppf->Release();
        }
        psl->Release();
    }
    CoUninitialize();
}

static void removeStartupShortcut() {
    const QString linkPath = getStartupShortcutPath();
    if (!linkPath.isEmpty() && QFile::exists(linkPath)) {
        QFile::remove(linkPath);
    }
}

#define WM_FLUX_TRAY_MSG (WM_APP + 201)
#define ID_FLUX_TRAY_OPEN 1001
#define ID_FLUX_TRAY_EXIT 1002
#endif

#include <QAbstractNativeEventFilter>

namespace Flux {

class TrayEventFilter : public QAbstractNativeEventFilter {
public:
    explicit TrayEventFilter(SystemManager *manager) : m_manager(manager) {}

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override {
        Q_UNUSED(result);
#ifdef Q_OS_WIN
        if (eventType == "windows_generic_MSG") {
            MSG *msg = reinterpret_cast<MSG *>(message);
            if (!msg) return false;

            if (msg->message == WM_FLUX_TRAY_MSG) {
                UINT uMsg = LOWORD(msg->lParam);
                switch (uMsg) {
                case WM_LBUTTONUP:
                case WM_LBUTTONDBLCLK:
                case 0x0400 /* NIN_SELECT */:
                    m_manager->showWindow();
                    return true;
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    m_manager->showContextMenu();
                    return true;
                }
            } else if (!m_manager->isMultiInstance() && m_manager->activateInstanceMsg() && msg->message == m_manager->activateInstanceMsg()) {
                m_manager->showWindow();
                return true;
            } else if (!m_manager->isMultiInstance() && m_manager->taskbarCreatedMsg() && msg->message == m_manager->taskbarCreatedMsg()) {
                m_manager->reinstallTray();
                return true;
            }
        }
#else
        Q_UNUSED(eventType);
        Q_UNUSED(message);
#endif
        return false;
    }

private:
    SystemManager *m_manager;
};

SystemManager::SystemManager(UserStore *userStore, bool startedFromBoot, bool isMultiInstance, QObject *parent)
    : QObject(parent)
    , m_userStore(userStore)
    , m_startedFromBoot(startedFromBoot)
    , m_isMultiInstance(isMultiInstance)
{
    if (m_isMultiInstance) {
        // Multi-instance: quit when window is closed, never install tray icon, never touch autostart
        QGuiApplication::setQuitOnLastWindowClosed(true);
        return;
    }

    // Ensure Qt does not quit when the window is hidden to tray
    QGuiApplication::setQuitOnLastWindowClosed(false);

    m_eventFilter = std::make_unique<TrayEventFilter>(this);
    QCoreApplication::instance()->installNativeEventFilter(m_eventFilter.get());

#ifdef Q_OS_WIN
    m_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");
    m_activateInstanceMsg = RegisterWindowMessageW(L"FLUX_ACTIVATE_INSTANCE");

    // Enable autostart by default on first run, or update path if already enabled
    if (m_userStore) {
        if (!m_userStore->boolValue(QStringLiteral("system/autostart_initialized"), false)) {
            setAutostartEnabled(true);
            m_userStore->setValue(QStringLiteral("system/autostart_initialized"), true);
        } else if (isAutostartEnabled()) {
            checkAndUpdateAutostartPath();
        }
    } else {
        if (isAutostartEnabled()) {
            checkAndUpdateAutostartPath();
        }
    }
#endif
}

SystemManager::~SystemManager() {
    if (m_eventFilter) {
        QCoreApplication::instance()->removeNativeEventFilter(m_eventFilter.get());
    }
    removeTray();
}

void SystemManager::setWindow(QQuickWindow *window) {
    if (m_window == window) return;
    if (m_window) {
        removeTray();
    }
    m_window = window;
    if (m_window) {
        initTray();
        connect(m_window, &QWindow::visibilityChanged, this, [this](QWindow::Visibility v) {
            if (!m_trayInstalled && v != QWindow::Hidden) {
                m_retryCount = 0;
                initTray();
            }
        });
    }
}

bool SystemManager::isAutostartEnabled() const {
#ifdef Q_OS_WIN
    HKEY hKey = NULL;
    bool inRun = false;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD type = 0;
        DWORD size = 0;
        LONG res = RegQueryValueExW(hKey, L"FLUX", NULL, &type, NULL, &size);
        RegCloseKey(hKey);
        inRun = (res == ERROR_SUCCESS && size > 0);
    }
    const QString linkPath = getStartupShortcutPath();
    bool inStartupFolder = (!linkPath.isEmpty() && QFile::exists(linkPath));
    return inRun || inStartupFolder;
#else
    return false;
#endif
}

void SystemManager::setAutostartEnabled(bool enabled) {
#ifdef Q_OS_WIN
    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (enabled) {
            QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            QString cmd = QString("\"%1\" --autostart").arg(exePath);
            const std::wstring wCmd = cmd.toStdWString();
            RegSetValueExW(hKey, L"FLUX", 0, REG_SZ, reinterpret_cast<const BYTE*>(wCmd.c_str()), static_cast<DWORD>((wCmd.size() + 1) * sizeof(wchar_t)));
            setWindowsStartupDelayZero();
            createStartupShortcut(exePath);
            FLUX_LOG_INFO("System", "Autostart registered in Windows Run & Startup Folder: " + cmd);
        } else {
            RegDeleteValueW(hKey, L"FLUX");
            removeStartupShortcut();
            FLUX_LOG_INFO("System", "Autostart unregistered from Windows Run & Startup Folder");
        }
        RegCloseKey(hKey);
        emit autostartChanged(enabled);
    }
#else
    Q_UNUSED(enabled);
#endif
}

bool SystemManager::isCloseToTrayEnabled() const {
    if (m_isMultiInstance) return false;
    if (m_userStore) {
        return m_userStore->boolValue(QStringLiteral("system/close_to_tray"), true);
    }
    return true;
}

void SystemManager::setCloseToTrayEnabled(bool enabled) {
    if (m_userStore) {
        m_userStore->setValue(QStringLiteral("system/close_to_tray"), enabled);
    }
    emit closeToTrayChanged(enabled);
}

void SystemManager::showWindow() {
    emit requestedShowWindow();
    if (m_window) {
        m_window->show();
        m_window->raise();
        m_window->requestActivate();
#ifdef Q_OS_WIN
        HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
        if (hwnd) {
            if (IsIconic(hwnd)) {
                ShowWindow(hwnd, SW_RESTORE);
            } else {
                ShowWindow(hwnd, SW_SHOW);
            }
            DWORD foreThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
            DWORD appThread = GetCurrentThreadId();
            if (foreThread != appThread) {
                AttachThreadInput(foreThread, appThread, TRUE);
                BringWindowToTop(hwnd);
                SetForegroundWindow(hwnd);
                AttachThreadInput(foreThread, appThread, FALSE);
            } else {
                BringWindowToTop(hwnd);
                SetForegroundWindow(hwnd);
            }
        }
#endif
    }
}

void SystemManager::hideWindow() {
    if (m_window) {
        m_window->hide();
    }
}

void SystemManager::exitApp() {
    m_isExiting = true;
    emit isExitingChanged();
    removeTray();
    QCoreApplication::quit();
}

void SystemManager::showContextMenu() {
#ifdef Q_OS_WIN
    if (!m_window) return;
    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());

    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_STRING, ID_FLUX_TRAY_OPEN, L"Open FLUX");
    SetMenuDefaultItem(hMenu, ID_FLUX_TRAY_OPEN, FALSE);
    InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    InsertMenuW(hMenu, 2, MF_BYPOSITION | MF_STRING, ID_FLUX_TRAY_EXIT, L"Exit FLUX");

    if (hwnd) {
        SetForegroundWindow(hwnd);
    }
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd ? hwnd : GetDesktopWindow(), NULL);
    DestroyMenu(hMenu);

    if (cmd == ID_FLUX_TRAY_OPEN) {
        showWindow();
    } else if (cmd == ID_FLUX_TRAY_EXIT) {
        exitApp();
    }
#endif
}

void SystemManager::initTray() {
#ifdef Q_OS_WIN
    if (m_isMultiInstance || m_trayInstalled || !m_window) return;

    HWND hwnd = reinterpret_cast<HWND>(m_window->winId());
    if (!hwnd) return;

    NOTIFYICONDATAW *nid = new NOTIFYICONDATAW();
    memset(nid, 0, sizeof(NOTIFYICONDATAW));
    nid->cbSize = sizeof(NOTIFYICONDATAW);
    nid->hWnd = hwnd;
    nid->uID = 1;
    nid->uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid->uCallbackMessage = WM_FLUX_TRAY_MSG;

    QString appDir = QCoreApplication::applicationDirPath();
    QString icoPath = QDir(appDir).filePath(QStringLiteral("resources/flux.ico"));
    HICON hIcon = (HICON)LoadImageW(NULL, reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(icoPath).utf16()),
                                   IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_LOADFROMFILE);
    if (!hIcon) {
        hIcon = (HICON)GetClassLongPtrW(hwnd, GCLP_HICONSM);
    }
    if (!hIcon) {
        hIcon = LoadIconW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1));
    }
    nid->hIcon = hIcon;
    wcscpy_s(nid->szTip, L"FLUX Media Player");

    if (Shell_NotifyIconW(NIM_ADD, nid)) {
        m_trayInstalled = true;
        m_nid = nid;
        FLUX_LOG_INFO("System", "System tray icon successfully installed");
    } else {
        delete nid;
        if (m_retryCount < 10) {
            ++m_retryCount;
            QTimer::singleShot(1000, this, [this]() {
                initTray();
            });
        } else {
            FLUX_LOG_WARN("System", "Failed to install system tray icon after retries");
        }
    }
#endif
}

void SystemManager::reinstallTray() {
#ifdef Q_OS_WIN
    removeTray();
    initTray();
#endif
}

void SystemManager::removeTray() {
#ifdef Q_OS_WIN
    if (m_trayInstalled && m_nid) {
        NOTIFYICONDATAW *nid = reinterpret_cast<NOTIFYICONDATAW *>(m_nid);
        Shell_NotifyIconW(NIM_DELETE, nid);
        if (nid->hIcon) {
            DestroyIcon(nid->hIcon);
        }
        delete nid;
        m_nid = nullptr;
        m_trayInstalled = false;
        FLUX_LOG_INFO("System", "System tray icon removed");
    }
#endif
}

void SystemManager::checkAndUpdateAutostartPath() {
#ifdef Q_OS_WIN
    QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    setWindowsStartupDelayZero();
    createStartupShortcut(exePath);

    HKEY hKey = NULL;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ | KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        wchar_t buffer[1024] = {0};
        DWORD size = sizeof(buffer);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, L"FLUX", NULL, &type, reinterpret_cast<LPBYTE>(buffer), &size) == ERROR_SUCCESS) {
            QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
            QString expectedCmd = QString("\"%1\" --autostart").arg(exePath);
            QString currentVal = QString::fromWCharArray(buffer);
            if (currentVal != expectedCmd) {
                const std::wstring wCmd = expectedCmd.toStdWString();
                RegSetValueExW(hKey, L"FLUX", 0, REG_SZ, reinterpret_cast<const BYTE*>(wCmd.c_str()), static_cast<DWORD>((wCmd.size() + 1) * sizeof(wchar_t)));
                FLUX_LOG_INFO("System", "Updated autostart command line in Windows Run: " + expectedCmd);
            }
        }
        RegCloseKey(hKey);
    }
#endif
}

void SystemManager::launchSecondInstance() {
    QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    FLUX_LOG_INFO("System", "Spawning secondary FLUX instance via: " + exePath);
    QProcess::startDetached(exePath, QStringList() << QStringLiteral("--multi"));
}

} // namespace Flux
