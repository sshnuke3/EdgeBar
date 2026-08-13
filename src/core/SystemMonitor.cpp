#include "SystemMonitor.h"
#include "Logging.h"

#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>

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
    connect(&m_timer, &QTimer::timeout, this, &SystemMonitor::poll, Qt::UniqueConnection);
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
    readTopProcess();

    // 累计当日流量
    QDate today = QDate::currentDate();
    if (m_trafficResetDate != today) {
        m_trafficResetDate = today;
        m_dailyRxBytes = 0;
        m_dailyTxBytes = 0;
    }

    // 采样间隔内的流量增量（bytes）
    if (m_prevNetTime > 0) {
        qint64 dtSec = 2;  // 近似间隔
        m_dailyRxBytes += static_cast<qint64>(m_netDownload * 1024 * dtSec);
        m_dailyTxBytes += static_cast<qint64>(m_netUpload * 1024 * dtSec);
    }

    // 记录快照
    Snapshot snap;
    snap.timestamp = QDateTime::currentSecsSinceEpoch();
    snap.cpu = m_cpuUsage;
    snap.mem = m_memUsage;
    snap.netDown = m_netDownload;
    snap.netUp = m_netUpload;
    snap.temp = m_temperature;
    m_snapshots.append(snap);
    while (m_snapshots.size() > MAX_SNAPSHOTS) {
        m_snapshots.removeFirst();
    }

    emit statsUpdated();
}

// ---------------------------------------------------------------------------
// readCpuStat: 读取 /proc/stat 计算 CPU 使用率
// ---------------------------------------------------------------------------

void SystemMonitor::readCpuStat()
{
    QFile file(QStringLiteral("/proc/stat"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(edgebarLog) << "Failed to open /proc/stat";
        return;
    }

    QString line = file.readLine();
    file.close();

    const auto parts = line.simplified().split(' ');
    if (parts.size() < 2 || parts[0] != QStringLiteral("cpu")) {
        return;
    }

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

    qint64 total = 0;
    for (qint64 v : fields) {
        total += v;
    }

    qint64 idle = fields.value(3);
    if (fields.size() > 4) {
        idle += fields.value(4);
    }

    qint64 diffTotal = total - m_prevTotal;
    qint64 diffIdle  = idle - m_prevIdle;

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

void SystemMonitor::readMemInfo()
{
    QFile file(QStringLiteral("/proc/meminfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(edgebarLog) << "Failed to open /proc/meminfo";
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

    m_memTotal = memTotal * 1024;
    m_memUsed  = (memTotal - memAvailable) * 1024;
    m_memUsage = (memTotal - memAvailable) * 100.0f / static_cast<float>(memTotal);
}

// ---------------------------------------------------------------------------
// readNetDev: 读取 /proc/net/dev 计算网络上传/下载速率
// ---------------------------------------------------------------------------

void SystemMonitor::readNetDev()
{
    QFile file(QStringLiteral("/proc/net/dev"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCWarning(edgebarLog) << "Failed to open /proc/net/dev";
        return;
    }

    qint64 rxBytes = 0;
    qint64 txBytes = 0;
    bool found = false;

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();

        int colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }

        QString iface = line.left(colon).trimmed();
        if (iface.isEmpty() || iface == QStringLiteral("lo")) {
            continue;
        }

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

    if (m_prevNetTime != 0) {
        qint64 dtMs = now - m_prevNetTime;
        if (dtMs > 0) {
            double secs = dtMs / 1000.0;
            m_netDownload = (rxBytes - m_prevRxBytes) / secs / 1024.0;
            m_netUpload   = (txBytes - m_prevTxBytes) / secs / 1024.0;

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
// readTemperature: 读取系统温度
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

    m_temperature = 0;
}

// ---------------------------------------------------------------------------
// readTopProcess: 扫描 /proc/[pid]/stat 找出 CPU 占用最高的进程
// ---------------------------------------------------------------------------
//
// /proc/[pid]/stat 格式（关键字段）：
//   field[0]  = pid
//   field[1]  = comm (进程名，带括号)
//   field[13] = utime (用户态 jiffies)
//   field[14] = stime (内核态 jiffies)
//
// CPU 使用率 = (utime+stime 的增量) / (总 jiffies 增量) * 100
// 总 jiffies 来自 readCpuStat() 的 m_prevTotal 差值。
//
// 只在 m_cpuAlertThreshold > 0 时扫描。
// 告警条件：进程 CPU% > 阈值（以单核计，即可超过 100）
//
// ---------------------------------------------------------------------------

void SystemMonitor::readTopProcess()
{
    // 阈值为 0 时禁用
    if (m_cpuAlertThreshold <= 0) {
        m_topProcess = ProcessInfo();
        return;
    }

    // 总 CPU jiffies 差值
    qint64 totalJiffies = m_prevTotal;  // 当前总 jiffies
    // 我们需要上一次的总 jiffies，但 m_prevTotal 已被 readCpuStat 更新
    // 所以用 m_prevProcTimes 里保存的 "上次总 jiffies" 来计算
    // 简化方案：直接用两次 poll 间的时间差 + CLOCKS_PER_SEC
    // 更可靠的方案：用 /proc/stat 的 total 差值

    // 遍历 /proc 目录下的数字子目录（每个是一个 pid）
    QDir procDir(QStringLiteral("/proc"));
    const auto entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    ProcessInfo bestProc;
    float bestCpu = 0;

    // 计算总 CPU 时间差
    qint64 diffTotal = (m_lastTotalJiffies > 0) ? (totalJiffies - m_lastTotalJiffies) : 0;
    m_lastTotalJiffies = totalJiffies;

    for (const QString &entry : entries) {
        bool ok = false;
        int pid = entry.toInt(&ok);
        if (!ok || pid <= 0) continue;

        QFile statFile(QStringLiteral("/proc/%1/stat").arg(pid));
        if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QString line = statFile.readLine();
        statFile.close();

        // 解析 /proc/[pid]/stat
        // 格式: pid (comm) state ppid ... utime stime ...
        // comm 可能包含空格和括号，需正确解析
        int firstParen = line.indexOf('(');
        int lastParen = line.lastIndexOf(')');
        if (firstParen < 0 || lastParen < 0) continue;

        QString name = line.mid(firstParen + 1, lastParen - firstParen - 1);

        // lastParen 之后的部分按空格分割
        QString rest = line.mid(lastParen + 1).trimmed();
        const auto fields = rest.split(' ', Qt::SkipEmptyParts);
        // fields[0] = state, fields[1] = ppid, ...
        // utime = field[11], stime = field[12] (0-indexed from rest)
        if (fields.size() < 13) continue;

        qint64 utime = fields[11].toLongLong();
        qint64 stime = fields[12].toLongLong();
        qint64 procTime = utime + stime;

        // 查找上次记录
        auto it = m_prevProcTimes.find(pid);
        if (it != m_prevProcTimes.end() && diffTotal > 0) {
            qint64 prevTime = it.value().first;
            qint64 diffProc = procTime - prevTime;

            // CPU% = diffProc / diffTotal * 100
            float cpuPercent = diffProc * 100.0f / static_cast<float>(diffTotal);

            if (cpuPercent > bestCpu) {
                bestCpu = cpuPercent;
                bestProc.pid = pid;
                bestProc.name = name;
                bestProc.cpuPercent = cpuPercent;
            }
        }

        // 更新记录
        m_prevProcTimes[pid] = qMakePair(procTime, now);
    }

    // 清理已退出的进程
    // 简单策略：如果 map 大小超过 500，清理最旧的
    if (m_prevProcTimes.size() > 500) {
        // 按时间戳排序，删除最旧的 100 个
        QList<int> pids = m_prevProcTimes.keys();
        std::sort(pids.begin(), pids.end(), [this](int a, int b) {
            return m_prevProcTimes[a].second < m_prevProcTimes[b].second;
        });
        for (int i = 0; i < 100 && i < pids.size(); ++i) {
            m_prevProcTimes.remove(pids[i]);
        }
    }

    // 设置 topProcess（只保留超过阈值的）
    if (bestCpu >= m_cpuAlertThreshold) {
        m_topProcess = bestProc;
        qCDebug(edgebarLog) << "CPU alert:" << bestProc.name
                            << "pid=" << bestProc.pid
                            << "cpu=" << bestProc.cpuPercent << "%";
    } else {
        m_topProcess = ProcessInfo();
    }
}

// ---------------------------------------------------------------------------
// exportCsv: 导出历史快照为 CSV 文件
// ---------------------------------------------------------------------------

bool SystemMonitor::exportCsv(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(edgebarLog) << "Cannot open file for CSV export:" << filePath;
        return false;
    }

    QTextStream out(&file);
    out << "timestamp,datetime,cpu_percent,mem_percent,net_down_kbs,net_up_kbs,temp_c\n";

    for (const auto &s : m_snapshots) {
        QDateTime dt = QDateTime::fromSecsSinceEpoch(s.timestamp);
        out << s.timestamp << ","
            << dt.toString("yyyy-MM-dd HH:mm:ss") << ","
            << QString::number(s.cpu, 'f', 1) << ","
            << QString::number(s.mem, 'f', 1) << ","
            << QString::number(s.netDown, 'f', 1) << ","
            << QString::number(s.netUp, 'f', 1) << ","
            << QString::number(s.temp, 'f', 1) << "\n";
    }

    file.close();
    qCInfo(edgebarLog) << "CSV exported:" << filePath
                        << "rows:" << m_snapshots.size();
    return true;
}
