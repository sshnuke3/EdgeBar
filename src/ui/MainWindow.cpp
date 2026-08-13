#include "MainWindow.h"
#include "SystemMonitorWidget.h"
#include "ClipboardWidget.h"
#include "QuickLaunchWidget.h"
#include "PomodoroWidget.h"
#include "HealthReminderWidget.h"
#include "MiniCountdown.h"
#include "DesktopWidget.h"
#include "plugins/AppLauncher.h"
#include "plugins/SystemCommand.h"
#include "core/Logging.h"

#include <DPlatformHandle>
#include <DGuiApplicationHelper>
#include <DPushButton>
#include <DConfig>
#include <DPalette>
#include <DDBusSender>

#include "core/IconHelper.h"
#include "core/NotificationManager.h"
#include "core/AutostartManager.h"

#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QKeyEvent>
#include <QShortcut>
#include <QPainter>
#include <QPainterPath>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

DCORE_USE_NAMESPACE

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
{
    // 核心数据
    m_sysMonitor = new SystemMonitor(this);
    m_clipboard  = new ClipboardManager(this);

    setupUI();
    setupWindow();
    setupEdgeTimer();
    setupMiniCountdown();
    setupDesktopWidget();
    setupGlobalShortcuts();
    loadConfig();
    applyThemeColors();
    setupNotificationManager();
    setupAutostart();

    // 主题变化时重新着色
    connect(DGuiApplicationHelper::instance(),
            &DGuiApplicationHelper::themeTypeChanged,
            this, [this]() {
                applyThemeColors();
                applyWallpaperForTheme();
                update();
            });
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
// loadConfig: 从 DConfig 读取持久化配置
// ---------------------------------------------------------------------------

void MainWindow::loadConfig()
{
    m_config = new DConfig(QStringLiteral("edgebar"), QString(), this);

    if (!m_config->isValid()) {
        qCWarning(edgebarLog) << "DConfig not available, using defaults";
        m_sysMonitor->start(2000);
        return;
    }

    // 边缘位置
    QString edgeSide = m_config->value("edgeSide", "right").toString();
    setEdgeSide(edgeSide == "left" ? LeftEdge : RightEdge);

    // 自动隐藏
    bool autoHide = m_config->value("autoHide", true).toBool();
    setAutoHide(autoHide);

    // 监控刷新间隔
    int interval = m_config->value("monitorInterval", 2000).toInt();
    m_sysMonitor->start(qBound(500, interval, 10000));

    // 剪贴板最大条数
    int maxItems = m_config->value("maxClipboardItems", 50).toInt();
    m_clipboard->setMaxItems(qBound(5, maxItems, 200));

    // 剪贴板图片历史开关
    bool enableImages = m_config->value("enableClipboardImages", true).toBool();
    m_clipboard->setEnableImages(enableImages);

    // 网速单位：byte (KB/s) 或 bit (kbps)
    QString netUnit = m_config->value("netSpeedUnit", "byte").toString();
    m_sysMonitorWidget->setUseByteUnit(netUnit != "bit");

    // CPU 告警阈值
    int cpuThreshold = m_config->value("cpuAlertThreshold", 80).toInt();
    m_sysMonitor->setCpuAlertThreshold(qBound(0, cpuThreshold, 100));

    // 流量预警阈值
    int trafficMB = m_config->value("trafficThresholdMB", 0).toInt();
    m_sysMonitor->setTrafficThresholdMB(qBound(0, trafficMB, 100000));
    m_sysMonitorWidget->setTrafficThresholdMB(qBound(0, trafficMB, 100000));

    // CPU 持续告警秒数
    int cpuSustained = m_config->value("cpuSustainedSeconds", 10).toInt();
    m_sysMonitor->setCpuSustainedSeconds(qBound(0, cpuSustained, 300));

    // 内存压力阈值
    int memThreshold = m_config->value("memPressureThreshold", 85).toInt();
    m_sysMonitor->setMemPressureThreshold(qBound(0, memThreshold, 100));

    // 健康提醒间隔
    int waterInterval = m_config->value("waterIntervalMin", 45).toInt();
    m_healthWidget->setWaterInterval(qBound(15, waterInterval, 180));

    int standInterval = m_config->value("standIntervalMin", 60).toInt();
    m_healthWidget->setStandInterval(qBound(15, standInterval, 240));

    qCInfo(edgebarLog) << "Config loaded:"
                       << "edge=" << edgeSide
                       << "autoHide=" << autoHide
                       << "interval=" << interval
                       << "maxItems=" << maxItems
                       << "images=" << enableImages
                       << "netUnit=" << netUnit
                       << "cpuThreshold=" << cpuThreshold
                       << "cpuSustained=" << cpuSustained
                       << "memThreshold=" << memThreshold
                       << "trafficMB=" << trafficMB
                       << "waterInterval=" << waterInterval
                       << "standInterval=" << standInterval;
}

// ---------------------------------------------------------------------------
// applyThemeColors: 根据深色/浅色主题调整 Tab 样式
// ---------------------------------------------------------------------------

void MainWindow::applyThemeColors()
{
    bool dark = (DGuiApplicationHelper::instance()->themeType()
                 == DGuiApplicationHelper::DarkType);

    QString normalBg = dark ? "rgba(255,255,255,0.03)" : "rgba(0,0,0,0.03)";
    QString checkedBg = dark ? "rgba(255,255,255,0.10)" : "rgba(0,0,0,0.08)";
    QString textColor = dark ? "rgba(255,255,255,0.85)" : "rgba(0,0,0,0.85)";
    QString checkedText = dark ? "rgba(255,255,255,1.0)" : "rgba(0,0,0,1.0)";

    QString style = QString(
        "DPushButton { border: none; border-radius: 6px; "
        "  background: %1; font-size: 11px; color: %2; "
        "  padding: 2px 6px; }"
        "DPushButton:checked { background: %3; color: %4; }"
        "DPushButton:hover { background: %5; }"
    ).arg(normalBg, textColor, checkedBg, checkedText,
         dark ? "rgba(255,255,255,0.06)" : "rgba(0,0,0,0.05)");

    for (int i = 0; i < 5; ++i) {
        if (m_tabButtons[i])
            m_tabButtons[i]->setStyleSheet(style);
    }
}

// ---------------------------------------------------------------------------
// applyWallpaperForTheme: 根据深色/浅色模式切换壁纸
// ---------------------------------------------------------------------------
//
// 通过 DConfig 读取用户配置的深色/浅色壁纸路径。
// 如果配置了不同壁纸，通过 dbus 调用 deepin-wm 设置。
// 响应论坛产品建议帖：用户希望深浅色模式切换时壁纸也跟着变。
//
// ---------------------------------------------------------------------------

void MainWindow::applyWallpaperForTheme()
{
    if (!m_config || !m_config->isValid()) return;

    bool dark = (DGuiApplicationHelper::instance()->themeType()
                 == DGuiApplicationHelper::DarkType);

    // 从 DConfig 读取壁纸配置
    QString darkWallpaper = m_config->value("darkWallpaper").toString();
    QString lightWallpaper = m_config->value("lightWallpaper").toString();

    QString targetWallpaper = dark ? darkWallpaper : lightWallpaper;

    if (targetWallpaper.isEmpty()) {
        qCDebug(edgebarLog) << "No custom wallpaper configured for"
                            << (dark ? "dark" : "light") << "theme";
        return;
    }

    // 通过 DDBusSender 调用 deepin Appearance 接口设置壁纸
    QDBusPendingCall call = DDBusSender()
        .service("com.deepin.dde.Appearance1")
        .path("/com/deepin/dde/Appearance1")
        .interface("com.deepin.dde.Appearance1")
        .method("SetWallpaper")
        .arg(QString("current"))
        .arg(targetWallpaper)
        .call();

    auto *watcher = new QDBusPendingCallWatcher(call, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, targetWallpaper, dark](QDBusPendingCallWatcher *w) {
        QDBusPendingReply<void> reply = *w;
        if (reply.isError()) {
            qCWarning(edgebarLog) << "Failed to change wallpaper:"
                                   << reply.error().message();
        } else {
            qCInfo(edgebarLog) << "Wallpaper changed to:" << targetWallpaper
                                << "for" << (dark ? "dark" : "light") << "theme";
        }
        w->deleteLater();
    });
}

// ---------------------------------------------------------------------------
// setupUI
// ---------------------------------------------------------------------------

void MainWindow::setupUI()
{
    m_centralWidget = new DWidget(this);
    auto *mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ---- 顶部 Tab 栏 ----
    auto *tabBar = new DWidget(m_centralWidget);
    tabBar->setFixedHeight(40);
    auto *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setSpacing(2);
    tabLayout->setContentsMargins(6, 4, 6, 2);

    auto *tabGroup = new QButtonGroup(tabBar);
    tabGroup->setExclusive(true);

    struct TabDef {
        const char *iconName;
        const char *text;
    };
    const TabDef tabs[] = {
        {"utilities-system-monitor", QT_TRANSLATE_NOOP("MainWindow", "监控")},
        {"edit-paste",               QT_TRANSLATE_NOOP("MainWindow", "剪贴板")},
        {"system-search",            QT_TRANSLATE_NOOP("MainWindow", "启动")},
        {"clock",                    QT_TRANSLATE_NOOP("MainWindow", "专注")},
        {"preferences-system-health", QT_TRANSLATE_NOOP("MainWindow", "健康")},
    };

    for (int i = 0; i < 5; ++i) {
        auto *btn = new DPushButton(tabBar);
        btn->setIcon(edgebarFindIcon(QString(tabs[i].iconName)));
        btn->setIconSize(QSize(14, 14));
        btn->setText(QString::fromUtf8(tabs[i].text));
        btn->setCheckable(true);
        btn->setFixedHeight(32);
        btn->setToolTip(QString::fromUtf8(tabs[i].text));
        tabLayout->addWidget(btn);
        tabGroup->addButton(btn);
        m_tabButtons[i] = btn;
    }

    m_tabButtons[0]->setChecked(true);
    tabLayout->addStretch();

    // ---- 子组件 ----
    m_sysMonitorWidget  = new SystemMonitorWidget(m_sysMonitor, m_centralWidget);
    m_clipboardWidget   = new ClipboardWidget(m_clipboard, m_centralWidget);
    m_quickLaunchWidget = new QuickLaunchWidget(m_centralWidget);
    m_pomodoroWidget    = new PomodoroWidget(m_centralWidget);
    m_healthWidget      = new HealthReminderWidget(m_centralWidget);

    // 注册插件
    m_quickLaunchWidget->registerPlugin(new AppLauncher());
    m_quickLaunchWidget->registerPlugin(new SystemCommand());

    // 只显示当前 Tab
    m_clipboardWidget->hide();
    m_quickLaunchWidget->hide();
    m_pomodoroWidget->hide();
    m_healthWidget->hide();

    // Tab 切换
    connect(m_tabButtons[0], &DPushButton::clicked, this, [this]() { setActiveTab(SystemTab); });
    connect(m_tabButtons[1], &DPushButton::clicked, this, [this]() { setActiveTab(ClipboardTab); });
    connect(m_tabButtons[2], &DPushButton::clicked, this, [this]() { setActiveTab(LaunchTab); });
    connect(m_tabButtons[3], &DPushButton::clicked, this, [this]() { setActiveTab(PomodoroTab); });
    connect(m_tabButtons[4], &DPushButton::clicked, this, [this]() { setActiveTab(HealthTab); });

    mainLayout->addWidget(tabBar);
    mainLayout->addWidget(m_sysMonitorWidget, 1);
    mainLayout->addWidget(m_clipboardWidget, 1);
    mainLayout->addWidget(m_quickLaunchWidget, 1);
    mainLayout->addWidget(m_pomodoroWidget, 1);
    mainLayout->addWidget(m_healthWidget, 1);

    setCentralWidget(m_centralWidget);
}

void MainWindow::setupWindow()
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool);

    setFixedSize(280, 640);

    DPlatformHandle handle(windowHandle());
    handle.setWindowRadius(18);
    handle.setShadowRadius(60);
    handle.setShadowOffset(QPoint(0, 10));
    handle.setShadowColor(QColor(0, 0, 0, 50));
    handle.setBorderWidth(1);
    handle.setBorderColor(QColor(0, 0, 0, 25));
    handle.setTranslucentBackground(true);
    handle.setEnableBlurWindow(true);
    handle.setEnableSystemMove(false);
    handle.setEnableSystemResize(false);
}

void MainWindow::setupEdgeTimer()
{
    m_edgeTimer = new QTimer(this);
    m_edgeTimer->setInterval(300);
    connect(m_edgeTimer, &QTimer::timeout, this, &MainWindow::checkMousePosition);
    m_edgeTimer->start();

    m_slideAnim = new QPropertyAnimation(this, "pos", this);
    m_slideAnim->setDuration(200);
    m_slideAnim->setEasingCurve(QEasingCurve::OutCubic);
}

// ---------------------------------------------------------------------------
// setupMiniCountdown: 初始化番茄钟迷你浮窗
// ---------------------------------------------------------------------------

void MainWindow::setupMiniCountdown()
{
    m_miniCountdown = new MiniCountdown();
    m_miniCountdown->hide();

    connect(m_miniCountdown, &MiniCountdown::clicked,
            this, &MainWindow::onMiniCountdownClicked);

    // 定时更新倒计时显示
    QTimer *miniTimer = new QTimer(this);
    miniTimer->setInterval(1000);
    connect(miniTimer, &QTimer::timeout, this, [this]() {
        if (!m_miniCountdown || !m_miniCountdown->isVisible() || !m_pomodoroWidget)
            return;
        m_miniCountdown->setRemaining(m_pomodoroWidget->remaining());
        m_miniCountdown->setBreakMode(m_pomodoroWidget->isBreak());
    });
    miniTimer->start();
}

void MainWindow::onMiniCountdownClicked()
{
    slideIn();
}

// ---------------------------------------------------------------------------
// setupDesktopWidget: 初始化桌面迷你小组件
// ---------------------------------------------------------------------------

void MainWindow::setupDesktopWidget()
{
    m_desktopWidget = new DesktopWidget(m_sysMonitor, m_pomodoroWidget,
                                        m_healthWidget);
    m_desktopWidget->hide();

    // 双击返回面板
    connect(m_desktopWidget, &DesktopWidget::requestReturnToPanel,
            this, [this]() {
        slideIn();
        m_desktopWidget->hide();
    });
}

// ---------------------------------------------------------------------------
// setupGlobalShortcuts: 全局快捷键
//   Super+E      — 唤出/隐藏 EdgeBar 面板
//   Super+Shift+D — 切换桌面小组件显示
// ---------------------------------------------------------------------------

void MainWindow::setupGlobalShortcuts()
{
    // Super+E: 唤出/隐藏面板
    auto *toggleShortcut = new QShortcut(QKeySequence("Meta+E"), this);
    toggleShortcut->setContext(Qt::ApplicationShortcut);
    connect(toggleShortcut, &QShortcut::activated, this, [this]() {
        if (m_hidden) {
            slideIn();
        } else {
            slideOut();
        }
    });

    // Super+Shift+D: 切换桌面小组件
    auto *desktopShortcut = new QShortcut(QKeySequence("Meta+Shift+D"), this);
    desktopShortcut->setContext(Qt::ApplicationShortcut);
    connect(desktopShortcut, &QShortcut::activated, this, [this]() {
        if (m_desktopWidget->isVisible()) {
            m_desktopWidget->hide();
        } else {
            m_desktopWidget->show();
        }
    });

    // Ctrl+1~5: 快速切换 Tab
    for (int i = 0; i < 5; ++i) {
        auto *tabShortcut = new QShortcut(
            QKeySequence(QString("Ctrl+%1").arg(i + 1)), this);
        tabShortcut->setContext(Qt::ApplicationShortcut);
        connect(tabShortcut, &QShortcut::activated, this, [this, i]() {
            if (!m_hidden) {
                setActiveTab(static_cast<TabIndex>(i));
            }
        });
    }
}

// ---------------------------------------------------------------------------
// setupNotificationManager: 初始化桌面通知管理器
// ---------------------------------------------------------------------------

void MainWindow::setupNotificationManager()
{
    m_notifier = new NotificationManager(this);

    // 从 DConfig 读取通知开关
    if (m_config && m_config->isValid()) {
        bool notifyEnabled = m_config->value("notificationsEnabled", true).toBool();
        bool soundEnabled = m_config->value("soundEnabled", true).toBool();
        m_notifier->setEnabled(notifyEnabled);
        m_notifier->setSoundEnabled(soundEnabled);
    }

    // 连接信号源
    m_notifier->connectSystemMonitor(m_sysMonitor);
    m_notifier->connectHealthReminder(m_healthWidget);
    m_notifier->connectPomodoro(m_pomodoroWidget);
}

// ---------------------------------------------------------------------------
// setupAutostart: 初始化开机自启动
// ---------------------------------------------------------------------------

void MainWindow::setupAutostart()
{
    m_autostart = new AutostartManager(this);

    if (m_config && m_config->isValid()) {
        bool autostartEnabled = m_config->value("autostartEnabled", false).toBool();

        // 同步 DConfig 设置与实际 .desktop 文件
        if (autostartEnabled && !m_autostart->isEnabled()) {
            m_autostart->enable();
        } else if (!autostartEnabled && m_autostart->isEnabled()) {
            m_autostart->disable();
        }
    }
}

void MainWindow::setEdgeSide(EdgeSide side)
{
    m_edgeSide = side;
    if (m_hidden) move(hiddenPosition());
    else          move(shownPosition());
}

void MainWindow::setAutoHide(bool enabled)
{
    m_autoHide = enabled;
    if (!enabled && m_hidden) slideIn();
}

void MainWindow::setActiveTab(TabIndex tab)
{
    m_currentTab = tab;

    m_sysMonitorWidget->hide();
    m_clipboardWidget->hide();
    m_quickLaunchWidget->hide();
    m_pomodoroWidget->hide();
    m_healthWidget->hide();

    switch (tab) {
    case SystemTab:    m_sysMonitorWidget->show();  break;
    case ClipboardTab: m_clipboardWidget->show();    break;
    case LaunchTab:    m_quickLaunchWidget->show();  break;
    case PomodoroTab:  m_pomodoroWidget->show();     break;
    case HealthTab:    m_healthWidget->show();       break;
    }

    // 更新按钮选中状态
    for (int i = 0; i < 5; ++i) {
        if (m_tabButtons[i])
            m_tabButtons[i]->setChecked(i == static_cast<int>(tab));
    }
}

void MainWindow::enterEvent(QEvent *event)
{
    if (m_autoHide && m_hidden) slideIn();
    DMainWindow::enterEvent(event);
}

void MainWindow::leaveEvent(QEvent *event)
{
    DMainWindow::leaveEvent(event);
}

// ---------------------------------------------------------------------------
// keyPressEvent: 键盘快捷键
//   Esc    — 隐藏面板
//   Tab   — 切换到下一个 Tab
//   Ctrl+Tab — 反向切换
// ---------------------------------------------------------------------------

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        slideOut();
        break;
    case Qt::Key_Tab:
        if (event->modifiers() & Qt::ControlModifier) {
            // Ctrl+Tab: 反向
            setActiveTab(static_cast<TabIndex>(
                (static_cast<int>(m_currentTab) + 4) % 5));
        } else {
            // Tab: 正向
            setActiveTab(static_cast<TabIndex>(
                (static_cast<int>(m_currentTab) + 1) % 5));
        }
        break;
    default:
        DMainWindow::keyPressEvent(event);
    }
}

void MainWindow::checkMousePosition()
{
    if (!m_autoHide) return;

    QPoint mouse = QCursor::pos();
    QRect screen = screenGeometry();

    bool nearEdge = false;
    if (m_edgeSide == LeftEdge)
        nearEdge = mouse.x() <= screen.left() + 4;
    else
        nearEdge = mouse.x() >= screen.right() - 4;

    if (nearEdge && m_hidden)
        slideIn();
    else if (!nearEdge && !m_hidden && !geometry().contains(mouse))
        slideOut();
}

void MainWindow::slideIn()
{
    m_hidden = false;
    m_slideAnim->stop();
    m_slideAnim->setStartValue(pos());
    m_slideAnim->setEndValue(shownPosition());
    m_slideAnim->start();
    // 面板滑入时隐藏迷你倒计时
    if (m_miniCountdown) m_miniCountdown->hide();
}

void MainWindow::slideOut()
{
    m_hidden = true;
    m_slideAnim->stop();
    m_slideAnim->setStartValue(pos());
    m_slideAnim->setEndValue(hiddenPosition());
    m_slideAnim->start();
    // 面板滑出时显示迷你倒计时（仅番茄钟运行时）
    // 简化方案：总是显示，由 PomodoroWidget 更新内容
    if (m_miniCountdown && m_currentTab == PomodoroTab) {
        QRect sg = screenGeometry();
        if (m_edgeSide == RightEdge) {
            m_miniCountdown->move(sg.right() - 28, sg.center().y() - 25);
        } else {
            m_miniCountdown->move(4, sg.center().y() - 25);
        }
        m_miniCountdown->show();
    }
}

QRect MainWindow::screenGeometry() const
{
    auto *screen = QGuiApplication::primaryScreen();
    return screen ? screen->availableGeometry() : QRect(0, 0, 1920, 1080);
}

QPoint MainWindow::hiddenPosition() const
{
    QRect screen = screenGeometry();
    int y = screen.top() + (screen.height() - height()) / 2;
    if (m_edgeSide == LeftEdge)
        return QPoint(screen.left() - width() + 4, y);
    else
        return QPoint(screen.right() - 4, y);
}

QPoint MainWindow::shownPosition() const
{
    QRect screen = screenGeometry();
    int y = screen.top() + (screen.height() - height()) / 2;
    if (m_edgeSide == LeftEdge)
        return QPoint(screen.left() + 4, y);
    else
        return QPoint(screen.right() - width() - 4, y);
}
