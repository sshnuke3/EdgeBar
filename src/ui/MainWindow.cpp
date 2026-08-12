#include "MainWindow.h"
#include "SystemMonitorWidget.h"
#include "ClipboardWidget.h"
#include "QuickLaunchWidget.h"
#include "PomodoroWidget.h"
#include "plugins/AppLauncher.h"
#include "plugins/SystemCommand.h"

#include <DPlatformHandle>
#include <DGuiApplicationHelper>
#include <DPushButton>

#include <QScreen>
#include <QGuiApplication>
#include <QTimer>
#include <QPropertyAnimation>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QButtonGroup>
#include <QDebug>
#include <QPainter>
#include <QPainterPath>

MainWindow::MainWindow(QWidget *parent)
    : DMainWindow(parent)
{
    // 核心数据
    m_sysMonitor = new SystemMonitor(this);
    m_sysMonitor->start(2000);

    m_clipboard = new ClipboardManager(this);

    setupUI();
    setupWindow();
    setupEdgeTimer();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupUI()
{
    m_centralWidget = new DWidget(this);
    auto *mainLayout = new QVBoxLayout(m_centralWidget);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // ---- 顶部 Tab 栏 ----
    auto *tabBar = new DWidget(m_centralWidget);
    auto *tabLayout = new QHBoxLayout(tabBar);
    tabLayout->setSpacing(2);
    tabLayout->setContentsMargins(6, 6, 6, 2);

    auto *tabGroup = new QButtonGroup(tabBar);
    tabGroup->setExclusive(true);

    auto createTab = [tabBar, tabGroup, tabLayout](const QString &icon, const QString &text) {
        auto *btn = new DPushButton(tabBar);
        btn->setText(text);
        btn->setCheckable(true);
        btn->setFixedHeight(32);
        btn->setStyleSheet(
            "DPushButton { border: none; border-radius: 6px; "
            "  background: transparent; font-size: 12px; }"
            "DPushButton:checked { background: rgba(0,0,0,0.08); }");
        tabLayout->addWidget(btn);
        tabGroup->addButton(btn);
        return btn;
    };

    auto *btnSys  = createTab("monitor",     QStringLiteral("监控"));
    auto *btnClip = createTab("clipboard",   QStringLiteral("剪贴板"));
    auto *btnApp  = createTab("launch",      QStringLiteral("启动"));
    auto *btnPomo = createTab("pomodoro",    QStringLiteral("专注"));

    btnSys->setChecked(true);
    tabLayout->addStretch();

    // ---- 子组件 ----
    m_sysMonitorWidget  = new SystemMonitorWidget(m_sysMonitor, m_centralWidget);
    m_clipboardWidget   = new ClipboardWidget(m_clipboard, m_centralWidget);
    m_quickLaunchWidget = new QuickLaunchWidget(m_centralWidget);
    m_pomodoroWidget    = new PomodoroWidget(m_centralWidget);

    // 注册插件到快速启动
    m_quickLaunchWidget->registerPlugin(new AppLauncher());
    m_quickLaunchWidget->registerPlugin(new SystemCommand());

    // 只显示当前 Tab
    m_clipboardWidget->hide();
    m_quickLaunchWidget->hide();
    m_pomodoroWidget->hide();

    // Tab 切换
    connect(btnSys, &DPushButton::clicked, this, [this]() {
        setActiveTab(SystemTab);
    });
    connect(btnClip, &DPushButton::clicked, this, [this]() {
        setActiveTab(ClipboardTab);
    });
    connect(btnApp, &DPushButton::clicked, this, [this]() {
        setActiveTab(LaunchTab);
    });
    connect(btnPomo, &DPushButton::clicked, this, [this]() {
        setActiveTab(PomodoroTab);
    });

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

    connect(DGuiApplicationHelper::instance(),
            &DGuiApplicationHelper::themeTypeChanged,
            this, [this]() { update(); });
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

void MainWindow::setEdgeSide(EdgeSide side)
{
    m_edgeSide = side;
    if (m_hidden) move(hiddenPosition());
    else          move(shownPosition());
}

void MainWindow::setActiveTab(TabIndex tab)
{
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
}

void MainWindow::slideOut()
{
    m_hidden = true;
    m_slideAnim->stop();
    m_slideAnim->setStartValue(pos());
    m_slideAnim->setEndValue(hiddenPosition());
    m_slideAnim->start();
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
