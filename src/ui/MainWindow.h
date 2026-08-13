#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <DMainWindow>
#include <DWidget>
#include <DPushButton>

#include <QPropertyAnimation>

#include <DConfig>

#include "core/SystemMonitor.h"
#include "core/ClipboardManager.h"

DWIDGET_USE_NAMESPACE

class SystemMonitorWidget;
class ClipboardWidget;
class QuickLaunchWidget;
class PomodoroWidget;
class MiniCountdown;
class HealthReminderWidget;
class DesktopWidget;

/**
 * @brief 桌边栏主窗口
 *
 * 屏幕边缘滑出的智能面板：
 *  - 无边框 + 置顶 + DTK 毛玻璃
 *  - 鼠标靠近边缘自动滑入，离开自动滑出
 *  - 顶部 Tab 切换：系统监控 / 剪贴板 / 快速启动 / 番茄钟
 *  - DConfig 持久化配置（边缘位置、自动隐藏、刷新间隔等）
 */
class MainWindow : public DMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum EdgeSide { LeftEdge, RightEdge };
    enum TabIndex { SystemTab, ClipboardTab, LaunchTab, PomodoroTab, HealthTab };

    void setEdgeSide(EdgeSide side);
    void setActiveTab(TabIndex tab);

    /// 自动隐藏开关
    void setAutoHide(bool enabled);
    bool autoHide() const { return m_autoHide; }

protected:
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void checkMousePosition();
    void onMiniCountdownClicked();

private:
    void setupUI();
    void setupWindow();
    void setupEdgeTimer();
    void setupMiniCountdown();
    void setupDesktopWidget();
    void setupGlobalShortcuts();
    void loadConfig();
    void applyThemeColors();
    void applyWallpaperForTheme();

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
    HealthReminderWidget *m_healthWidget = nullptr;
    MiniCountdown        *m_miniCountdown = nullptr;
    DesktopWidget        *m_desktopWidget = nullptr;

    // 核心数据
    SystemMonitor    *m_sysMonitor = nullptr;
    ClipboardManager *m_clipboard = nullptr;
    Dtk::Core::DConfig *m_config = nullptr;

    // Tab 按钮
    DPushButton *m_tabButtons[5] = {nullptr};

    // 边缘隐藏
    EdgeSide    m_edgeSide = RightEdge;
    bool        m_autoHide = true;
    bool        m_hidden = false;
    QTimer      *m_edgeTimer = nullptr;
    QPropertyAnimation *m_slideAnim = nullptr;

    // 当前 Tab
    TabIndex m_currentTab = SystemTab;
};

#endif // MAINWINDOW_H
