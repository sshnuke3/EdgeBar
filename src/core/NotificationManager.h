#ifndef EDGE_NOTIFICATION_MANAGER_H
#define EDGE_NOTIFICATION_MANAGER_H

#include <QObject>

class SystemMonitor;
class HealthReminderWidget;
class PomodoroWidget;

/**
 * @brief 桌面通知管理器
 *
 * 使用 DNotifySender (dtkcore) 发送系统桌面通知。
 * 监听 SystemMonitor / HealthReminderWidget / PomodoroWidget 的信号，
 * 在 CPU 持续高占用、内存压力、喝水提醒、番茄钟完成、流量超额时发送通知。
 *
 * 依赖 DTK API:
 *   - DNotifySender (dtkcore) — 链式构建器发送 freedesktop Notifications
 *   - DDesktopServices (dtkgui/dtkwidget) — 播放系统音效
 */
class NotificationManager : public QObject
{
    Q_OBJECT
public:
    explicit NotificationManager(QObject *parent = nullptr);

    void setEnabled(bool enabled) { m_enabled = enabled; }
    bool enabled() const { return m_enabled; }

    void setSoundEnabled(bool enabled) { m_soundEnabled = enabled; }
    bool soundEnabled() const { return m_soundEnabled; }

    /// 绑定系统监控信号
    void connectSystemMonitor(SystemMonitor *monitor);

    /// 绑定健康提醒信号
    void connectHealthReminder(HealthReminderWidget *health);

    /// 绑定番茄钟信号
    void connectPomodoro(PomodoroWidget *pomodoro);

public slots:
    /// 发送通知（标题 + 正文 + 图标名）
    void notify(const QString &summary, const QString &body,
                const QString &iconName = QStringLiteral("dialog-information"));

private slots:
    void onCpuAlert(const QString &processName, double cpuPercent, int sustainedSeconds);
    void onMemPressureAlert(int level, float avg10);
    void onTrafficAlert(qint64 totalMB);
    void onWaterReminder();
    void onStandReminder();
    void onPomodoroComplete(bool isBreak);

private:
    void playNotificationSound();

    bool m_enabled = true;
    bool m_soundEnabled = true;
};

#endif // EDGE_NOTIFICATION_MANAGER_H
