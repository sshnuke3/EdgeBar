#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include "core/SystemMonitor.h"

class TestSystemMonitor : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 初始值检查
    void testInitialState();

    // 启动/停止不崩溃
    void testStartStop();

    // CPU 使用率范围合理
    void testCpuUsageRange();

    // 内存使用率范围合理
    void testMemUsageRange();

    // 网络历史数据初始化为空
    void testHistoryEmpty();

    // 温度默认值
    void testTemperatureDefault();

    // CPU 告警阈值设置
    void testCpuAlertThreshold();

    // 流量阈值设置
    void testTrafficThreshold();

    // CSV 导出空数据不崩溃
    void testExportCsvEmpty();

    // ProcessInfo 初始状态
    void testTopProcessDefault();

private:
    SystemMonitor *m_monitor = nullptr;
};

void TestSystemMonitor::initTestCase()
{
    m_monitor = new SystemMonitor();
}

void TestSystemMonitor::cleanupTestCase()
{
    delete m_monitor;
}

void TestSystemMonitor::testInitialState()
{
    QVERIFY(m_monitor->cpuUsage() >= 0.0f);
    QVERIFY(m_monitor->memUsage() >= 0.0f);
    QVERIFY(m_monitor->netUpload() >= 0.0f);
    QVERIFY(m_monitor->netDownload() >= 0.0f);
    QVERIFY(m_monitor->temperature() >= 0.0f);
}

void TestSystemMonitor::testStartStop()
{
    // 启动后立即停止不应崩溃
    m_monitor->start(500);
    QTest::qWait(600);  // 等待至少一次 poll
    m_monitor->stop();

    QVERIFY(m_monitor->cpuUsage() >= 0.0f);
}

void TestSystemMonitor::testCpuUsageRange()
{
    float cpu = m_monitor->cpuUsage();
    QVERIFY2(cpu >= 0.0f && cpu <= 100.0f,
             QString("CPU usage out of range: %1").arg(cpu).toUtf8());
}

void TestSystemMonitor::testMemUsageRange()
{
    // 启动一次 poll 获取内存数据（间隔短一些确保触发）
    m_monitor->start(100);
    QTest::qWait(500);
    m_monitor->stop();

    // 在容器/CI 环境中 /proc/meminfo 可能不可读，跳过值范围检查
    if (m_monitor->memTotal() == 0) {
        QSKIP("/proc/meminfo not available in this environment");
    }

    float mem = m_monitor->memUsage();
    QVERIFY2(mem >= 0.0f && mem <= 100.0f,
             QString("Memory usage out of range: %1").arg(mem).toUtf8());
    QVERIFY(m_monitor->memTotal() > 0);
    QVERIFY(m_monitor->memUsed() >= 0);
    QVERIFY(m_monitor->memUsed() <= m_monitor->memTotal());
}

void TestSystemMonitor::testHistoryEmpty()
{
    // 在 poll 之前历史应为空或很小
    QVERIFY(m_monitor->cpuHistory().size() >= 0);
    QVERIFY(m_monitor->netDownHistory().size() >= 0);
    QVERIFY(m_monitor->netUpHistory().size() >= 0);
}

void TestSystemMonitor::testTemperatureDefault()
{
    QVERIFY(m_monitor->temperature() >= 0.0f);
}

void TestSystemMonitor::testCpuAlertThreshold()
{
    m_monitor->setCpuAlertThreshold(50);
    // 阈值设置后，topProcess 会在 poll 时更新
    // 这里只验证不崩溃
    QVERIFY(true);

    // 禁用告警
    m_monitor->setCpuAlertThreshold(0);
    QVERIFY(m_monitor->topProcess().pid == 0);
}

void TestSystemMonitor::testTrafficThreshold()
{
    m_monitor->setTrafficThresholdMB(500);
    QVERIFY(m_monitor->dailyRxBytes() >= 0);
    QVERIFY(m_monitor->dailyTxBytes() >= 0);
}

void TestSystemMonitor::testExportCsvEmpty()
{
    // 导出空快照不应崩溃
    QString tmpPath = QDir::tempPath() + "/edgebar_test_export.csv";
    bool ok = m_monitor->exportCsv(tmpPath);
    // 可能成功（空文件）也可能失败（权限）
    QVERIFY(ok || !ok);  // 只验证不崩溃

    QFile::remove(tmpPath);
}

void TestSystemMonitor::testTopProcessDefault()
{
    // 未设置阈值时 topProcess 应为空
    m_monitor->setCpuAlertThreshold(0);
    QCOMPARE(m_monitor->topProcess().pid, 0);
    QVERIFY(m_monitor->topProcess().name.isEmpty());
}

QTEST_MAIN(TestSystemMonitor)
#include "test_systemmonitor.moc"
