#ifndef SYSTEMMONITORWIDGET_H
#define SYSTEMMONITORWIDGET_H

#include <DWidget>
#include <QLabel>

#include "core/SystemMonitor.h"

DWIDGET_USE_NAMESPACE

/**
 * @brief 系统监控可视化组件
 *
 * 自绘图表：
 *  - CPU：圆形仪表 + 历史曲线
 *  - 内存：横向进度条 + 数值
 *  - 网络：双曲线图（上传/下载）+ 速率文本
 *  - 温度：温度计风格指示
 */
class SystemMonitorWidget : public DWidget
{
    Q_OBJECT
public:
    explicit SystemMonitorWidget(SystemMonitor *monitor, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onStatsUpdated();

private:
    SystemMonitor *m_monitor;

    void drawCpuGauge(QPainter *painter, const QRect &rect);
    void drawMemBar(QPainter *painter, const QRect &rect);
    void drawNetGraph(QPainter *painter, const QRect &rect);
    void drawTemperature(QPainter *painter, const QRect &rect);
    void drawInfoText(QPainter *painter, const QRect &rect);

    QString formatSize(qint64 bytes) const;
    QString formatSpeed(float kbps) const;
};

#endif // SYSTEMMONITORWIDGET_H
