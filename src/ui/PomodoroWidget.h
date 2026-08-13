#ifndef POMODOROWIDGET_H
#define POMODOROWIDGET_H

#include <DWidget>
#include <DPushButton>
#include <DLineEdit>
#include <QTimer>
#include <QList>

DWIDGET_USE_NAMESPACE

/**
 * @brief 番茄钟组件
 *
 * 圆形倒计时 + 任务标签 + 开始/暂停/重置按钮
 * 25分钟专注 → 5分钟休息循环
 * 每个专注 session 绑定一个任务标签，结束后记录到历史
 */
class PomodoroWidget : public DWidget
{
    Q_OBJECT
public:
    explicit PomodoroWidget(QWidget *parent = nullptr);

    /// 获取剩余秒数（供迷你浮窗使用）
    int remaining() const { return m_remaining; }
    /// 是否正在运行
    bool isRunning() const { return m_running; }
    /// 是否为休息模式
    bool isBreak() const { return m_isBreak; }

signals:
    /// 专注或休息结束时触发（isBreak=true 表示刚结束休息进入专注）
    void sessionComplete(bool isBreak);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onStartStop();
    void onReset();
    void onTick();
    void onTaskChanged(const QString &text);

private:
    void setupUI();

    int m_totalSeconds = 25 * 60;
    int m_remaining = 25 * 60;
    bool m_running = false;
    bool m_isBreak = false;
    int m_focusCount = 0;

    /// 当前专注 session 绑定的任务标签
    QString m_currentTask;

    /// session 开始时锁定的任务标签（专注期间不可改）
    QString m_lockedTask;

    /// 完成的专注记录
    struct SessionRecord {
        QString task;
        qint64  timestamp;
    };
    QList<SessionRecord> m_history;

    QTimer *m_timer = nullptr;
    DPushButton *m_btnStart = nullptr;
    DPushButton *m_btnReset = nullptr;
    DLineEdit   *m_taskEdit = nullptr;
};

#endif // POMODOROWIDGET_H
