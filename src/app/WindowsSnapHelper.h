#pragma once

#include <QObject>

class QQuickWindow;

namespace Flux {

class WindowsSnapHelper : public QObject {
    Q_OBJECT
    Q_PROPERTY(int leftInteractiveWidth READ leftInteractiveWidth WRITE setLeftInteractiveWidth NOTIFY leftInteractiveWidthChanged)
    Q_PROPERTY(int rightInteractiveWidth READ rightInteractiveWidth WRITE setRightInteractiveWidth NOTIFY rightInteractiveWidthChanged)
    Q_PROPERTY(bool menuOpen READ menuOpen WRITE setMenuOpen NOTIFY menuOpenChanged)

public:
    static WindowsSnapHelper* instance();

    int leftInteractiveWidth() const { return m_leftInteractiveWidth; }
    void setLeftInteractiveWidth(int w);

    int rightInteractiveWidth() const { return m_rightInteractiveWidth; }
    void setRightInteractiveWidth(int w);

    bool menuOpen() const { return m_menuOpen; }
    void setMenuOpen(bool open);

    void setWindow(QQuickWindow *window);

signals:
    void leftInteractiveWidthChanged();
    void rightInteractiveWidthChanged();
    void menuOpenChanged();
    void requestCloseMenus();

private:
    explicit WindowsSnapHelper(QObject *parent = nullptr);
    int m_leftInteractiveWidth{460};
    int m_rightInteractiveWidth{240};
    bool m_menuOpen{false};
};

void installWindowsSnapHelper(QQuickWindow *window);

} // namespace Flux
