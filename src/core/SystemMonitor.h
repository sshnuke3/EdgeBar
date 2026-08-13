#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <QDate>

/**
 * @brief 系统资源监控
 *
 * 读取 /proc/stat, /proc/meminfo, /proc/net/dev, /sys/class/thermal
 * 提供 CPU、内存、网络、温度的实时数据。
 * 支持扫描高 CPU 占用进程。
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

    /// 高 CPU 占用进程信息
    struct ProcessInfo {
        int     pid;
        QString name;
        float   cpuPercent;  // 单核百分比（可超过 100*N核）
        int     sustainedSeconds = 0;  // 持续高占用秒数
    };

    /// 获取 CPU 占用最高的进程（无告警时为空）
    const ProcessInfo &topProcess() const { return m_topProcess; }

    /// 设置 CPU 告警阈值（0=禁用）
    void setCpuAlertThreshold(int threshold) { m_cpuAlertThreshold = threshold; }

    /// 设置 CPU 持续高占用告警秒数（默认 0=立即告警）
    void setCpuSustainedSeconds(int seconds) { m_cpuSustainedThreshold = seconds; }

    /// 内存压力等级
    enum MemPressureLevel {
        MemPressureNone = 0,    // 正常
        MemPressureSome  = 1,   // 部分进程被阻塞
        MemPressureFull  = 2,   // 全部进程被阻塞
        MemPressureCritical = 3 // 严重，建议关闭进程
    };

    /// 获取内存压力等级
    MemPressureLevel memPressureLevel() const { return m_memPressureLevel; }

    /// 获取内存压力 avg10 值（百分比，0-100）
    float memPressureAvg10() const { return m_memPressureAvg10; }

    /// 获取内存压力 total 值（微秒）
    qint64 memPressureTotal() const { return m_memPressureTotal; }

    /// 获取占用内存最高的进程列表
    struct MemProcessInfo {
        int     pid;
        QString name;
        qint64  rssBytes;    // 实际物理内存
        float   memPercent;  // 占总内存百分比
    };
    const QList<MemProcessInfo> &topMemProcesses() const { return m_topMemProcesses; }

    /// 设置内存压力告警阈值（百分比，0=禁用）
    void setMemPressureThreshold(int percent) { m_memPressureThreshold = percent; }

    /// 获取当日累计流量（bytes）
    qint64 dailyRxBytes() const { return m_dailyRxBytes; }
    qint64 dailyTxBytes() const { return m_dailyTxBytes; }

    /// 设置流量预警阈值（MB，0=禁用）
    void setTrafficThresholdMB(int mb) { m_trafficThresholdMB = mb; }

    /// 获取全部历史快照（用于 CSV 导出）
    struct Snapshot {
        qint64 timestamp;
        float  cpu;
        float  mem;
        float  netDown;
        float  netUp;
        float  temp;
    };
    const QVector<Snapshot> &snapshots() const { return m_snapshots; }

    /// 导出历史数据为 CSV
    bool exportCsv(const QString &filePath) const;

    /// 生成今日效率报告
    struct DailyReport {
        QDate date;
        float cpuAvg;        // CPU 平均使用率
        float cpuPeak;       // CPU 峰值
        float memAvg;        // 内存平均使用率
        float memPeak;       // 内存峰值
        float tempAvg;       // 平均温度
        float tempPeak;      // 最高温度
        qint64 netDownTotal; // 下载总量（bytes）
        qint64 netUpTotal;   // 上传总量（bytes）
        int snapshotCount;   // 采样数
        QString peakHour;    // CPU 峰值时段
    };
    DailyReport generateDailyReport() const;

signals:
    void statsUpdated();

private slots:
    void poll();

private:
    void readCpuStat();
    void readMemInfo();
    void readNetDev();
    void readTemperature();
    void readTopProcess();
    void readMemPressure();
    void readTopMemProcesses();

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

    // 进程监控
    int m_cpuAlertThreshold = 0;
    int m_cpuSustainedThreshold = 0;  // 持续 N 秒才告警
    ProcessInfo m_topProcess;
    // 持续追踪：记录上次 top process 的 pid 和累计秒数
    int m_lastTopPid = 0;
    int m_sustainedCount = 0;  // 连续高于阈值的 poll 次数
    int m_pollIntervalMs = 2000;  // poll 间隔，用于计算持续秒数
    // pid -> (utime+stime, timestamp) 用于计算增量
    QMap<int, QPair<qint64, qint64>> m_prevProcTimes;
    // 上次 poll 的总 jiffies（用于进程 CPU% 计算）
    qint64 m_lastTotalJiffies = 0;

    // 流量累计
    qint64 m_dailyRxBytes = 0;
    qint64 m_dailyTxBytes = 0;
    int    m_trafficThresholdMB = 0;
    QDate  m_trafficResetDate;

    // 历史快照（用于 CSV 导出，上限 360 条 = 12 分钟 @2s）
    QVector<Snapshot> m_snapshots;
    static constexpr int MAX_SNAPSHOTS = 360;

    // 内存压力（PSI）
    MemPressureLevel m_memPressureLevel = MemPressureNone;
    float   m_memPressureAvg10 = 0;
    qint64  m_memPressureTotal = 0;
    int     m_memPressureThreshold = 0;  // 百分比阈值
    QList<MemProcessInfo> m_topMemProcesses;

    static constexpr int HISTORY_SIZE = 60;
};

#endif // SYSTEMMONITOR_H
