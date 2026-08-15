#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QTimer>
#include "ui/PomodoroWidget.h"

class TestPomodoroWidget : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 初始状态：25分钟专注，未运行
    void testInitialState();

    // remaining() 返回初始 25*60
    void testInitialRemaining();

    // isRunning() 初始为 false
    void testNotRunningInitially();

    // isBreak() 初始为 false
    void testNotBreakInitially();

    // 通过模拟点击开始按钮启动计时器
    void testStartButton();

    // 启动后 remaining 每秒递减
    void testTimerTicks();

    // 重置按钮回到初始状态
    void testResetButton();

    // sessionComplete 信号可被监听
    void testSessionCompleteSignal();

    // 开始 -> 暂停 -> 继续 状态变化
    void testStartPauseResume();

private:
    PomodoroWidget *m_pomodoro = nullptr;

    /// 查找 DPushButton by text
    DPushButton *findButton(PomodoroWidget *w, const QString &text);
};

DPushButton *TestPomodoroWidget::findButton(PomodoroWidget *w, const QString &text)
{
    auto buttons = w->findChildren<DPushButton*>();
    for (auto *btn : buttons) {
        if (btn->text().contains(text))
            return btn;
    }
    return nullptr;
}

void TestPomodoroWidget::initTestCase()
{
    m_pomodoro = new PomodoroWidget();
    m_pomodoro->resize(280, 640);
    m_pomodoro->show();
}

void TestPomodoroWidget::cleanupTestCase()
{
    delete m_pomodoro;
}

void TestPomodoroWidget::testInitialState()
{
    QVERIFY(!m_pomodoro->isRunning());
    QVERIFY(!m_pomodoro->isBreak());
    QCOMPARE(m_pomodoro->remaining(), 25 * 60);
}

void TestPomodoroWidget::testInitialRemaining()
{
    QCOMPARE(m_pomodoro->remaining(), 25 * 60);
}

void TestPomodoroWidget::testNotRunningInitially()
{
    QVERIFY(!m_pomodoro->isRunning());
}

void TestPomodoroWidget::testNotBreakInitially()
{
    QVERIFY(!m_pomodoro->isBreak());
}

void TestPomodoroWidget::testStartButton()
{
    DPushButton *btnStart = findButton(m_pomodoro, QStringLiteral("开始"));
    QVERIFY(btnStart != nullptr);

    // 点击开始按钮
    QTest::mouseClick(btnStart, Qt::LeftButton);
    QTest::qWait(100);

    QVERIFY(m_pomodoro->isRunning());

    // 暂停
    QTest::mouseClick(btnStart, Qt::LeftButton);
    QTest::qWait(100);

    QVERIFY(!m_pomodoro->isRunning());
}

void TestPomodoroWidget::testTimerTicks()
{
    DPushButton *btnStart = findButton(m_pomodoro, QStringLiteral("继续"));
    if (!btnStart)
        btnStart = findButton(m_pomodoro, QStringLiteral("开始"));
    QVERIFY(btnStart != nullptr);

    int before = m_pomodoro->remaining();

    // 启动计时器
    QTest::mouseClick(btnStart, Qt::LeftButton);
    QVERIFY(m_pomodoro->isRunning());

    // 等待 2.5 秒（至少 2 次 tick）
    QTest::qWait(2500);

    int after = m_pomodoro->remaining();
    QVERIFY2(after < before,
             QString("Expected remaining < %1, got %2").arg(before).arg(after).toUtf8());

    // 暂停
    QTest::mouseClick(btnStart, Qt::LeftButton);
}

void TestPomodoroWidget::testResetButton()
{
    DPushButton *btnReset = findButton(m_pomodoro, QStringLiteral("重置"));
    QVERIFY(btnReset != nullptr);

    // 先启动
    DPushButton *btnStart = findButton(m_pomodoro, QStringLiteral("继续"));
    if (!btnStart)
        btnStart = findButton(m_pomodoro, QStringLiteral("开始"));
    if (btnStart) {
        QTest::mouseClick(btnStart, Qt::LeftButton);
        QTest::qWait(100);
    }

    // 重置
    QTest::mouseClick(btnReset, Qt::LeftButton);
    QTest::qWait(100);

    QVERIFY(!m_pomodoro->isRunning());
    QVERIFY(!m_pomodoro->isBreak());
    QCOMPARE(m_pomodoro->remaining(), 25 * 60);
}

void TestPomodoroWidget::testSessionCompleteSignal()
{
    PomodoroWidget p;
    QSignalSpy spy(&p, &PomodoroWidget::sessionComplete);

    // 信号初始不应触发
    QTest::qWait(100);
    QCOMPARE(spy.count(), 0);

    // 验证信号可连接
    QVERIFY(spy.isValid());
}

void TestPomodoroWidget::testStartPauseResume()
{
    PomodoroWidget p;
    p.resize(280, 640);

    DPushButton *btnStart = findButton(&p, QStringLiteral("开始"));
    QVERIFY(btnStart != nullptr);

    // 开始
    QTest::mouseClick(btnStart, Qt::LeftButton);
    QTest::qWait(50);
    QVERIFY(p.isRunning());

    // 暂停
    btnStart = findButton(&p, QStringLiteral("暂停"));
    QVERIFY(btnStart != nullptr);
    QTest::mouseClick(btnStart, Qt::LeftButton);
    QTest::qWait(50);
    QVERIFY(!p.isRunning());

    // 继续
    btnStart = findButton(&p, QStringLiteral("继续"));
    QVERIFY(btnStart != nullptr);
    QTest::mouseClick(btnStart, Qt::LeftButton);
    QTest::qWait(50);
    QVERIFY(p.isRunning());

    // 重置
    DPushButton *btnReset = findButton(&p, QStringLiteral("重置"));
    QTest::mouseClick(btnReset, Qt::LeftButton);
    QVERIFY(!p.isRunning());
    QVERIFY(!p.isBreak());
    QCOMPARE(p.remaining(), 25 * 60);
}

QTEST_MAIN(TestPomodoroWidget)
#include "test_pomodoro.moc"
