#include "SystemMonitorWidget.h"
#include "core/Logging.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QLinearGradient>
#include <QDebug>
#include <cmath>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QGuiApplication>
#include <QClipboard>

SystemMonitorWidget::SystemMonitorWidget(SystemMonitor *monitor, QWidget *parent)
    : DWidget(parent)
    , m_monitor(monitor)
{
    setMinimumSize(260, 600);
    setAttribute(Qt::WA_StyledBackground);

    connect(m_monitor, &SystemMonitor::statsUpdated,
            this, &SystemMonitorWidget::onStatsUpdated);
}

void SystemMonitorWidget::onStatsUpdated()
{
    update();
}

void SystemMonitorWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QRect r = rect().adjusted(8, 8, -8, -8);

    // 检查是否需要显示告警条
    bool hasCpuAlert = m_monitor->topProcess().pid > 0;
    bool hasMemPressure = m_monitor->memPressureLevel() >= SystemMonitor::MemPressureSome;
    bool hasTrafficAlert = false;
    if (m_trafficThresholdMB > 0) {
        qint64 dailyMB = (m_monitor->dailyRxBytes() + m_monitor->dailyTxBytes()) / (1024 * 1024);
        hasTrafficAlert = dailyMB >= m_trafficThresholdMB;
    }

    int alertH = 0;
    if (hasCpuAlert) alertH += 24;
    if (hasMemPressure) alertH += 24;
    if (hasTrafficAlert) alertH += 24;

    QRect alertRect(r.left(), r.top(), r.width(), alertH);
    QRect contentRect(r.left(), r.top() + alertH,
                      r.width(), r.height() - alertH);
    int sectionH = contentRect.height() / 4;

    int alertY = r.top();
    if (hasCpuAlert) {
        drawAlertBar(&painter, QRect(r.left(), alertY, r.width(), 24));
        alertY += 24;
    }
    if (hasMemPressure) {
        drawMemPressureBar(&painter, QRect(r.left(), alertY, r.width(), 24));
        alertY += 24;
    }
    if (hasTrafficAlert) {
        drawTrafficAlert(&painter, QRect(r.left(), alertY, r.width(), 24));
    }

    drawCpuGauge(&painter, QRect(contentRect.left(), contentRect.top(),
                                  contentRect.width(), sectionH));
    drawMemBar(&painter, QRect(contentRect.left(), contentRect.top() + sectionH,
                               contentRect.width(), sectionH));
    drawNetGraph(&painter, QRect(contentRect.left(), contentRect.top() + sectionH * 2,
                                 contentRect.width(), sectionH));
    drawTemperature(&painter, QRect(contentRect.left(), contentRect.top() + sectionH * 3,
                                     contentRect.width(), sectionH));
}

// ---------------------------------------------------------------------------
// drawAlertBar: CPU 高占用进程告警条
// ---------------------------------------------------------------------------

void SystemMonitorWidget::drawAlertBar(QPainter *painter, const QRect &rect)
{
    painter->save();

    const auto &proc = m_monitor->topProcess();

    // 告警背景（橙色渐变）
    QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
    grad.setColorAt(0, QColor(255, 152, 0, 220));
    grad.setColorAt(1, QColor(245, 124, 0, 200));
    painter->setPen(Qt::NoPen);
    painter->setBrush(grad);
    painter->drawRoundedRect(rect.adjusted(0, 0, 0, 0), 4, 4);

    // 告警图标（简单三角形感叹号）
    int iconX = rect.left() + 8;
    int iconY = rect.center().y();
    QPainterPath triPath;
    triPath.moveTo(iconX, iconY - 6);
    triPath.lineTo(iconX + 8, iconY + 5);
    triPath.lineTo(iconX - 8, iconY + 5);
    triPath.closeSubpath();
    painter->setBrush(QColor(255, 255, 255, 200));
    painter->drawPath(triPath);
    painter->setPen(QPen(QColor(245, 124, 0), 2));
    painter->drawPoint(iconX, iconY + 1);

    // 告警文字
    QString alertText = QStringLiteral("%1  %2%")
        .arg(proc.name)
        .arg(proc.cpuPercent, 0, 'f', 0);

    // 持续时长标签
    if (proc.sustainedSeconds > 0) {
        int mins = proc.sustainedSeconds / 60;
        int secs = proc.sustainedSeconds % 60;
        if (mins > 0)
            alertText += QStringLiteral("  %1m%2s").arg(mins).arg(secs);
        else
            alertText += QStringLiteral("  %1s").arg(secs);
    }
    painter->setPen(QColor(255, 255, 255));
    QFont alertFont = font();
    alertFont.setPointSizeF(alertFont.pointSizeF() * 0.85);
    alertFont.setBold(true);
    painter->setFont(alertFont);
    painter->drawText(QRect(iconX + 14, rect.top(), rect.width() - 22, rect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, alertText);

    painter->restore();
}

// ---------------------------------------------------------------------------
// drawTrafficAlert: 流量超额预警条
// ---------------------------------------------------------------------------

void SystemMonitorWidget::drawTrafficAlert(QPainter *painter, const QRect &rect)
{
    painter->save();

    qint64 dailyTotal = m_monitor->dailyRxBytes() + m_monitor->dailyTxBytes();
    qint64 dailyMB = dailyTotal / (1024 * 1024);

    // 红色渐变背景
    QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
    grad.setColorAt(0, QColor(231, 76, 60, 220));
    grad.setColorAt(1, QColor(192, 57, 43, 200));
    painter->setPen(Qt::NoPen);
    painter->setBrush(grad);
    painter->drawRoundedRect(rect, 4, 4);

    // 告警图标（圆形感叹号）
    int iconX = rect.left() + 10;
    int iconY = rect.center().y();
    painter->setBrush(QColor(255, 255, 255, 200));
    painter->drawEllipse(iconX - 5, iconY - 5, 10, 10);
    painter->setPen(QPen(QColor(231, 76, 60), 2));
    painter->drawText(QRect(iconX - 5, iconY - 5, 10, 10),
                      Qt::AlignCenter, QStringLiteral("!"));

    // 告警文字
    QString alertText = QStringLiteral("今日流量 %1 MB / 已超 %2 MB")
        .arg(dailyMB)
        .arg(m_trafficThresholdMB);
    painter->setPen(QColor(255, 255, 255));
    QFont alertFont = font();
    alertFont.setPointSizeF(alertFont.pointSizeF() * 0.85);
    alertFont.setBold(true);
    painter->setFont(alertFont);
    painter->drawText(QRect(iconX + 14, rect.top(), rect.width() - 22, rect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, alertText);

    painter->restore();
}

// ---------------------------------------------------------------------------
// drawMemPressureBar: 内存压力预警条
// ---------------------------------------------------------------------------

void SystemMonitorWidget::drawMemPressureBar(QPainter *painter, const QRect &rect)
{
    painter->save();

    auto level = m_monitor->memPressureLevel();

    // 根据等级选择颜色
    QColor baseColor, darkColor;
    QString levelText;
    if (level == SystemMonitor::MemPressureCritical) {
        baseColor = QColor(231, 76, 60, 220);
        darkColor = QColor(192, 57, 43, 200);
        levelText = QStringLiteral("内存严重不足");
    } else if (level == SystemMonitor::MemPressureFull) {
        baseColor = QColor(245, 124, 0, 220);
        darkColor = QColor(230, 100, 0, 200);
        levelText = QStringLiteral("内存压力");
    } else {
        baseColor = QColor(255, 193, 7, 220);
        darkColor = QColor(255, 160, 0, 200);
        levelText = QStringLiteral("内存偏紧");
    }

    // 渐变背景
    QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
    grad.setColorAt(0, baseColor);
    grad.setColorAt(1, darkColor);
    painter->setPen(Qt::NoPen);
    painter->setBrush(grad);
    painter->drawRoundedRect(rect, 4, 4);

    // 图标（内存条形状）
    int iconX = rect.left() + 10;
    int iconY = rect.center().y();
    painter->setBrush(QColor(255, 255, 255, 200));
    painter->drawRoundedRect(iconX - 6, iconY - 4, 12, 8, 1, 1);
    // 内存条上的小线条
    painter->setPen(QPen(QColor(0, 0, 0, 80), 1));
    painter->drawLine(iconX - 4, iconY - 2, iconX + 4, iconY - 2);
    painter->drawLine(iconX - 4, iconY, iconX + 4, iconY);
    painter->drawLine(iconX - 4, iconY + 2, iconX + 4, iconY + 2);

    // 构建告警文字
    QString alertText = levelText;
    float avg10 = m_monitor->memPressureAvg10();
    if (avg10 > 0) {
        alertText += QStringLiteral("  (PSI: %1%)").arg(avg10, 0, 'f', 1);
    }

    // 如果有高内存进程，显示 top 1
    const auto &memProcs = m_monitor->topMemProcesses();
    if (!memProcs.isEmpty()) {
        const auto &top = memProcs.first();
        QString memStr = formatSize(top.rssBytes);
        alertText += QStringLiteral("  %1: %2").arg(top.name).arg(memStr);
    }

    painter->setPen(QColor(255, 255, 255));
    QFont alertFont = font();
    alertFont.setPointSizeF(alertFont.pointSizeF() * 0.85);
    alertFont.setBold(true);
    painter->setFont(alertFont);
    painter->drawText(QRect(iconX + 14, rect.top(), rect.width() - 22, rect.height()),
                      Qt::AlignLeft | Qt::AlignVCenter, alertText);

    painter->restore();
}

void SystemMonitorWidget::drawCpuGauge(QPainter *painter, const QRect &rect)
{
    painter->save();

    int size = qMin(rect.width(), rect.height()) - 20;
    int cx = rect.center().x();
    int cy = rect.top() + size / 2 + 10;
    int radius = size / 2;

    // 背景圆环
    painter->setPen(QPen(QColor(0, 0, 0, 25), 8, Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(cx - radius, cy - radius, size, size, 0, 360 * 16);

    // CPU 使用率弧线 — 渐变色
    float usage = m_monitor->cpuUsage();
    int angle = static_cast<int>(usage * 360 / 100);

    // 渐变色：绿 → 黄 → 橙 → 红
    auto usageToColor = [](float u) -> QColor {
        u = qBound(0.0f, u, 100.0f);
        struct Stop { float val; QColor color; };
        static const Stop stops[] = {
            {0.0f,  QColor(72, 174, 79)},    // 绿
            {50.0f, QColor(245, 200, 50)},   // 黄绿
            {75.0f, QColor(245, 167, 38)},   // 橙黄
            {90.0f, QColor(231, 76, 60)},    // 红
            {100.0f, QColor(200, 40, 40)}    // 深红
        };
        for (int i = 0; i < 4; ++i) {
            if (u <= stops[i + 1].val) {
                float range = stops[i + 1].val - stops[i].val;
                float ratio = range > 0 ? (u - stops[i].val) / range : 0;
                const QColor &c1 = stops[i].color;
                const QColor &c2 = stops[i + 1].color;
                int r = static_cast<int>(c1.red() + (c2.red() - c1.red()) * ratio);
                int g = static_cast<int>(c1.green() + (c2.green() - c1.green()) * ratio);
                int b = static_cast<int>(c1.blue() + (c2.blue() - c1.blue()) * ratio);
                return QColor(r, g, b);
            }
        }
        return QColor(200, 40, 40);
    };

    QColor cpuColor = usageToColor(usage);

    painter->setPen(QPen(cpuColor, 8, Qt::SolidLine, Qt::RoundCap));
    painter->drawArc(cx - radius, cy - radius, size, size, 90 * 16, -angle * 16);

    // 中央文字
    painter->setPen(palette().color(QPalette::Text));
    QFont bigFont = font();
    bigFont.setPointSizeF(bigFont.pointSizeF() * 2.2);
    bigFont.setBold(true);
    painter->setFont(bigFont);
    painter->drawText(QRect(cx - radius, cy - radius, size, size),
                      Qt::AlignCenter,
                      QString::number(static_cast<int>(usage)) + "%");

    // 标签
    painter->setPen(palette().color(QPalette::Text));
    QFont labelFont = font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.8);
    labelFont.setBold(true);
    painter->setFont(labelFont);
    painter->drawText(QRect(rect.left(), rect.top(), rect.width(), 20),
                      Qt::AlignLeft | Qt::AlignTop,
                      QStringLiteral("CPU"));

    // CPU 历史曲线（右侧）
    QRect histRect(cx + radius + 10, cy - radius, rect.right() - cx - radius - 20, size);
    if (histRect.width() > 20) {
        painter->setPen(QPen(QColor(0, 0, 0, 25), 1));
        painter->drawRoundedRect(histRect, 4, 4);

        const auto &history = m_monitor->cpuHistory();
        if (history.size() > 1) {
            QPainterPath path;
            for (int i = 0; i < history.size(); ++i) {
                float x = histRect.left() + i * histRect.width() / (history.size() - 1);
                float y = histRect.bottom() - history[i] * histRect.height() / 100.0f;
                if (i == 0) path.moveTo(x, y);
                else        path.lineTo(x, y);
            }
            painter->setPen(QPen(cpuColor, 1.5));
            painter->drawPath(path);
        }
    }

    painter->restore();
}

void SystemMonitorWidget::drawMemBar(QPainter *painter, const QRect &rect)
{
    painter->save();

    // 标签
    painter->setPen(palette().color(QPalette::Text));
    QFont labelFont = font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.8);
    labelFont.setBold(true);
    painter->setFont(labelFont);
    painter->drawText(QRect(rect.left(), rect.top(), rect.width(), 20),
                      Qt::AlignLeft | Qt::AlignTop,
                      QStringLiteral("内存"));

    float usage = m_monitor->memUsage();
    qint64 total = m_monitor->memTotal();
    qint64 used = m_monitor->memUsed();

    // 数值
    painter->setPen(palette().color(QPalette::Text));
    painter->setFont(font());
    QString memText = QString("%1 / %2  (%3%)")
        .arg(formatSize(used))
        .arg(formatSize(total))
        .arg(static_cast<int>(usage));
    painter->drawText(QRect(rect.left(), rect.top(), rect.width(), 20),
                      Qt::AlignRight | Qt::AlignTop, memText);

    // 进度条
    int barY = rect.top() + 28;
    int barH = 16;
    QRect barRect(rect.left() + 4, barY, rect.width() - 8, barH);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 20));
    painter->drawRoundedRect(barRect, 8, 8);

    // 填充
    QColor memColor;
    if (usage < 60)      memColor = QColor(52, 152, 219);
    else if (usage < 85) memColor = QColor(245, 167, 38);
    else                 memColor = QColor(231, 76, 60);

    int fillW = static_cast<int>((barRect.width() - 4) * usage / 100.0f);
    QRect fillRect(barRect.left() + 2, barRect.top() + 2, fillW, barH - 4);
    painter->setBrush(memColor);
    painter->drawRoundedRect(fillRect, 6, 6);

    painter->restore();
}

void SystemMonitorWidget::drawNetGraph(QPainter *painter, const QRect &rect)
{
    painter->save();

    // 标签
    painter->setPen(palette().color(QPalette::Text));
    QFont labelFont = font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.8);
    labelFont.setBold(true);
    painter->setFont(labelFont);
    painter->drawText(QRect(rect.left(), rect.top(), rect.width(), 20),
                      Qt::AlignLeft | Qt::AlignTop,
                      QStringLiteral("网络"));

    // 速率文本（根据单位模式显示）
    QString netText = QString("↓ %1  ↑ %2")
        .arg(formatSpeed(m_monitor->netDownload()))
        .arg(formatSpeed(m_monitor->netUpload()));
    painter->setFont(font());
    painter->drawText(QRect(rect.left(), rect.top(), rect.width(), 20),
                      Qt::AlignRight | Qt::AlignTop, netText);

    // 曲线图区域
    QRect graphRect(rect.left() + 4, rect.top() + 28, rect.width() - 8, rect.height() - 32);

    painter->setPen(QPen(QColor(0, 0, 0, 20), 1));
    painter->setBrush(QColor(0, 0, 0, 8));
    painter->drawRoundedRect(graphRect, 4, 4);

    // 找最大值用于缩放
    const auto &downHist = m_monitor->netDownHistory();
    const auto &upHist = m_monitor->netUpHistory();

    float maxVal = 1.0f;
    for (float v : downHist) maxVal = qMax(maxVal, v);
    for (float v : upHist)    maxVal = qMax(maxVal, v);
    maxVal = qMax(maxVal, 10.0f);

    // 下载曲线（蓝色填充）
    if (downHist.size() > 1) {
        QPainterPath path;
        QPainterPath fillPath;
        for (int i = 0; i < downHist.size(); ++i) {
            float x = graphRect.left() + i * graphRect.width() / (downHist.size() - 1);
            float y = graphRect.bottom() - downHist[i] * graphRect.height() / maxVal;
            if (i == 0) { path.moveTo(x, y); fillPath.moveTo(x, graphRect.bottom()); }
            else        { path.lineTo(x, y); }
            fillPath.lineTo(x, y);
        }
        fillPath.lineTo(graphRect.right(), graphRect.bottom());
        fillPath.closeSubpath();

        QColor downColor(52, 152, 219, 60);
        painter->setPen(Qt::NoPen);
        painter->setBrush(downColor);
        painter->drawPath(fillPath);

        painter->setPen(QPen(QColor(52, 152, 219), 1.5));
        painter->drawPath(path);
    }

    // 上传曲线（绿色）
    if (upHist.size() > 1) {
        QPainterPath path;
        for (int i = 0; i < upHist.size(); ++i) {
            float x = graphRect.left() + i * graphRect.width() / (upHist.size() - 1);
            float y = graphRect.bottom() - upHist[i] * graphRect.height() / maxVal;
            if (i == 0) path.moveTo(x, y);
            else        path.lineTo(x, y);
        }
        painter->setPen(QPen(QColor(46, 204, 113), 1.5));
        painter->drawPath(path);
    }

    painter->restore();
}

void SystemMonitorWidget::drawTemperature(QPainter *painter, const QRect &rect)
{
    painter->save();

    // 标签
    painter->setPen(palette().color(QPalette::Text));
    QFont labelFont = font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.8);
    labelFont.setBold(true);
    painter->setFont(labelFont);
    painter->drawText(QRect(rect.left(), rect.top(), rect.width(), 20),
                      Qt::AlignLeft | Qt::AlignTop,
                      QStringLiteral("温度"));

    float temp = m_monitor->temperature();

    // 渐变色：根据温度在 30°C~100°C 之间插值
    // 30°C → 蓝(52,152,219) → 50°C → 绿(80,200,80) → 70°C → 黄(255,180,0)
    // → 85°C → 橙(255,120,0) → 100°C → 红(255,60,60)
    auto tempToColor = [](float t) -> QColor {
        // 钳制到 [30, 100]
        t = qBound(30.0f, t, 100.0f);
        // 关键点
        struct ColorStop { float temp; QColor color; };
        static const ColorStop stops[] = {
            {30.0f, QColor(100, 180, 255)},   // 冷蓝
            {50.0f, QColor(80, 200, 80)},     // 绿
            {70.0f, QColor(255, 180, 0)},     // 黄
            {85.0f, QColor(255, 120, 0)},     // 橙
            {100.0f, QColor(255, 60, 60)}     // 红
        };
        for (int i = 0; i < 4; ++i) {
            if (t <= stops[i + 1].temp) {
                float range = stops[i + 1].temp - stops[i].temp;
                float ratio = (t - stops[i].temp) / range;
                const QColor &c1 = stops[i].color;
                const QColor &c2 = stops[i + 1].color;
                int r = static_cast<int>(c1.red() + (c2.red() - c1.red()) * ratio);
                int g = static_cast<int>(c1.green() + (c2.green() - c1.green()) * ratio);
                int b = static_cast<int>(c1.blue() + (c2.blue() - c1.blue()) * ratio);
                return QColor(r, g, b);
            }
        }
        return QColor(255, 60, 60);
    };

    QColor tempColor = tempToColor(temp);

    // 数值
    QFont bigFont = font();
    bigFont.setPointSizeF(bigFont.pointSizeF() * 1.8);
    bigFont.setBold(true);
    painter->setFont(bigFont);

    painter->setPen(tempColor);
    painter->drawText(QRect(rect.left(), rect.top() + 20, rect.width(), 40),
                      Qt::AlignCenter,
                      QString::number(static_cast<int>(temp)) + "°C");

    // 温度计可视化 — 渐变填充
    int barY = rect.top() + 70;
    int barH = 8;
    QRect barRect(rect.left() + 20, barY, rect.width() - 40, barH);

    // 背景
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 20));
    painter->drawRoundedRect(barRect, 4, 4);

    // 渐变填充条
    float tempPercent = qBound(0.0f, (temp - 30.0f) / 70.0f, 1.0f);
    int fillW = static_cast<int>((barRect.width() - 4) * tempPercent);
    QRect fillRect(barRect.left() + 2, barRect.top() + 2, fillW, barH - 4);

    // 用渐变色绘制填充
    QLinearGradient tempGradient(fillRect.left(), 0, fillRect.right(), 0);
    // 根据当前温度范围生成渐变
    float startTemp = 30.0f;
    float endTemp = qMax(temp, 30.0f);
    int segments = 5;
    for (int i = 0; i <= segments; ++i) {
        float t = startTemp + (endTemp - startTemp) * i / segments;
        tempGradient.setColorAt(static_cast<qreal>(i) / segments, tempToColor(t));
    }
    painter->setBrush(tempGradient);
    painter->drawRoundedRect(fillRect, 3, 3);

    // 刻度标记
    QFont tinyFont = font();
    tinyFont.setPointSizeF(tinyFont.pointSizeF() * 0.6);
    painter->setFont(tinyFont);
    painter->setPen(QColor(0, 0, 0, 100));

    // 50°C 刻度
    int mark50 = barRect.left() + 2 + static_cast<int>((barRect.width() - 4) * (50.0f - 30.0f) / 70.0f);
    painter->drawLine(mark50, barRect.bottom(), mark50, barRect.bottom() + 3);
    painter->drawText(QRect(mark50 - 15, barRect.bottom() + 4, 30, 12),
                      Qt::AlignCenter, "50");

    // 70°C 刻度
    int mark70 = barRect.left() + 2 + static_cast<int>((barRect.width() - 4) * (70.0f - 30.0f) / 70.0f);
    painter->drawLine(mark70, barRect.bottom(), mark70, barRect.bottom() + 3);
    painter->drawText(QRect(mark70 - 15, barRect.bottom() + 4, 30, 12),
                      Qt::AlignCenter, "70");

    // 85°C 刻度（警戒线）
    int mark85 = barRect.left() + 2 + static_cast<int>((barRect.width() - 4) * (85.0f - 30.0f) / 70.0f);
    painter->setPen(QPen(QColor(255, 60, 60, 180), 1, Qt::DashLine));
    painter->drawLine(mark85, barRect.top() - 2, mark85, barRect.bottom() + 3);

    painter->restore();
}

QString SystemMonitorWidget::formatSize(qint64 bytes) const
{
    if (bytes < 1024)         return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024)  return QString::number(bytes / 1024) + " KB";
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024 * 1024)) + " MB";
    return QString::number(bytes / (1024LL * 1024 * 1024), 'f', 1) + " GB";
}

// ---------------------------------------------------------------------------
// formatSpeed: 格式化网络速率
// ---------------------------------------------------------------------------
//
// SystemMonitor 存储的速率单位为 KB/s（kilobytes per second）。
// m_useByteUnit = true  -> 显示 KB/s 或 MB/s
// m_useByteUnit = false -> 显示 kbps 或 Mbps（乘以 8）
//
// ---------------------------------------------------------------------------

QString SystemMonitorWidget::formatSpeed(float kbps) const
{
    if (m_useByteUnit) {
        // Byte 模式：KB/s → MB/s
        if (kbps < 1024)
            return QString::number(kbps, 'f', 1) + " KB/s";
        return QString::number(kbps / 1024, 'f', 2) + " MB/s";
    } else {
        // Bit 模式：KB/s × 8 = kbps → Mbps
        float kbps_val = kbps * 8;
        if (kbps_val < 1024)
            return QString::number(kbps_val, 'f', 1) + " kbps";
        return QString::number(kbps_val / 1024, 'f', 2) + " Mbps";
    }
}

// ---------------------------------------------------------------------------
// contextMenuEvent: 右键菜单 — 导出 CSV
// ---------------------------------------------------------------------------

void SystemMonitorWidget::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);

    auto *exportAct = menu.addAction(QStringLiteral("导出历史数据 (CSV)"));

    // CPU 持续告警阈值设置
    auto *sustainedMenu = menu.addMenu(QStringLiteral("CPU持续告警"));
    sustainedMenu->addAction(QStringLiteral("立即告警"), [this]() {
        m_monitor->setCpuSustainedSeconds(0);
    });
    sustainedMenu->addAction(QStringLiteral("持续 5秒"), [this]() {
        m_monitor->setCpuSustainedSeconds(5);
    });
    sustainedMenu->addAction(QStringLiteral("持续 10秒"), [this]() {
        m_monitor->setCpuSustainedSeconds(10);
    });
    sustainedMenu->addAction(QStringLiteral("持续 30秒"), [this]() {
        m_monitor->setCpuSustainedSeconds(30);
    });
    sustainedMenu->addAction(QStringLiteral("持续 60秒"), [this]() {
        m_monitor->setCpuSustainedSeconds(60);
    });

    // 今日流量信息
    qint64 dailyMB = (m_monitor->dailyRxBytes() + m_monitor->dailyTxBytes()) / (1024 * 1024);
    menu.addSeparator();
    auto *infoAct = menu.addAction(
        QStringLiteral("今日流量: %1 MB").arg(dailyMB));
    infoAct->setEnabled(false);

    // 生成日报告
    menu.addSeparator();
    auto *reportAct = menu.addAction(QStringLiteral("生成今日效率报告"));

    QAction *ret = menu.exec(event->globalPos());
    if (ret == exportAct) {
        QString defaultPath = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);
        if (defaultPath.isEmpty())
            defaultPath = QDir::homePath();

        QString fileName = QFileDialog::getSaveFileName(
            this, QStringLiteral("导出 CSV"),
            defaultPath + "/edgebar_history.csv",
            QStringLiteral("CSV 文件 (*.csv)"));

        if (!fileName.isEmpty()) {
            if (m_monitor->exportCsv(fileName)) {
                qCInfo(edgebarLog) << "CSV exported to" << fileName;
            }
        }
    } else if (ret == reportAct) {
        // 生成日报告并显示
        auto report = m_monitor->generateDailyReport();

        QString reportText = QStringLiteral(
            "===== EdgeBar 效率报告 %1 =====\n\n"
            "CPU 平均: %2%  峰值: %3%\n"
            "内存 平均: %4%  峰值: %5%\n"
            "温度 平均: %6°C  最高: %7°C\n"
            "下载: %8 MB  上传: %9 MB\n"
            "峰值时段: %10\n"
            "采样数: %11\n"
            "========================"
        ).arg(report.date.toString("yyyy-MM-dd"))
         .arg(report.cpuAvg, 0, 'f', 1).arg(report.cpuPeak, 0, 'f', 1)
         .arg(report.memAvg, 0, 'f', 1).arg(report.memPeak, 0, 'f', 1)
         .arg(report.tempAvg, 0, 'f', 1).arg(report.tempPeak, 0, 'f', 1)
         .arg(report.netDownTotal / (1024 * 1024))
         .arg(report.netUpTotal / (1024 * 1024))
         .arg(report.peakHour)
         .arg(report.snapshotCount);

        // 复制到剪贴板
        QGuiApplication::clipboard()->setText(reportText);
        qCInfo(edgebarLog) << "Daily report generated:\n" << reportText;
    }
}
