#include "SystemMonitor.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QDebug>

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// start / stop: 定时器控制
// ---------------------------------------------------------------------------

void SystemMonitor::start(int intervalMs)
{
    connect(&m_timer, &QTimer::timeout, this, &SystemMonitor::poll);
    m_timer.start(intervalMs);
}

void SystemMonitor::stop()
{
    m_timer.stop();
}

// ---------------------------------------------------------------------------
// poll: 统一采集入口，依次读取各项指标后发出信号
// ---------------------------------------------------------------------------

void SystemMonitor::poll()
{
    readCpuStat();
    readMemInfo();
    readNetDev();
    readTemperature();
    emit statsUpdated();
}

// ---------------------------------------------------------------------------
// readCpuStat: 读取 /proc/stat 计算 CPU 使用率
// ---------------------------------------------------------------------------
//
// /proc/stat 第一行为 aggregate CPU 统计：
//   cpu  user nice system idle iowait irq softirq steal [guest guest_nice]
//
// 计算方式：
//   total     = user + nice + system + idle + iowait + irq + softirq + steal
//   idle_time = idle + iowait
//   使用率    = (total差 - idle差) * 100 / total差
//
// 首次采样仅记录基线，不计算使用率。
//
// ---------------------------------------------------------------------------

void SystemMonitor::readCpuStat()
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SystemMonitor: failed to open /proc/stat";
        return;
    }

    // 仅需第一行（aggregate CPU）
    QString line = file.readLine();
    file.close();

    const auto parts = line.simplified().split(' ');
    if (parts.size() < 2 || parts[0] != QStringLiteral("cpu")) {
        return;
    }

    // 解析 user/nice/system/idle/iowait/irq/softirq/steal（最多 8 个字段）
    QVector<qint64> fields;
    for (int i = 1; i < parts.size() && fields.size() < 8; ++i) {
        bool ok = false;
        qint64 v = parts[i].toLongLong(&ok);
        if (ok) {
            fields.append(v);
        }
    }
    if (fields.size() < 4) {
        return;
    }

    // total = 所有字段之和
    qint64 total = 0;
    for (qint64 v : fields) {
        total += v;
    }

    // idle = idle + iowait
    qint64 idle = fields.value(3);
    if (fields.size() > 4) {
        idle += fields.value(4);
    }

    qint64 diffTotal = total - m_prevTotal;
    qint64 diffIdle  = idle - m_prevIdle;

    // 首次采样时仅记录基线，不计算使用率
    if (m_prevTotal != 0 && diffTotal > 0) {
        float usage = (diffTotal - diffIdle) * 100.0f / static_cast<float>(diffTotal);
        m_cpuUsage = usage;

        m_cpuHistory.append(usage);
        while (m_cpuHistory.size() > HISTORY_SIZE) {
            m_cpuHistory.removeFirst();
        }
    }

    m_prevTotal = total;
    m_prevIdle  = idle;
}

// ---------------------------------------------------------------------------
// readMemInfo: 读取 /proc/meminfo 计算内存使用情况
// ---------------------------------------------------------------------------
//
// 解析 MemTotal 和 MemAvailable（单位 kB），转为字节。
//   memTotal = MemTotal * 1024
//   memUsed  = (MemTotal - MemAvailable) * 1024
//   memUsage = (MemTotal - MemAvailable) * 100 / MemTotal
//
// ---------------------------------------------------------------------------

void SystemMonitor::readMemInfo()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SystemMonitor: failed to open /proc/meminfo";
        return;
    }

    qint64 memTotal = 0;
    qint64 memAvailable = 0;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith(QStringLiteral("MemTotal:"))) {
            const auto cols = line.split(' ', Qt::SkipEmptyParts);
            if (cols.size() >= 2) {
                memTotal = cols[1].toLongLong();
            }
        } else if (line.startsWith(QStringLiteral("MemAvailable:"))) {
            const auto cols = line.split(' ', Qt::SkipEmptyParts);
            if (cols.size() >= 2) {
                memAvailable = cols[1].toLongLong();
            }
        }
    }
    file.close();

    if (memTotal <= 0) {
        return;
    }

    m_memTotal = memTotal * 1024;                                   // kB -> bytes
    m_memUsed  = (memTotal - memAvailable) * 1024;                  // kB -> bytes
    m_memUsage = (memTotal - memAvailable) * 100.0f / static_cast<float>(memTotal);
}

// ---------------------------------------------------------------------------
// readNetDev: 读取 /proc/net/dev 计算网络上传/下载速率
// ---------------------------------------------------------------------------
//
// /proc/net/dev 格式（取第一个非 lo 接口）：
//   iface: rx_bytes rx_packets rx_errs rx_drop rx_fifo rx_frame
//          rx_compressed rx_multicast tx_bytes tx_packets ...
//
// 速率 = (bytes差 / 时间差(秒)) / 1024  -> KB/s
//
// 首次采样仅记录基线。
//
// ---------------------------------------------------------------------------

void SystemMonitor::readNetDev()
{
    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "SystemMonitor: failed to open /proc/net/dev";
        return;
    }

    qint64 rxBytes = 0;
    qint64 txBytes = 0;
    bool found = false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        // 表头行不含 ':'，直接跳过
        int colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }

        QString iface = line.left(colon).trimmed();
        if (iface.isEmpty() || iface == QStringLiteral("lo")) {
            continue;
        }

        // 取第一个非 lo 接口
        // 冒号后依次为：rx_bytes(0) rx_packets(1) rx_errs(2) rx_drop(3)
        //              rx_fifo(4) rx_frame(5) rx_compressed(6) rx_multicast(7)
        //              tx_bytes(8) tx_packets(9) ...
        const auto fields = line.mid(colon + 1).trimmed()
                                .split(' ', Qt::SkipEmptyParts);
        if (fields.size() < 16) {
            continue;
        }

        rxBytes = fields[0].toLongLong();
        txBytes = fields[8].toLongLong();
        found = true;
        break;
    }
    file.close();

    if (!found) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();

    // 首次采样时仅记录基线
    if (m_prevNetTime != 0) {
        qint64 dtMs = now - m_prevNetTime;
        if (dtMs > 0) {
            double secs = dtMs / 1000.0;
            m_netDownload = (rxBytes - m_prevRxBytes) / secs / 1024.0;  // KB/s
            m_netUpload   = (txBytes - m_prevTxBytes) / secs / 1024.0;  // KB/s

            m_netDownHistory.append(m_netDownload);
            while (m_netDownHistory.size() > HISTORY_SIZE) {
                m_netDownHistory.removeFirst();
            }

            m_netUpHistory.append(m_netUpload);
            while (m_netUpHistory.size() > HISTORY_SIZE) {
                m_netUpHistory.removeFirst();
            }
        }
    }

    m_prevRxBytes = rxBytes;
    m_prevTxBytes = txBytes;
    m_prevNetTime = now;
}

// ---------------------------------------------------------------------------
// readTemperature: 读取系统温度（摄氏度）
// ---------------------------------------------------------------------------
//
// 尝试以下路径（值单位为毫摄氏度，除以 1000 得到摄氏度）：
//   /sys/class/thermal/thermal_zone0/temp
//   /sys/class/hwmon/hwmon0/temp1_input
//
// 全部失败时 m_temperature 置 0。
//
// ---------------------------------------------------------------------------

void SystemMonitor::readTemperature()
{
    static const QStringList tempPaths = {
        QStringLiteral("/sys/class/thermal/thermal_zone0/temp"),
        QStringLiteral("/sys/class/hwmon/hwmon0/temp1_input")
    };

    for (const QString &path : tempPaths) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            bool ok = false;
            qint64 raw = file.readAll().trimmed().toLongLong(&ok);
            file.close();
            if (ok) {
                m_temperature = raw / 1000.0f;
                return;
            }
        }
    }

    // 所有路径均不可用
    m_temperature = 0;
}
