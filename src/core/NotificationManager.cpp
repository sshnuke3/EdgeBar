#include "NotificationManager.h"
#include "SystemMonitor.h"
#include "HealthReminderWidget.h"
#include "PomodoroWidget.h"
#include "Logging.h"

#include <DNotifySender>

#include <QDBusPendingCall>

#if __has_include(<DDesktopServices>)
#include <DDesktopServices>
#define HAS_DESKTOP_SERVICES 1
#else
#define HAS_DESKTOP_SERVICES 0
#endif

using namespace Dtk::Core::DUtil;

// ---------------------------------------------------------------------------
// NotificationManager
// ---------------------------------------------------------------------------

NotificationManager::NotificationManager(QObject *parent)
    : QObject(parent)
{
}

void NotificationManager::connectSystemMonitor(SystemMonitor *monitor)
{
    if (!monitor) return;

    connect(monitor, &SystemMonitor::cpuAlert,
            this, &NotificationManager::onCpuAlert);

    connect(monitor, &SystemMonitor::memPressureAlert,
            this, &NotificationManager::onMemPressureAlert);

    connect(monitor, &SystemMonitor::trafficAlert,
            this, &NotificationManager::onTrafficAlert);
}

void NotificationManager::connectHealthReminder(HealthReminderWidget *health)
{
    if (!health) return;

    connect(health, &HealthReminderWidget::waterReminder,
            this, &NotificationManager::onWaterReminder);

    connect(health, &HealthReminderWidget::standReminder,
            this, &NotificationManager::onStandReminder);
}

void NotificationManager::connectPomodoro(PomodoroWidget *pomodoro)
{
    if (!pomodoro) return;

    connect(pomodoro, &PomodoroWidget::sessionComplete,
            this, &NotificationManager::onPomodoroComplete);
}

// ---------------------------------------------------------------------------
// notify: 发送桌面通知
// ---------------------------------------------------------------------------

void NotificationManager::notify(const QString &summary, const QString &body,
                                  const QString &iconName)
{
    if (!m_enabled) return;

    qCInfo(edgebarLog) << "Notification:" << summary << "-" << body;

    DNotifySender(summary)
        .appName(QStringLiteral("EdgeBar"))
        .appIcon(iconName)
        .appBody(body)
        .timeOut(5000)
        .call();

    if (m_soundEnabled) {
        playNotificationSound();
    }
}

// ---------------------------------------------------------------------------
// 信号处理槽
// ---------------------------------------------------------------------------

void NotificationManager::onCpuAlert(const QString &processName,
                                     double cpuPercent, int sustainedSeconds)
{
    QString body = QStringLiteral("进程 %1 持续占用 CPU %2%，已 %3 秒")
                      .arg(processName)
                      .arg(cpuPercent, 0, 'f', 1)
                      .arg(sustainedSeconds);
    notify(QStringLiteral("CPU 高占用告警"), body,
           QStringLiteral("dialog-warning"));
}

void NotificationManager::onMemPressureAlert(int level, float avg10)
{
    QString levelStr;
    switch (level) {
    case 3: levelStr = QStringLiteral("严重"); break;
    case 2: levelStr = QStringLiteral("全部进程受阻"); break;
    case 1: levelStr = QStringLiteral("部分进程受阻"); break;
    default: levelStr = QStringLiteral("轻度"); break;
    }

    QString body = QStringLiteral("内存压力等级：%1（avg10: %2%）")
                      .arg(levelStr)
                      .arg(avg10, 0, 'f', 1);
    notify(QStringLiteral("内存压力告警"), body,
           QStringLiteral("dialog-warning"));
}

void NotificationManager::onTrafficAlert(qint64 totalMB)
{
    QString body = QStringLiteral("今日累计流量已达 %1 MB，超过预设阈值")
                      .arg(totalMB);
    notify(QStringLiteral("流量超额告警"), body,
           QStringLiteral("network-wired"));
}

void NotificationManager::onWaterReminder()
{
    notify(QStringLiteral("喝水提醒"),
           QStringLiteral("已经很久没喝水了，起来倒杯水吧！"),
           QStringLiteral("preferences-system-health"));
}

void NotificationManager::onStandReminder()
{
    notify(QStringLiteral("久坐提醒"),
           QStringLiteral("坐得太久了，站起来活动一下！"),
           QStringLiteral("preferences-system-health"));
}

void NotificationManager::onPomodoroComplete(bool isBreak)
{
    if (isBreak) {
        // 休息结束 → 专注开始
        notify(QStringLiteral("休息结束"),
               QStringLiteral("休息时间到，开始新一轮专注吧！"),
               QStringLiteral("clock"));
    } else {
        // 专注结束 → 休息开始
        notify(QStringLiteral("专注完成"),
               QStringLiteral("干得漂亮！该休息一下了。"),
               QStringLiteral("clock"));
    }
}

// ---------------------------------------------------------------------------
// playNotificationSound: 播放系统通知音效
// ---------------------------------------------------------------------------

void NotificationManager::playNotificationSound()
{
#if HAS_DESKTOP_SERVICES
    DDesktopServices::playSystemSoundEffect(DDesktopServices::SSE_Notifications);
#endif
}
