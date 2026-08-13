#include "MiniCountdown.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QMouseEvent>
#include <QFont>

MiniCountdown::MiniCountdown(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setFixedSize(24, 50);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
}

void MiniCountdown::setRemaining(int seconds)
{
    if (m_remaining == seconds) return;
    m_remaining = seconds;
    update();
}

void MiniCountdown::setBreakMode(bool isBreak)
{
    m_isBreak = isBreak;
    update();
}

void MiniCountdown::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 背景条
    QColor bgColor = m_isBreak ? QColor(46, 204, 113, 200) : QColor(231, 76, 60, 200);
    QPainterPath bgPath;
    bgPath.addRoundedRect(rect().adjusted(0, 0, 0, 0), 4, 4);
    painter.fillPath(bgPath, bgColor);

    // 倒计时文字（MM:SS 格式，竖排简化为只显示分钟）
    int mins = m_remaining / 60;
    int secs = m_remaining % 60;

    QFont font;
    font.setPointSizeF(7);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(QColor(255, 255, 255));

    // 显示 "MM" 在上，"SS" 在下
    QString minStr = QString::number(mins).rightJustified(2, '0');
    QString secStr = QString::number(secs).rightJustified(2, '0');
    painter.drawText(QRect(0, 4, width(), 20),
                     Qt::AlignCenter, minStr);
    painter.drawText(QRect(0, 24, width(), 20),
                     Qt::AlignCenter, secStr);
}

void MiniCountdown::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}
