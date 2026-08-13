#include "PomodoroWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFont>
#include <QDateTime>
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
    layout->setSpacing(6);
    layout->setContentsMargins(6, 6, 6, 6);

    // ---- 标题行 ----
    auto *titleWidget = new DWidget(this);
    titleWidget->setFixedHeight(20);
    auto *titleLayout = new QHBoxLayout(titleWidget);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    auto *titleLabel = new DPushButton(titleWidget);
    titleLabel->setText(QStringLiteral("专注模式"));
    titleLabel->setFlat(true);
    titleLabel->setEnabled(false);
    titleLabel->setStyleSheet("border: none; font-size: 11px;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    layout->addWidget(titleWidget);

    // ---- 任务输入框 ----
    m_taskEdit = new DLineEdit(this);
    m_taskEdit->setPlaceholderText(QStringLiteral("当前任务（可选）…"));
    m_taskEdit->setFixedHeight(30);
    m_taskEdit->setClearButtonEnabled(true);
    layout->addWidget(m_taskEdit);

    // 中央留空给自绘
    layout->addStretch(1);

    // ---- 按钮行 ----
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
    connect(m_taskEdit, &DLineEdit::textChanged,
            this, &PomodoroWidget::onTaskChanged);
}

void PomodoroWidget::onTaskChanged(const QString &text)
{
    // 只在未运行或休息时允许修改任务
    if (!m_running || m_isBreak) {
        m_currentTask = text.trimmed();
    }
}

// ---------------------------------------------------------------------------
// onStartStop: 开始/暂停按钮
// ---------------------------------------------------------------------------
//
// 开始专注时：
//   - 锁定当前任务标签（专注期间不可改）
//   - 禁用任务输入框
//
// 暂停时：
//   - 解锁任务输入框
//
// ---------------------------------------------------------------------------

void PomodoroWidget::onStartStop()
{
    if (m_running) {
        m_timer->stop();
        m_running = false;
        m_btnStart->setText(QStringLiteral("继续"));
        m_taskEdit->setEnabled(true);
    } else {
        // 开始专注时锁定任务标签
        if (!m_isBreak) {
            m_lockedTask = m_currentTask;
            m_taskEdit->setEnabled(false);
        }
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
    m_lockedTask.clear();
    m_btnStart->setText(QStringLiteral("开始"));
    m_taskEdit->setEnabled(true);
    update();
}

// ---------------------------------------------------------------------------
// onTick: 每秒倒计时
// ---------------------------------------------------------------------------
//
// 专注结束 → 休息开始：
//   - 记录完成的 session（任务标签 + 时间戳）
//   - 切换到休息模式
//   - 解锁任务输入框
//
// 休息结束 → 专注开始：
//   - 切换回专注模式
//   - 番茄计数 +1
//
// ---------------------------------------------------------------------------

void PomodoroWidget::onTick()
{
    if (m_remaining > 0) {
        m_remaining--;
    } else {
        if (m_isBreak) {
            // 休息结束 → 进入专注
            m_isBreak = false;
            m_totalSeconds = 25 * 60;
            m_remaining = m_totalSeconds;
            m_focusCount++;
            m_lockedTask.clear();
            m_taskEdit->setEnabled(true);
            emit sessionComplete(true);
        } else {
            // 专注结束 → 记录 session
            m_history.append({m_lockedTask, QDateTime::currentSecsSinceEpoch()});
            if (m_history.size() > 20) {
                m_history.removeFirst();
            }

            // 切换到休息
            m_isBreak = true;
            m_totalSeconds = (m_focusCount > 0 && m_focusCount % 4 == 3)
                           ? 15 * 60 : 5 * 60;
            m_remaining = m_totalSeconds;
            m_taskEdit->setEnabled(true);
            emit sessionComplete(false);
        }
    }
    update();
}

// ---------------------------------------------------------------------------
// paintEvent: 自绘番茄钟界面
// ---------------------------------------------------------------------------
//
// 布局：
//   - 圆形进度环 + 倒计时文字 + 状态文字
//   - 当前任务标签（圆环下方）
//   - 番茄计数（底部）
//   - 最近完成的任务历史（最底部，最多 3 条）
//
// ---------------------------------------------------------------------------

void PomodoroWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    int size = qMin(width() - 40, 140);
    int cx = width() / 2;
    int cy = height() / 2 - 40;
    int radius = size / 2;

    // ---- 背景圆环 ----
    painter.setPen(QPen(QColor(0, 0, 0, 20), 6, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(cx - radius, cy - radius, size, size, 0, 360 * 16);

    // ---- 进度弧线 ----
    float progress = 1.0f - static_cast<float>(m_remaining) / m_totalSeconds;
    int angle = static_cast<int>(progress * 360);

    QColor arcColor = m_isBreak ? QColor(46, 204, 113) : QColor(231, 76, 60);
    painter.setPen(QPen(arcColor, 6, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(cx - radius, cy - radius, size, size, 90 * 16, -angle * 16);

    // ---- 时间文字 ----
    painter.setPen(palette().color(QPalette::Text));
    QFont timeFont = font();
    timeFont.setPointSizeF(timeFont.pointSizeF() * 1.8);
    timeFont.setBold(true);
    painter.setFont(timeFont);

    int mins = m_remaining / 60;
    int secs = m_remaining % 60;
    QString timeStr = QString::asprintf("%02d:%02d", mins, secs);
    painter.drawText(QRect(cx - radius, cy - radius, size, size),
                    Qt::AlignCenter, timeStr);

    // ---- 状态文字 ----
    QFont statusFont = font();
    statusFont.setPointSizeF(statusFont.pointSizeF() * 0.7);
    painter.setFont(statusFont);

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

    // ---- 当前任务标签 ----
    QString displayTask = m_isBreak ? m_lockedTask : m_currentTask;
    if (displayTask.isEmpty()) {
        displayTask = m_isBreak ? QStringLiteral("（休息时间）") : QStringLiteral("（未设置任务）");
    }

    QFont taskFont = font();
    taskFont.setPointSizeF(taskFont.pointSizeF() * 0.75);
    painter.setFont(taskFont);
    painter.setPen(palette().color(QPalette::Text));

    // 任务标签带背景框
    QFontMetrics fm(taskFont);
    int taskW = fm.horizontalAdvance(displayTask) + 16;
    taskW = qMin(taskW, width() - 16);
    QRect taskRect(cx - taskW / 2, cy + radius + 10, taskW, 20);

    painter.setPen(Qt::NoPen);
    QColor taskBg = m_isBreak ? QColor(46, 204, 113, 40) : QColor(231, 76, 60, 40);
    painter.setBrush(taskBg);
    painter.drawRoundedRect(taskRect, 10, 10);

    painter.setPen(palette().color(QPalette::Text));
    painter.drawText(taskRect, Qt::AlignCenter, displayTask);

    // ---- 番茄计数 ----
    QString countStr = QStringLiteral("已完成 %1 个番茄").arg(m_focusCount);
    QFont countFont = font();
    countFont.setPointSizeF(countFont.pointSizeF() * 0.7);
    painter.setFont(countFont);
    QColor subColor = palette().color(QPalette::Text);
    subColor.setAlpha(120);
    painter.setPen(subColor);
    painter.drawText(QRect(0, height() - 60, width(), 16),
                    Qt::AlignCenter, countStr);

    // ---- 最近完成的任务历史（最多 3 条）----
    int historyY = height() - 44;
    int shown = qMin(m_history.size(), 3);
    for (int i = 0; i < shown; ++i) {
        const auto &rec = m_history[m_history.size() - 1 - i];
        QString taskText = rec.task.isEmpty() ? QStringLiteral("（未命名）") : rec.task;
        QString timeStr2 = QDateTime::fromSecsSinceEpoch(rec.timestamp)
                              .toString("HH:mm");
        QString line = QStringLiteral("%1  %2").arg(timeStr2).arg(taskText);

        // 截断
        QFontMetrics fm2(countFont);
        if (fm2.horizontalAdvance(line) > width() - 20) {
            line = fm2.elidedText(line, Qt::ElideRight, width() - 20);
        }

        painter.setPen(subColor);
        painter.drawText(QRect(4, historyY - i * 14, width() - 8, 14),
                        Qt::AlignCenter, line);
    }
}
