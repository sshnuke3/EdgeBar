#include "HealthReminderWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QApplication>
#include <QPainterPath>
#include <QDebug>
#include <cmath>

// ---------------------------------------------------------------------------
// HealthReminderWidget
// ---------------------------------------------------------------------------

HealthReminderWidget::HealthReminderWidget(QWidget *parent)
    : DWidget(parent)
{
    m_lastDrinkTime = QDateTime::currentDateTime();
    m_lastStandTime = QDateTime::currentDateTime();
    m_waterDayStart = QDateTime(QDateTime::currentDateTime().date(),
                                QTime(0, 0));

    setupUI();

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &HealthReminderWidget::onTick);
    m_tickTimer->start();
}

void HealthReminderWidget::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);
    layout->setContentsMargins(10, 10, 10, 10);

    // ---- 喝水提醒区 ----
    auto *waterArea = new DWidget(this);
    waterArea->setFixedHeight(130);
    auto *waterLayout = new QVBoxLayout(waterArea);
    waterLayout->setSpacing(4);
    waterLayout->setContentsMargins(0, 0, 0, 0);

    m_waterLabel = new DLabel(waterArea);
    m_waterLabel->setAlignment(Qt::AlignCenter);
    QFont waterFont = font();
    waterFont.setBold(true);
    waterFont.setPointSizeF(waterFont.pointSizeF() * 1.1);
    m_waterLabel->setFont(waterFont);

    m_waterCountLabel = new DLabel(waterArea);
    m_waterCountLabel->setAlignment(Qt::AlignCenter);
    QFont countFont = font();
    countFont.setPointSizeF(countFont.pointSizeF() * 0.8);
    m_waterCountLabel->setFont(countFont);
    m_waterCountLabel->setStyleSheet("color: gray;");

    m_drinkBtn = new DPushButton(waterArea);
    m_drinkBtn->setText(QStringLiteral("已喝水 ✓"));
    m_drinkBtn->setFixedHeight(32);

    waterLayout->addStretch();
    waterLayout->addWidget(m_waterLabel);
    waterLayout->addWidget(m_waterCountLabel);
    waterLayout->addSpacing(4);
    waterLayout->addWidget(m_drinkBtn);
    waterLayout->addStretch();

    // ---- 间隔设置区 ----
    auto *waterIntervalLabel = new DLabel(QStringLiteral("喝水间隔"), waterArea);
    waterIntervalLabel->setStyleSheet("color: gray; font-size: 10px;");

    auto *waterIntervalLayout = new QHBoxLayout;
    waterIntervalLayout->setSpacing(4);
    auto *w30 = new DPushButton(QStringLiteral("30分"), waterArea);
    auto *w45 = new DPushButton(QStringLiteral("45分"));
    auto *w60 = new DPushButton(QStringLiteral("60分"));
    w30->setCheckable(true);
    w45->setCheckable(true);
    w60->setCheckable(true);
    w45->setChecked(true);
    w30->setFixedHeight(24);
    w45->setFixedHeight(24);
    w60->setFixedHeight(24);
    w30->setStyleSheet("font-size: 10px;");
    w45->setStyleSheet("font-size: 10px;");
    w60->setStyleSheet("font-size: 10px;");

    waterIntervalLayout->addWidget(waterIntervalLabel);
    waterIntervalLayout->addWidget(w30);
    waterIntervalLayout->addWidget(w45);
    waterIntervalLayout->addWidget(w60);
    waterLayout->addLayout(waterIntervalLayout);

    layout->addWidget(waterArea);

    // ---- 分隔线 ----
    auto *separator = new DWidget(this);
    separator->setFixedHeight(1);
    separator->setStyleSheet("background: rgba(128,128,128,0.15);");
    layout->addWidget(separator);

    // ---- 久坐提醒区 ----
    auto *standArea = new DWidget(this);
    standArea->setFixedHeight(130);
    auto *standLayout = new QVBoxLayout(standArea);
    standLayout->setSpacing(4);
    standLayout->setContentsMargins(0, 0, 0, 0);

    m_standLabel = new DLabel(standArea);
    m_standLabel->setAlignment(Qt::AlignCenter);
    m_standLabel->setFont(waterFont);

    m_standCountLabel = new DLabel(standArea);
    m_standCountLabel->setAlignment(Qt::AlignCenter);
    m_standCountLabel->setFont(countFont);
    m_standCountLabel->setStyleSheet("color: gray;");

    m_standBtn = new DPushButton(standArea);
    m_standBtn->setText(QStringLiteral("已起身 ✓"));
    m_standBtn->setFixedHeight(32);

    standLayout->addStretch();
    standLayout->addWidget(m_standLabel);
    standLayout->addWidget(m_standCountLabel);
    standLayout->addSpacing(4);
    standLayout->addWidget(m_standBtn);
    standLayout->addStretch();

    // 间隔设置
    auto *standIntervalLabel = new DLabel(QStringLiteral("久坐提醒"), standArea);
    standIntervalLabel->setStyleSheet("color: gray; font-size: 10px;");

    auto *standIntervalLayout = new QHBoxLayout;
    standIntervalLayout->setSpacing(4);
    auto *s45 = new DPushButton(QStringLiteral("45分"), standArea);
    auto *s60 = new DPushButton(QStringLiteral("60分"));
    auto *s90 = new DPushButton(QStringLiteral("90分"));
    s45->setCheckable(true);
    s60->setCheckable(true);
    s90->setCheckable(true);
    s60->setChecked(true);
    s45->setFixedHeight(24);
    s60->setFixedHeight(24);
    s90->setFixedHeight(24);
    s45->setStyleSheet("font-size: 10px;");
    s60->setStyleSheet("font-size: 10px;");
    s90->setStyleSheet("font-size: 10px;");

    standIntervalLayout->addWidget(standIntervalLabel);
    standIntervalLayout->addWidget(s45);
    standIntervalLayout->addWidget(s60);
    standIntervalLayout->addWidget(s90);
    standLayout->addLayout(standIntervalLayout);

    layout->addWidget(standArea);
    layout->addStretch();

    // 连接按钮
    connect(m_drinkBtn, &DPushButton::clicked, this, &HealthReminderWidget::onDrinkWater);
    connect(m_standBtn, &DPushButton::clicked, this, &HealthReminderWidget::onStandUp);

    connect(w30, &DPushButton::clicked, this, [this, w30, w45, w60]() {
        w45->setChecked(false); w60->setChecked(false);
        onWaterIntervalChanged(30);
    });
    connect(w45, &DPushButton::clicked, this, [this, w30, w45, w60]() {
        w30->setChecked(false); w60->setChecked(false);
        onWaterIntervalChanged(45);
    });
    connect(w60, &DPushButton::clicked, this, [this, w30, w45, w60]() {
        w30->setChecked(false); w45->setChecked(false);
        onWaterIntervalChanged(60);
    });

    connect(s45, &DPushButton::clicked, this, [this, s45, s60, s90]() {
        s60->setChecked(false); s90->setChecked(false);
        onStandIntervalChanged(45);
    });
    connect(s60, &DPushButton::clicked, this, [this, s45, s60, s90]() {
        s45->setChecked(false); s90->setChecked(false);
        onStandIntervalChanged(60);
    });
    connect(s90, &DPushButton::clicked, this, [this, s45, s60, s90]() {
        s45->setChecked(false); s60->setChecked(false);
        onStandIntervalChanged(90);
    });
}

void HealthReminderWidget::onTick()
{
    resetDailyIfNeeded();
    update();
}

void HealthReminderWidget::resetDailyIfNeeded()
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime todayStart = QDateTime(now.date(), QTime(0, 0));

    if (m_waterDayStart < todayStart) {
        m_waterDayStart = todayStart;
        m_waterCountToday = 0;
        m_standCountToday = 0;
    }
}

void HealthReminderWidget::onDrinkWater()
{
    m_lastDrinkTime = QDateTime::currentDateTime();
    m_waterCountToday++;
    update();
}

void HealthReminderWidget::onStandUp()
{
    m_lastStandTime = QDateTime::currentDateTime();
    m_standCountToday++;
    update();
}

void HealthReminderWidget::onWaterIntervalChanged(int minutes)
{
    m_waterIntervalMin = minutes;
    update();
}

void HealthReminderWidget::onStandIntervalChanged(int minutes)
{
    m_standIntervalMin = minutes;
    update();
}

QString HealthReminderWidget::formatCountdown(qint64 seconds) const
{
    int mins = seconds / 60;
    int secs = seconds % 60;
    if (mins > 0)
        return QString::number(mins) + ":" + QString::number(secs).rightJustified(2, '0');
    return QString::number(secs) + "s";
}

// ---------------------------------------------------------------------------
// paintEvent: 自绘水杯和椅子进度图
// ---------------------------------------------------------------------------

void HealthReminderWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect r = rect().adjusted(8, 8, -8, -8);

    // ---- 喝水区 ----
    int waterH = 120;
    QRect waterRect(r.left(), r.top(), r.width(), waterH);

    QDateTime now = QDateTime::currentDateTime();
    qint64 waterElapsed = m_lastDrinkTime.secsTo(now);
    qint64 waterTotal = m_waterIntervalMin * 60;
    m_waterProgress = qBound(0.0f, static_cast<float>(waterElapsed) / waterTotal, 1.0f);

    qint64 waterRemaining = qMax(qint64(0), waterTotal - waterElapsed);

    // 绘制水杯
    int cupX = waterRect.left() + 20;
    int cupY = waterRect.top() + 15;
    int cupW = 40;
    int cupH = 60;

    QPainterPath cupPath;
    // 杯子形状：梯形（上宽下窄）
    cupPath.moveTo(cupX, cupY);
    cupPath.lineTo(cupX + cupW, cupY);
    cupPath.lineTo(cupX + cupW - 4, cupY + cupH);
    cupPath.lineTo(cupX + 4, cupY + cupH);
    cupPath.closeSubpath();

    // 杯子背景
    painter.setPen(QPen(QColor(100, 180, 255, 100), 2));
    painter.setBrush(QColor(100, 180, 255, 20));
    painter.drawPath(cupPath);

    // 水位填充
    if (m_waterProgress < 1.0f) {
        // 水位 = (1 - progress) * 杯高
        float waterLevel = 1.0f - m_waterProgress;
        int fillH = static_cast<int>((cupH - 8) * waterLevel);
        int fillY = cupY + cupH - 4 - fillH;

        painter.save();
        painter.setClipPath(cupPath);
        QLinearGradient waterGrad(0, fillY, 0, fillY + fillH);
        waterGrad.setColorAt(0, QColor(52, 152, 219, 180));
        waterGrad.setColorAt(1, QColor(41, 128, 185, 220));
        painter.setBrush(waterGrad);
        painter.setPen(Qt::NoPen);
        painter.drawRect(cupX + 2, fillY, cupW - 4, fillH + 4);
        painter.restore();
    } else {
        // 水喝完了，杯子空了，闪烁红色提醒
        QColor alertColor(231, 76, 60, 150 + static_cast<int>(50 * sin(now.toMSecsSinceEpoch() / 500.0)));
        painter.setPen(QPen(alertColor, 2));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(cupPath);
    }

    // 水杯标签
    if (m_waterProgress >= 1.0f) {
        m_waterLabel->setText(QStringLiteral("该喝水了！"));
        m_waterLabel->setStyleSheet("color: #e74c3c;");
    } else {
        m_waterLabel->setText(QStringLiteral("下次喝水: ") + formatCountdown(waterRemaining));
        m_waterLabel->setStyleSheet("");
    }
    m_waterCountLabel->setText(QStringLiteral("今日已喝 %1 次").arg(m_waterCountToday));

    // ---- 久坐区 ----
    int standY = waterRect.bottom() + 20;
    QRect standRect(r.left(), standY, r.width(), 120);

    qint64 standElapsed = m_lastStandTime.secsTo(now);
    qint64 standTotal = m_standIntervalMin * 60;
    m_standProgress = qBound(0.0f, static_cast<float>(standElapsed) / standTotal, 1.0f);

    qint64 standRemaining = qMax(qint64(0), standTotal - standElapsed);

    // 绘制椅子+进度环
    int chairX = standRect.left() + 20;
    int chairY = standRect.top() + 15;
    int chairSize = 50;

    // 椅子背景圆
    painter.setPen(QPen(QColor(128, 128, 128, 80), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(QPointF(chairX + chairSize/2, chairY + chairSize/2), chairSize/2, chairSize/2);

    // 进度弧
    int startAngle = 90 * 16;
    int spanAngle = static_cast<int>(-m_standProgress * 360 * 16);

    QColor progColor;
    if (m_standProgress >= 1.0f) {
        progColor = QColor(231, 76, 60, 150 + static_cast<int>(50 * sin(now.toMSecsSinceEpoch() / 500.0)));
    } else if (m_standProgress > 0.8f) {
        progColor = QColor(245, 167, 38);
    } else {
        progColor = QColor(72, 174, 79);
    }

    painter.setPen(QPen(progColor, 4, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(chairX, chairY, chairSize, chairSize, startAngle, spanAngle);

    // 椅子图标（简单线条）
    int cx = chairX + chairSize / 2;
    int cy = chairY + chairSize / 2;
    painter.setPen(QPen(QColor(128, 128, 128, 150), 2));
    // 椅子座
    painter.drawLine(cx - 8, cy - 2, cx + 8, cy - 2);
    // 椅子腿
    painter.drawLine(cx - 8, cy - 2, cx - 8, cy + 8);
    painter.drawLine(cx + 8, cy - 2, cx + 8, cy + 8);
    // 椅子靠背
    painter.drawLine(cx - 8, cy - 2, cx - 8, cy - 10);

    // 久坐标签
    if (m_standProgress >= 1.0f) {
        m_standLabel->setText(QStringLiteral("该起身活动了！"));
        m_standLabel->setStyleSheet("color: #e74c3c;");
    } else {
        m_standLabel->setText(QStringLiteral("下次起身: ") + formatCountdown(standRemaining));
        m_standLabel->setStyleSheet("");
    }
    m_standCountLabel->setText(QStringLiteral("今日已起身 %1 次").arg(m_standCountToday));
}
