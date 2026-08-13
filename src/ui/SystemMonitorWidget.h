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
 *  - CPU：圆形仪表 + 历史曲线 + 进程告警条
 *  - 内存：横向进度条 + 数值
 *  - 网络：双曲线图（上传/下载）+ 速率文本（支持 KB/s / kbps 切换）
 *  - 温度：温度计风格指示
 */
class SystemMonitorWidget : public DWidget
{
    Q_OBJECT
public:
    explicit SystemMonitorWidget(SystemMonitor *monitor, QWidget *parent = nullptr);

    /// 设置网速单位：true=KB/s, false=kbps
    void setUseByteUnit(bool useByte) { m_useByteUnit = useByte; }

    /// 设置流量预警阈值（MB，0=禁用）
    void setTrafficThresholdMB(int mb) { m_trafficThresholdMB = mb; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void onStatsUpdated();

private:
    SystemMonitor *m_monitor;
    bool m_useByteUnit = true;

    void drawCpuGauge(QPainter *painter, const QRect &rect);
    void drawMemBar(QPainter *painter, const QRect &rect);
    void drawNetGraph(QPainter *painter, const QRect &rect);
    void drawTemperature(QPainter *painter, const QRect &rect);
    void drawAlertBar(QPainter *painter, const QRect &rect);
    void drawTrafficAlert(QPainter *painter, const QRect &rect);
    void drawMemPressureBar(QPainter *painter, const QRect &rect);

    QString formatSize(qint64 bytes) const;
    QString formatSpeed(float kbps) const;

    int m_trafficThresholdMB = 0;
};

#endif // SYSTEMMONITORWIDGET_H
