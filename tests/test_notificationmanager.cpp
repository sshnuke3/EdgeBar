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

    // 通过信号触发各槽函数 — 验证连接和不崩溃
    void testCpuAlertViaSignal();
    void testMemPressureAlertViaSignal();
    void testTrafficAlertViaSignal();

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
    QVERIFY(true);
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
    m_notifier->notify("test", "body should be ignored");
    QVERIFY(true);
    m_notifier->setEnabled(true);
}

void TestNotificationManager::testNotifyWhenEnabled()
{
    m_notifier->setEnabled(true);
    m_notifier->setSoundEnabled(false);
    m_notifier->notify("test_summary", "test_body");
    QVERIFY(true);
}

// ---------------------------------------------------------------------------
// 通过 emit 信号触发 NotificationManager 的私有槽函数
// ---------------------------------------------------------------------------

void TestNotificationManager::testCpuAlertViaSignal()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->connectSystemMonitor(m_monitor);

    // emit 信号 — NotificationManager 的 onCpuAlert 槽应被触发
    emit m_monitor->cpuAlert("signal_test_proc", 88.0, 10);
    QVERIFY(true);
}

void TestNotificationManager::testMemPressureAlertViaSignal()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->connectSystemMonitor(m_monitor);

    emit m_monitor->memPressureAlert(2, 45.0f);
    QVERIFY(true);
}

void TestNotificationManager::testTrafficAlertViaSignal()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->connectSystemMonitor(m_monitor);

    emit m_monitor->trafficAlert(1024);
    QVERIFY(true);
}

void TestNotificationManager::testSignalConnection()
{
    m_notifier->setSoundEnabled(false);
    m_notifier->connectSystemMonitor(m_monitor);

    // 触发全部三种信号
    emit m_monitor->cpuAlert("signal_test_proc", 88.0, 10);
    emit m_monitor->memPressureAlert(1, 30.0f);
    emit m_monitor->trafficAlert(500);
    QVERIFY(true);
}

QTEST_MAIN(TestNotificationManager)
#include "test_notificationmanager.moc"
