#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QTimer>
#include <QVector>

/**
 * @brief 系统资源监控
 *
 * 读取 /proc/stat, /proc/meminfo, /proc/net/dev, /sys/class/thermal
 * 提供 CPU、内存、网络、温度的实时数据。
 */
class SystemMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(float cpuUsage READ cpuUsage NOTIFY statsUpdated)
    Q_PROPERTY(float memUsage READ memUsage NOTIFY statsUpdated)
    Q_PROPERTY(qint64 memTotal READ memTotal NOTIFY statsUpdated)
    Q_PROPERTY(qint64 memUsed READ memUsed NOTIFY statsUpdated)
    Q_PROPERTY(float netUpload READ netUpload NOTIFY statsUpdated)
    Q_PROPERTY(float netDownload READ netDownload NOTIFY statsUpdated)
    Q_PROPERTY(float temperature READ temperature NOTIFY statsUpdated)

public:
    explicit SystemMonitor(QObject *parent = nullptr);

    void start(int intervalMs = 2000);
    void stop();

    float   cpuUsage() const    { return m_cpuUsage; }
    float   memUsage() const    { return m_memUsage; }
    qint64  memTotal() const    { return m_memTotal; }
    qint64  memUsed() const     { return m_memUsed; }
    float   netUpload() const   { return m_netUpload; }
    float   netDownload() const { return m_netDownload; }
    float   temperature() const { return m_temperature; }

    /// CPU 历史数据（用于绘制曲线图）
    const QVector<float> &cpuHistory() const { return m_cpuHistory; }

    /// 网络下载历史数据
    const QVector<float> &netDownHistory() const { return m_netDownHistory; }

    /// 网络上传历史数据
    const QVector<float> &netUpHistory() const { return m_netUpHistory; }

signals:
    void statsUpdated();

private slots:
    void poll();

private:
    void readCpuStat();
    void readMemInfo();
    void readNetDev();
    void readTemperature();

    QTimer m_timer;

    // CPU
    qint64 m_prevIdle = 0;
    qint64 m_prevTotal = 0;
    float  m_cpuUsage = 0;
    QVector<float> m_cpuHistory;

    // 内存
    qint64 m_memTotal = 0;
    qint64 m_memUsed = 0;
    float  m_memUsage = 0;

    // 网络
    qint64 m_prevRxBytes = 0;
    qint64 m_prevTxBytes = 0;
    qint64 m_prevNetTime = 0;
    float  m_netUpload = 0;    // KB/s
    float  m_netDownload = 0;  // KB/s
    QVector<float> m_netDownHistory;
    QVector<float> m_netUpHistory;

    // 温度
    float m_temperature = 0;

    static constexpr int HISTORY_SIZE = 60;
};

#endif // SYSTEMMONITOR_H
