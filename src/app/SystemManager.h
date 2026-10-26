#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QQuickWindow>
#include <memory>

namespace Flux {

class UserStore;
class TrayEventFilter;

class SystemManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool autostart READ isAutostartEnabled WRITE setAutostartEnabled NOTIFY autostartChanged)
    Q_PROPERTY(bool closeToTray READ isCloseToTrayEnabled WRITE setCloseToTrayEnabled NOTIFY closeToTrayChanged)
    Q_PROPERTY(bool startedFromBoot READ startedFromBoot CONSTANT)
    Q_PROPERTY(bool isExiting READ isExiting NOTIFY isExitingChanged)

public:
    explicit SystemManager(UserStore *userStore, bool startedFromBoot, QObject *parent = nullptr);
    ~SystemManager() override;

    void setWindow(QQuickWindow *window);
    QQuickWindow* window() const { return m_window; }

    bool startedFromBoot() const { return m_startedFromBoot; }
    bool isExiting() const { return m_isExiting; }

    bool isAutostartEnabled() const;
    void setAutostartEnabled(bool enabled);

    bool isCloseToTrayEnabled() const;
    void setCloseToTrayEnabled(bool enabled);

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void hideWindow();
    Q_INVOKABLE void exitApp();

    void showContextMenu();
    void reinstallTray();
    unsigned int taskbarCreatedMsg() const { return m_taskbarCreatedMsg; }
    unsigned int activateInstanceMsg() const { return m_activateInstanceMsg; }

signals:
    void autostartChanged(bool enabled);
    void closeToTrayChanged(bool enabled);
    void isExitingChanged();
    void requestedShowWindow();

private:
    void initTray();
    void removeTray();
    void checkAndUpdateAutostartPath();

    UserStore *m_userStore = nullptr;
    bool m_startedFromBoot = false;
    bool m_isExiting = false;
    QPointer<QQuickWindow> m_window;
    std::unique_ptr<TrayEventFilter> m_eventFilter;

#ifdef Q_OS_WIN
    bool m_trayInstalled = false;
    int m_retryCount = 0;
    void *m_nid = nullptr;
    unsigned int m_taskbarCreatedMsg = 0;
    unsigned int m_activateInstanceMsg = 0;
#endif
};

} // namespace Flux
