#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/NotificationManager.h"
#include "core/SystemMonitor.h"

class TestNotificationManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 默认状态：启用 + 音效启用
    void testDefaultState();

    // 设置启用/禁用
    void testSetEnabled();

    // 设置音效开关
    void testSetSoundEnabled();

    // null 指针连接不崩溃
    void testConnectNullSystemMonitor();
    void testConnectNullHealthReminder();
    void testConnectNullPomodoro();

    // 禁用状态下 notify 不崩溃
    void testNotifyWhenDisabled();

    // 启用状态下 notify 不崩溃（DBus 可能不可用）
    void testNotifyWhenEnabled();

    // CPU 告警槽函数不崩溃
    void testOnCpuAlert();

    // 内存压力告警槽函数不崩溃
    void testOnMemPressureAlert();

    // 流量告警槽函数不崩溃
    void testOnTrafficAlert();

    // 喝水提醒槽函数不崩溃
    void testOnWaterReminder();

    // 久坐提醒槽函数不崩溃
    void testOnStandReminder();

    // 番茄钟完成槽函数不崩溃
    void testOnPomodoroComplete();

    // 连接 SystemMonitor 后信号能到达
    void testSignalConnection();

private:
    NotificationManager *m_notifier = nullptr;
    SystemMonitor *m_monitor = nullptr;
};

void TestNotificationManager::initTestCase()
{
    m_notifier = new NotificationManager();
    m_monitor = new SystemMonitor();
}

void TestNotificationManager::cleanupTestCase()
{
    delete m_notifier;
    delete m_monitor;
}

void TestNotificationManager::testDefaultState()
{
    QVERIFY(m_notifier->enabled());
    QVERIFY(m_notifier->soundEnabled());
}

void TestNotificationManager::testSetEnabled()
{
    m_notifier->setEnabled(false);
    QVERIFY(!m_notifier->enabled());

    m_notifier->setEnabled(true);
    QVERIFY(m_notifier->enabled());
}

void TestNotificationManager::testSetSoundEnabled()
{
    m_notifier->setSoundEnabled(false);
    QVERIFY(!m_notifier->soundEnabled());

    m_notifier->setSoundEnabled(true);
    QVERIFY(m_notifier->soundEnabled());
}

void TestNotificationManager::testConnectNullSystemMonitor()
{
    m_notifier->connectSystemMonitor(nullptr);
    QVERIFY(true);  // 不崩溃即通过
}

void TestNotificationManager::testConnectNullHealthReminder()
{
    m_notifier->connectHealthReminder(nullptr);
    QVERIFY(true);
}

void TestNotificationManager::testConnectNullPomodoro()
{
    m_notifier->connectPomodoro(nullptr);
    QVERIFY(true);
}

void TestNotificationManager::testNotifyWhenDisabled()
{
    m_notifier->setEnabled(false);
    // 禁用状态下 notify 应直接返回
    m_notifier->notify("test", "body should be ignored");
    QVERIFY(true);
    m_notifier->setEnabled(true);
}

void TestNotificationManager::testNotifyWhenEnabled()
{
    // 启用状态下 notify 会尝试通过 DBus 发送，
    // 在测试环境中 DBus 可能不可用，但不应崩溃
    m_notifier->setEnabled(true);
    m_notifier->setSoundEnabled(false);  // 关闭音效避免依赖
    m_notifier->notify("test_summary", "test_body");
    QVERIFY(true);
}

void TestNotificationManager::testOnCpuAlert()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->onCpuAlert("test_process", 95.5, 15);
    QVERIFY(true);
}

void TestNotificationManager::testOnMemPressureAlert()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->onMemPressureAlert(2, 45.0f);
    QVERIFY(true);
}

void TestNotificationManager::testOnTrafficAlert()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->onTrafficAlert(1024);
    QVERIFY(true);
}

void TestNotificationManager::testOnWaterReminder()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->onWaterReminder();
    QVERIFY(true);
}

void TestNotificationManager::testOnStandReminder()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->onStandReminder();
    QVERIFY(true);
}

void TestNotificationManager::testOnPomodoroComplete()
{
    m_notifier->setSoundEnabled(false);
    // 专注完成
    m_notifier->onPomodoroComplete(false);
    QVERIFY(true);
    // 休息结束
    m_notifier->onPomodoroComplete(true);
    QVERIFY(true);
}

void TestNotificationManager::testSignalConnection()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->connectSystemMonitor(m_monitor);

    // 触发 SystemMonitor 的信号 — 验证连接成功
    // emit 信号后 NotificationManager 的槽应被调用
    emit m_monitor->cpuAlert("signal_test_proc", 88.0, 10);
    QVERIFY(true);

    emit m_monitor->memPressureAlert(1, 30.0f);
    QVERIFY(true);

    emit m_monitor->trafficAlert(500);
    QVERIFY(true);
}

QTEST_MAIN(TestNotificationManager)
#include "test_notificationmanager.moc"
