#ifndef POMODOROWIDGET_H
#define POMODOROWIDGET_H

#include <DWidget>
#include <DPushButton>
#include <QTimer>

DWIDGET_USE_NAMESPACE

/**
 * @brief 番茄钟组件
 *
 * 圆形倒计时 + 开始/暂停/重置按钮
 * 25分钟专注 → 5分钟休息循环
 */
class PomodoroWidget : public DWidget
{
    Q_OBJECT
public:
    explicit PomodoroWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onStartStop();
    void onReset();
    void onTick();

private:
    void setupUI();

    int m_totalSeconds = 25 * 60;
    int m_remaining = 25 * 60;
    bool m_running = false;
    bool m_isBreak = false;
    int m_focusCount = 0;

    QTimer *m_timer = nullptr;
    DPushButton *m_btnStart = nullptr;
    DPushButton *m_btnReset = nullptr;
};

#endif // POMODOROWIDGET_H
