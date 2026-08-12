#include "SystemMonitorWidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QLinearGradient>
#include <QDebug>
#include <cmath>

SystemMonitorWidget::SystemMonitorWidget(SystemMonitor *monitor, QWidget *parent)
    : DWidget(parent)
    , m_monitor(monitor)
{
    setMinimumSize(260, 560);
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
    int sectionH = r.height() / 4;

    drawCpuGauge(&painter, QRect(r.left(), r.top(), r.width(), sectionH));
    drawMemBar(&painter, QRect(r.left(), r.top() + sectionH, r.width(), sectionH));
    drawNetGraph(&painter, QRect(r.left(), r.top() + sectionH * 2, r.width(), sectionH));
    drawTemperature(&painter, QRect(r.left(), r.top() + sectionH * 3, r.width(), sectionH));
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

    // CPU 使用率弧线
    float usage = m_monitor->cpuUsage();
    int angle = static_cast<int>(usage * 360 / 100);

    QColor cpuColor;
    if (usage < 50)      cpuColor = QColor(72, 174, 79);   // 绿
    else if (usage < 80) cpuColor = QColor(245, 167, 38);  // 橙
    else                 cpuColor = QColor(231, 76, 60);    // 红

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

    // 速率文本
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
    maxVal = qMax(maxVal, 10.0f); // 最小量程

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

    // 数值
    QFont bigFont = font();
    bigFont.setPointSizeF(bigFont.pointSizeF() * 1.8);
    bigFont.setBold(true);
    painter->setFont(bigFont);

    QColor tempColor;
    if (temp < 50)      tempColor = QColor(52, 152, 219);
    else if (temp < 70)  tempColor = QColor(245, 167, 38);
    else                 tempColor = QColor(231, 76, 60);

    painter->setPen(tempColor);
    painter->drawText(QRect(rect.left(), rect.top() + 20, rect.width(), 40),
                      Qt::AlignCenter,
                      QString::number(static_cast<int>(temp)) + "°C");

    // 温度计可视化
    int barY = rect.top() + 70;
    int barH = 8;
    QRect barRect(rect.left() + 20, barY, rect.width() - 40, barH);

    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(0, 0, 0, 20));
    painter->drawRoundedRect(barRect, 4, 4);

    float tempPercent = qBound(0.0f, temp / 100.0f, 1.0f);
    int fillW = static_cast<int>((barRect.width() - 4) * tempPercent);
    QRect fillRect(barRect.left() + 2, barRect.top() + 2, fillW, barH - 4);
    painter->setBrush(tempColor);
    painter->drawRoundedRect(fillRect, 3, 3);

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

QString SystemMonitorWidget::formatSpeed(float kbps) const
{
    if (kbps < 1024)  return QString::number(kbps, 'f', 1) + " KB/s";
    return QString::number(kbps / 1024, 'f', 2) + " MB/s";
}
