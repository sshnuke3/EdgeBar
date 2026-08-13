#include "MainWindow.h"
#include "SystemMonitorWidget.h"
#include "ClipboardWidget.h"
#include "QuickLaunchWidget.h"
#include "PomodoroWidget.h"
#include "MiniCountdown.h"
#include "plugins/AppLauncher.h"
#include "plugins/SystemCommand.h"
#include "core/Logging.h"

#include <DPlatformHandle>
#include <DGuiApplicationHelper>
#include <DPushButton>
#include <DConfig>
#include <DPalette>

#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>
#include <QIcon>

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
    loadConfig();
    applyThemeColors();

    // 主题变化时重新着色
    connect(DGuiApplicationHelper::instance(),
            &DGuiApplicationHelper::themeTypeChanged,
            this, [this]() {
                applyThemeColors();
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

    qCInfo(edgebarLog) << "Config loaded:"
                       << "edge=" << edgeSide
                       << "autoHide=" << autoHide
                       << "interval=" << interval
                       << "maxItems=" << maxItems
                       << "images=" << enableImages
                       << "netUnit=" << netUnit
                       << "cpuThreshold=" << cpuThreshold
                       << "trafficMB=" << trafficMB;
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

    for (int i = 0; i < 4; ++i) {
        if (m_tabButtons[i])
            m_tabButtons[i]->setStyleSheet(style);
    }
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
    };

    for (int i = 0; i < 4; ++i) {
        auto *btn = new DPushButton(tabBar);
        btn->setIcon(QIcon::fromTheme(QString(tabs[i].iconName)));
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

    // 注册插件
    m_quickLaunchWidget->registerPlugin(new AppLauncher());
    m_quickLaunchWidget->registerPlugin(new SystemCommand());

    // 只显示当前 Tab
    m_clipboardWidget->hide();
    m_quickLaunchWidget->hide();
    m_pomodoroWidget->hide();

    // Tab 切换
    connect(m_tabButtons[0], &DPushButton::clicked, this, [this]() { setActiveTab(SystemTab); });
    connect(m_tabButtons[1], &DPushButton::clicked, this, [this]() { setActiveTab(ClipboardTab); });
    connect(m_tabButtons[2], &DPushButton::clicked, this, [this]() { setActiveTab(LaunchTab); });
    connect(m_tabButtons[3], &DPushButton::clicked, this, [this]() { setActiveTab(PomodoroTab); });

    mainLayout->addWidget(tabBar);
    mainLayout->addWidget(m_sysMonitorWidget, 1);
    mainLayout->addWidget(m_clipboardWidget, 1);
    mainLayout->addWidget(m_quickLaunchWidget, 1);
    mainLayout->addWidget(m_pomodoroWidget, 1);

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

    switch (tab) {
    case SystemTab:    m_sysMonitorWidget->show();  break;
    case ClipboardTab: m_clipboardWidget->show();    break;
    case LaunchTab:    m_quickLaunchWidget->show();  break;
    case PomodoroTab:  m_pomodoroWidget->show();     break;
    }

    // 更新按钮选中状态
    for (int i = 0; i < 4; ++i) {
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
                (static_cast<int>(m_currentTab) + 3) % 4));
        } else {
            // Tab: 正向
            setActiveTab(static_cast<TabIndex>(
                (static_cast<int>(m_currentTab) + 1) % 4));
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
