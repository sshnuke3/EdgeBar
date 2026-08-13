#ifndef DESKTOPWIDGET_H
#define DESKTOPWIDGET_H

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QPoint>
#include <QTimer>

class SystemMonitor;
class PomodoroWidget;
class HealthReminderWidget;

/**
 * @brief 桌面迷你小组件
 *
 * 从主面板分离出的独立浮窗，驻留桌面。
 * 支持三种模式：
 *   - CPU仪表：圆形仪表 + 数值
 *   - 番茄钟：倒计时数字 + 进度环
 *   - 喝水进度：水杯图标 + 倒计时
 *
 * 特性：
 *   - 无边框 + 置顶 + 毛玻璃背景
 *   - 可拖拽移动位置
 *   - 双击切换模式
 *   - 右键菜单：切换模式 / 返回面板 / 关闭
 *   - 鼠标进入时展开显示更多信息
 */
class DesktopWidget : public QWidget
{
    Q_OBJECT
public:
    enum WidgetMode {
        CpuGaugeMode = 0,
        PomodoroMode = 1,
        WaterMode    = 2
    };

    explicit DesktopWidget(SystemMonitor *monitor,
                           PomodoroWidget *pomodoro,
                           HealthReminderWidget *health,
                           QWidget *parent = nullptr);

    void setMode(WidgetMode mode);
    WidgetMode mode() const { return m_mode; }

    /// 展开模式（鼠标进入时显示更大）
    void setExpanded(bool expanded);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

signals:
    void clicked();
    void requestReturnToPanel();

private slots:
    void onTick();

private:
    void drawCpuGauge(QPainter *painter, const QRect &rect);
    void drawPomodoro(QPainter *painter, const QRect &rect);
    void drawWater(QPainter *painter, const QRect &rect);

    SystemMonitor *m_monitor;
    PomodoroWidget *m_pomodoro;
    HealthReminderWidget *m_health;

    WidgetMode m_mode = CpuGaugeMode;
    bool m_expanded = false;
    bool m_dragging = false;
    QPoint m_dragOffset;

    QTimer *m_updateTimer = nullptr;

    // 尺寸常量
    static constexpr int COMPACT_SIZE = 80;
    static constexpr int EXPANDED_SIZE = 120;
};

#endif // DESKTOPWIDGET_H
