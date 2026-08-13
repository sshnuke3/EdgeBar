#include "DesktopWidget.h"
#include "core/SystemMonitor.h"
#include "PomodoroWidget.h"
#include "HealthReminderWidget.h"

#include <QPainterPath>
#include <QPaintEvent>
#include <QMenu>
#include <QAction>
#include <QLinearGradient>
#include <cmath>
#include <QGuiApplication>
#include <QScreen>

// ---------------------------------------------------------------------------
// DesktopWidget
// ---------------------------------------------------------------------------

DesktopWidget::DesktopWidget(SystemMonitor *monitor,
                             PomodoroWidget *pomodoro,
                             HealthReminderWidget *health,
                             QWidget *parent)
    : QWidget(parent)
    , m_monitor(monitor)
    , m_pomodoro(pomodoro)
    , m_health(health)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    setFixedSize(COMPACT_SIZE, COMPACT_SIZE);

    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(1000);
    connect(m_updateTimer, &QTimer::timeout, this, &DesktopWidget::onTick);
    m_updateTimer->start();

    // 初始放置在屏幕右侧中间
    auto *screen = QGuiApplication::primaryScreen();
    if (screen) {
        QRect sg = screen->availableGeometry();
        move(sg.right() - COMPACT_SIZE - 20, sg.center().y() - COMPACT_SIZE / 2);
    }
}

void DesktopWidget::setMode(WidgetMode mode)
{
    m_mode = mode;
    update();
}

void DesktopWidget::setExpanded(bool expanded)
{
    if (m_expanded == expanded) return;
    m_expanded = expanded;

    int size = expanded ? EXPANDED_SIZE : COMPACT_SIZE;
    setFixedSize(size, size);
    update();
}

// ---------------------------------------------------------------------------
// paintEvent
// ---------------------------------------------------------------------------

void DesktopWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    // 背景：圆角毛玻璃效果
    QRect r = rect().adjusted(2, 2, -2, -2);

    QPainterPath bgPath;
    bgPath.addRoundedRect(r, 16, 16);

    // 半透明背景
    QColor bgColor(30, 30, 30, m_expanded ? 200 : 180);
    painter.fillPath(bgPath, bgColor);

    // 边框
    painter.setPen(QPen(QColor(255, 255, 255, 30), 1));
    painter.drawPath(bgPath);

    // 根据模式绘制内容
    switch (m_mode) {
    case CpuGaugeMode:
        drawCpuGauge(&painter, r);
        break;
    case PomodoroMode:
        drawPomodoro(&painter, r);
        break;
    case WaterMode:
        drawWater(&painter, r);
        break;
    }
}

void DesktopWidget::drawCpuGauge(QPainter *painter, const QRect &rect)
{
    if (!m_monitor) return;

    float usage = m_monitor->cpuUsage();

    // 渐变色
    auto usageToColor = [](float u) -> QColor {
        u = qBound(0.0f, u, 100.0f);
        if (u < 50) {
            float r = u / 50;
            return QColor(72 + (245 - 72) * r, 174 + (200 - 174) * r, 79 + (50 - 79) * r);
        } else if (u < 75) {
            float r = (u - 50) / 25;
            return QColor(245 + (245 - 245) * r, 200 + (167 - 200) * r, 50 + (38 - 50) * r);
        } else if (u < 90) {
            float r = (u - 75) / 15;
            return QColor(245 + (231 - 245) * r, 167 + (76 - 167) * r, 38 + (60 - 38) * r);
        } else {
            float r = qBound(0.0f, (u - 90) / 10, 1.0f);
            return QColor(231 + (200 - 231) * r, 76 + (40 - 76) * r, 60);
        }
    };

    QColor arcColor = usageToColor(usage);

    int cx = rect.center().x();
    int cy = rect.center().y();
    int radius = m_expanded ? 42 : 28;

    // 背景圆
    painter->setPen(QPen(QColor(255, 255, 255, 25), 4));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(cx, cy), radius, radius);

    // 进度弧
    int startAngle = 90 * 16;
    int spanAngle = static_cast<int>(-usage * 360 / 100 * 16);
    painter->setPen(QPen(arcColor, 5, Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(cx - radius, cy - radius, radius * 2, radius * 2,
                     startAngle, spanAngle);

    // CPU 数值
    QFont valFont;
    valFont.setBold(true);
    valFont.setPointSizeF(m_expanded ? 14 : 10);
    painter->setFont(valFont);
    painter->setPen(QColor(255, 255, 255, 230));
    painter->drawText(QRect(cx - radius, cy - radius, radius * 2, radius * 2),
                      Qt::AlignCenter,
                      QString::number(static_cast<int>(usage)) + "%");

    // 标签
    QFont labelFont;
    labelFont.setPointSizeF(m_expanded ? 7 : 5);
    painter->setFont(labelFont);
    painter->setPen(QColor(255, 255, 255, 100));
    painter->drawText(QRect(rect.left(), rect.bottom() - 16, rect.width(), 14),
                      Qt::AlignCenter, QStringLiteral("CPU"));

    // 展开时显示更多信息
    if (m_expanded) {
        float mem = m_monitor->memUsage();
        float temp = m_monitor->temperature();

        QFont infoFont;
        infoFont.setPointSizeF(6);
        painter->setFont(infoFont);
        painter->setPen(QColor(255, 255, 255, 150));

        painter->drawText(QRect(rect.left(), rect.top() + 6, rect.width(), 14),
                          Qt::AlignCenter,
                          QStringLiteral("内存 %1%").arg(static_cast<int>(mem)));
        painter->drawText(QRect(rect.left(), rect.top() + 18, rect.width(), 14),
                          Qt::AlignCenter,
                          QStringLiteral("温度 %1°C").arg(static_cast<int>(temp)));
    }
}

void DesktopWidget::drawPomodoro(QPainter *painter, const QRect &rect)
{
    if (!m_pomodoro) return;

    int remaining = m_pomodoro->remaining();
    bool isBreak = m_pomodoro->isBreak();

    int cx = rect.center().x();
    int cy = rect.center().y();
    int radius = m_expanded ? 42 : 28;

    // 颜色
    QColor mainColor = isBreak ? QColor(46, 204, 113) : QColor(231, 76, 60);

    // 背景圆
    painter->setPen(QPen(QColor(255, 255, 255, 25), 4));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QPointF(cx, cy), radius, radius);

    // 进度弧（假设 25 分钟 = 1500 秒）
    int totalSeconds = isBreak ? 300 : 1500;  // 5 分钟休息或 25 分钟专注
    float progress = totalSeconds > 0
        ? 1.0f - static_cast<float>(remaining) / totalSeconds
        : 0;
    progress = qBound(0.0f, progress, 1.0f);

    int startAngle = 90 * 16;
    int spanAngle = static_cast<int>(-progress * 360 * 16);
    painter->setPen(QPen(mainColor, 5, Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(cx - radius, cy - radius, radius * 2, radius * 2,
                     startAngle, spanAngle);

    // 倒计时数字
    int mins = remaining / 60;
    int secs = remaining % 60;
    QString timeStr = QString::number(mins).rightJustified(2, '0') + ":" +
                      QString::number(secs).rightJustified(2, '0');

    QFont valFont;
    valFont.setBold(true);
    valFont.setPointSizeF(m_expanded ? 11 : 8);
    painter->setFont(valFont);
    painter->setPen(QColor(255, 255, 255, 230));
    painter->drawText(QRect(cx - radius, cy - radius, radius * 2, radius * 2),
                      Qt::AlignCenter, timeStr);

    // 标签
    QFont labelFont;
    labelFont.setPointSizeF(m_expanded ? 7 : 5);
    painter->setFont(labelFont);
    painter->setPen(QColor(255, 255, 255, 100));
    painter->drawText(QRect(rect.left(), rect.bottom() - 16, rect.width(), 14),
                      Qt::AlignCenter,
                      isBreak ? QStringLiteral("休息") : QStringLiteral("专注"));
}

void DesktopWidget::drawWater(QPainter *painter, const QRect &rect)
{
    // 简化版：显示一个水杯图标 + 倒计时
    // 实际从 HealthReminderWidget 读取数据需要接口，这里用简化实现

    int cx = rect.center().x();
    int cy = rect.center().y();
    int cupW = m_expanded ? 40 : 28;
    int cupH = m_expanded ? 50 : 36;
    int cupX = cx - cupW / 2;
    int cupY = cy - cupH / 2 - 4;

    // 杯子形状
    QPainterPath cupPath;
    cupPath.moveTo(cupX, cupY);
    cupPath.lineTo(cupX + cupW, cupY);
    cupPath.lineTo(cupX + cupW - 3, cupY + cupH);
    cupPath.lineTo(cupX + 3, cupY + cupH);
    cupPath.closeSubpath();

    painter->setPen(QPen(QColor(100, 180, 255, 120), 2));
    painter->setBrush(QColor(100, 180, 255, 20));
    painter->drawPath(cupPath);

    // 水位（假设 45 分钟间隔，简化显示）
    // 用当前分钟数取模来模拟进度
    int currentMin = QTime::currentTime().minute();
    int interval = 45;
    float progress = (currentMin % interval) / static_cast<float>(interval);
    float waterLevel = 1.0f - progress;

    int fillH = static_cast<int>((cupH - 6) * waterLevel);
    if (fillH > 0) {
        painter->save();
        painter->setClipPath(cupPath);
        QLinearGradient waterGrad(0, cupY + cupH - 3 - fillH, 0, cupY + cupH - 3);
        waterGrad.setColorAt(0, QColor(52, 152, 219, 180));
        waterGrad.setColorAt(1, QColor(41, 128, 185, 220));
        painter->setBrush(waterGrad);
        painter->setPen(Qt::NoPen);
        painter->drawRect(cupX + 2, cupY + cupH - 3 - fillH, cupW - 4, fillH + 3);
        painter->restore();
    }

    // 倒计时文字
    int remainingMin = interval - (currentMin % interval);
    QFont valFont;
    valFont.setBold(true);
    valFont.setPointSizeF(m_expanded ? 9 : 7);
    painter->setFont(valFont);

    QColor textColor = waterLevel < 0.2f ? QColor(231, 76, 60) : QColor(255, 255, 255, 200);
    painter->setPen(textColor);
    painter->drawText(QRect(rect.left(), rect.bottom() - 18, rect.width(), 14),
                      Qt::AlignCenter,
                      QString::number(remainingMin) + QStringLiteral(" min"));

    // 标签
    QFont labelFont;
    labelFont.setPointSizeF(m_expanded ? 7 : 5);
    painter->setFont(labelFont);
    painter->setPen(QColor(255, 255, 255, 100));
    painter->drawText(QRect(rect.left(), rect.top() + 4, rect.width(), 14),
                      Qt::AlignCenter, QStringLiteral("喝水"));
}

// ---------------------------------------------------------------------------
// 事件处理
// ---------------------------------------------------------------------------

void DesktopWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void DesktopWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        move(event->globalPos() - m_dragOffset);
        event->accept();
    }
}

void DesktopWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        event->accept();
    }
}

void DesktopWidget::enterEvent(QEvent *event)
{
    setExpanded(true);
    QWidget::enterEvent(event);
}

void DesktopWidget::leaveEvent(QEvent *event)
{
    setExpanded(false);
    QWidget::leaveEvent(event);
}

void DesktopWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        // 双击切换模式
        int next = (static_cast<int>(m_mode) + 1) % 3;
        setMode(static_cast<WidgetMode>(next));
    }
}

void DesktopWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    auto *cpuAct = menu.addAction(QStringLiteral("CPU 仪表"));
    cpuAct->setCheckable(true);
    cpuAct->setChecked(m_mode == CpuGaugeMode);

    auto *pomoAct = menu.addAction(QStringLiteral("番茄钟"));
    pomoAct->setCheckable(true);
    pomoAct->setChecked(m_mode == PomodoroMode);

    auto *waterAct = menu.addAction(QStringLiteral("喝水进度"));
    waterAct->setCheckable(true);
    waterAct->setChecked(m_mode == WaterMode);

    menu.addSeparator();

    auto *returnAct = menu.addAction(QStringLiteral("返回面板"));
    auto *closeAct = menu.addAction(QStringLiteral("关闭小组件"));

    QAction *ret = menu.exec(event->globalPos());
    if (ret == cpuAct) {
        setMode(CpuGaugeMode);
    } else if (ret == pomoAct) {
        setMode(PomodoroMode);
    } else if (ret == waterAct) {
        setMode(WaterMode);
    } else if (ret == returnAct) {
        emit requestReturnToPanel();
    } else if (ret == closeAct) {
        hide();
    }
}

void DesktopWidget::onTick()
{
    update();
}
