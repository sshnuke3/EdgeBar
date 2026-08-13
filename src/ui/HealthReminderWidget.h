#ifndef HEALTHREMINDERWIDGET_H
#define HEALTHREMINDERWIDGET_H

#include <DWidget>
#include <DLabel>
#include <DPushButton>
#include <DToolButton>
#include <DSlider>
#include <DGroupBox>

#include <QTimer>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>

DWIDGET_USE_NAMESPACE

/**
 * @brief 健康提醒组件
 *
 * 集成喝水提醒和久坐提醒：
 * - 喝水提醒：可设置间隔（30/45/60分钟），到时弹窗提醒
 * - 久坐提醒：可设置间隔（45/60/90分钟），到时提醒起身活动
 * - 显示今日喝水次数、站立次数
 * - 自绘水杯图标和椅子图标，带动画进度条
 */
class HealthReminderWidget : public DWidget
{
    Q_OBJECT
public:
    explicit HealthReminderWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onTick();
    void onDrinkWater();
    void onStandUp();
    void onWaterIntervalChanged(int minutes);
    void onStandIntervalChanged(int minutes);

private:
    void setupUI();

    // 喝水提醒
    int m_waterIntervalMin = 45;       // 喝水间隔（分钟）
    QDateTime m_lastDrinkTime;         // 上次喝水时间
    int m_waterCountToday = 0;         // 今日喝水次数
    QDateTime m_waterDayStart;         // 今日起始时间（用于重置计数）

    // 久坐提醒
    int m_standIntervalMin = 60;       // 久坐提醒间隔（分钟）
    QDateTime m_lastStandTime;         // 上次站立时间
    int m_standCountToday = 0;         // 今日站立次数

    QTimer *m_tickTimer = nullptr;

    // 控件
    DPushButton *m_drinkBtn = nullptr;
    DPushButton *m_standBtn = nullptr;
    DLabel *m_waterLabel = nullptr;
    DLabel *m_standLabel = nullptr;
    DLabel *m_waterCountLabel = nullptr;
    DLabel *m_standCountLabel = nullptr;

    // 动画进度（0-1）
    float m_waterProgress = 0;
    float m_standProgress = 0;

    void resetDailyIfNeeded();
    QString formatCountdown(qint64 seconds) const;
};

#endif // HEALTHREMINDERWIDGET_H
