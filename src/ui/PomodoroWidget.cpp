#include "PomodoroWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFont>
#include <cmath>

PomodoroWidget::PomodoroWidget(QWidget *parent)
    : DWidget(parent)
{
    setupUI();

    m_timer = new QTimer(this);
    m_timer->setInterval(1000);
    connect(m_timer, &QTimer::timeout, this, &PomodoroWidget::onTick);
}

void PomodoroWidget::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(6, 6, 6, 6);

    // 标签
    auto *label = new DWidget(this);
    label->setFixedHeight(20);
    auto *labelLayout = new QHBoxLayout(label);
    labelLayout->setContentsMargins(0, 0, 0, 0);
    auto *titleLabel = new DPushButton(label);
    titleLabel->setText(m_isBreak ? QStringLiteral("休息时间") : QStringLiteral("专注模式"));
    titleLabel->setFlat(true);
    titleLabel->setEnabled(false);
    titleLabel->setStyleSheet("border: none; font-size: 11px;");
    labelLayout->addWidget(titleLabel);
    labelLayout->addStretch();
    layout->addWidget(label);

    // 中央留空给自绘
    layout->addStretch(1);

    // 按钮行
    auto *btnLayout = new QHBoxLayout;
    btnLayout->setSpacing(8);

    m_btnStart = new DPushButton(this);
    m_btnStart->setText(QStringLiteral("开始"));
    m_btnStart->setFixedHeight(30);

    m_btnReset = new DPushButton(this);
    m_btnReset->setText(QStringLiteral("重置"));
    m_btnReset->setFixedHeight(30);

    btnLayout->addStretch();
    btnLayout->addWidget(m_btnStart);
    btnLayout->addWidget(m_btnReset);
    btnLayout->addStretch();

    layout->addLayout(btnLayout);
    layout->addStretch(1);

    connect(m_btnStart, &DPushButton::clicked,
            this, &PomodoroWidget::onStartStop);
    connect(m_btnReset, &DPushButton::clicked,
            this, &PomodoroWidget::onReset);
}

void PomodoroWidget::onStartStop()
{
    if (m_running) {
        m_timer->stop();
        m_running = false;
        m_btnStart->setText(QStringLiteral("继续"));
    } else {
        m_timer->start();
        m_running = true;
        m_btnStart->setText(QStringLiteral("暂停"));
    }
    update();
}

void PomodoroWidget::onReset()
{
    m_timer->stop();
    m_running = false;
    m_isBreak = false;
    m_remaining = m_totalSeconds = 25 * 60;
    m_btnStart->setText(QStringLiteral("开始"));
    update();
}

void PomodoroWidget::onTick()
{
    if (m_remaining > 0) {
        m_remaining--;
    } else {
        // 切换状态
        if (m_isBreak) {
            m_isBreak = false;
            m_totalSeconds = 25 * 60;
            m_remaining = m_totalSeconds;
            m_focusCount++;
        } else {
            m_isBreak = true;
            m_totalSeconds = (m_focusCount > 0 && m_focusCount % 4 == 3)
                           ? 15 * 60 : 5 * 60;
            m_remaining = m_totalSeconds;
        }
    }
    update();
}

void PomodoroWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    int size = qMin(width() - 40, 160);
    int cx = width() / 2;
    int cy = height() / 2 - 20;
    int radius = size / 2;

    // 背景圆环
    painter.setPen(QPen(QColor(0, 0, 0, 20), 6, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(cx - radius, cy - radius, size, size, 0, 360 * 16);

    // 进度弧线
    float progress = 1.0f - static_cast<float>(m_remaining) / m_totalSeconds;
    int angle = static_cast<int>(progress * 360);

    QColor arcColor = m_isBreak ? QColor(46, 204, 113) : QColor(231, 76, 60);
    painter.setPen(QPen(arcColor, 6, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(cx - radius, cy - radius, size, size, 90 * 16, -angle * 16);

    // 时间文字
    painter.setPen(palette().color(QPalette::Text));
    QFont timeFont = font();
    timeFont.setPointSizeF(timeFont.pointSizeF() * 2.0);
    timeFont.setBold(true);
    painter.setFont(timeFont);

    int mins = m_remaining / 60;
    int secs = m_remaining % 60;
    QString timeStr = QString::asprintf("%02d:%02d", mins, secs);
    painter.drawText(QRect(cx - radius, cy - radius, size, size),
                    Qt::AlignCenter, timeStr);

    // 状态文字
    QFont statusFont = font();
    statusFont.setPointSizeF(statusFont.pointSizeF() * 0.75);
    painter.setFont(statusFont);
    painter.setPen(palette().color(QPalette::Text));

    QString status;
    if (m_running) {
        status = m_isBreak ? QStringLiteral("休息中…") : QStringLiteral("专注中…");
    } else if (m_remaining == m_totalSeconds) {
        status = QStringLiteral("准备开始");
    } else {
        status = QStringLiteral("已暂停");
    }
    painter.drawText(QRect(cx - radius, cy + radius - 4, size, 20),
                    Qt::AlignCenter, status);

    // 番茄计数
    QString countStr = QStringLiteral("已完成 %1 个番茄").arg(m_focusCount);
    QFont countFont = font();
    countFont.setPointSizeF(countFont.pointSizeF() * 0.7);
    painter.setFont(countFont);
    QColor subColor = palette().color(QPalette::Text);
    subColor.setAlpha(120);
    painter.setPen(subColor);
    painter.drawText(QRect(0, height() - 30, width(), 20),
                    Qt::AlignCenter, countStr);
}
