#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <DWidget>

#include "core/SystemMonitor.h"
#include "core/ClipboardManager.h"

DWIDGET_USE_NAMESPACE

class SystemMonitorWidget;
class ClipboardWidget;
class QuickLaunchWidget;
class PomodoroWidget;

/**
 * @brief 桌边栏主窗口
 *
 * 屏幕边缘滑出的智能面板：
 *  - 无边框 + 置顶 + DTK 毛玻璃
 *  - 鼠标靠近边缘自动滑入，离开自动滑出
 *  - 顶部 Tab 切换：系统监控 / 剪贴板 / 快速启动 / 番茄钟
 */
class MainWindow : public DMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum EdgeSide { LeftEdge, RightEdge };
    enum TabIndex { SystemTab, ClipboardTab, LaunchTab, PomodoroTab };

    void setEdgeSide(EdgeSide side);
    void setActiveTab(TabIndex tab);

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;

private slots:
    void checkMousePosition();

private:
    void setupUI();
    void setupWindow();
    void setupEdgeTimer();
    QPoint hiddenPosition() const;
    QPoint shownPosition() const;
    QRect  screenGeometry() const;
    void slideIn();
    void slideOut();

    DWidget *m_centralWidget = nullptr;

    // 子组件
    SystemMonitorWidget  *m_sysMonitorWidget = nullptr;
    ClipboardWidget      *m_clipboardWidget = nullptr;
    QuickLaunchWidget    *m_quickLaunchWidget = nullptr;
    PomodoroWidget       *m_pomodoroWidget = nullptr;

    // 核心数据
    SystemMonitor    *m_sysMonitor = nullptr;
    ClipboardManager *m_clipboard = nullptr;

    // 边缘隐藏
    EdgeSide    m_edgeSide = RightEdge;
    bool        m_autoHide = true;
    bool        m_hidden = false;
    QTimer      *m_edgeTimer = nullptr;
    class QPropertyAnimation *m_slideAnim = nullptr;
};

#endif // MAINWINDOW_H
