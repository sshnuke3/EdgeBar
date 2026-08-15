#include <QtTest/QtTest>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include "core/AutostartManager.h"

class TestAutostartManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    // 初始状态：应为禁用
    void testInitialState();

    // enable() 创建 .desktop 文件
    void testEnable();

    // isEnabled() 在 enable 后返回 true
    void testIsEnabledAfterEnable();

    // disable() 删除 .desktop 文件
    void testDisable();

    // isEnabled() 在 disable 后返回 false
    void testIsEnabledAfterDisable();

    // 重复 enable 不崩溃
    void testDoubleEnable();

    // 重复 disable 不崩溃
    void testDoubleDisable();

    // .desktop 文件内容验证
    void testDesktopFileContent();

    // 完整的 enable -> disable -> enable 循环
    void testEnableDisableCycle();

private:
    AutostartManager *m_mgr = nullptr;
    QString m_desktopPath;
};

void TestAutostartManager::initTestCase()
{
    m_mgr = new AutostartManager();

    // 预期路径
    QString autostartDir = QStandardPaths::writableLocation(
        QStandardPaths::GenericConfigLocation) + "/autostart";
    m_desktopPath = autostartDir + "/edgebar.desktop";

    // 清理之前可能残留的文件
    QFile::remove(m_desktopPath);
}

void TestAutostartManager::cleanupTestCase()
{
    // 清理测试产生的文件
    QFile::remove(m_desktopPath);
    delete m_mgr;
}

void TestAutostartManager::testInitialState()
{
    QVERIFY(!m_mgr->isEnabled());
}

void TestAutostartManager::testEnable()
{
    bool ok = m_mgr->enable();
    QVERIFY(ok);
    QVERIFY(QFile::exists(m_desktopPath));
}

void TestAutostartManager::testIsEnabledAfterEnable()
{
    QVERIFY(m_mgr->isEnabled());
}

void TestAutostartManager::testDisable()
{
    bool ok = m_mgr->disable();
    QVERIFY(ok);
    QVERIFY(!QFile::exists(m_desktopPath));
}

void TestAutostartManager::testIsEnabledAfterDisable()
{
    QVERIFY(!m_mgr->isEnabled());
}

void TestAutostartManager::testDoubleEnable()
{
    m_mgr->enable();
    bool ok = m_mgr->enable();  // 再次 enable
    QVERIFY(ok);  // 覆盖写入，不崩溃
    QVERIFY(m_mgr->isEnabled());
    m_mgr->disable();
}

void TestAutostartManager::testDoubleDisable()
{
    bool ok = m_mgr->disable();  // 已经是禁用状态
    QVERIFY(ok);  // 已禁用时直接返回 true
    QVERIFY(!m_mgr->isEnabled());
}

void TestAutostartManager::testDesktopFileContent()
{
    m_mgr->enable();

    QFile file(m_desktopPath);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));

    QString content = QString::fromUtf8(file.readAll());
    file.close();

    // 验证必需字段
    QVERIFY(content.contains("[Desktop Entry]"));
    QVERIFY(content.contains("Type=Application"));
    QVERIFY(content.contains("Name=EdgeBar"));
    QVERIFY(content.contains("Exec=/usr/bin/edgebar"));
    QVERIFY(content.contains("Icon=sidebar"));
    QVERIFY(content.contains("Terminal=false"));
    QVERIFY(content.contains("X-GNOME-Autostart-enabled=true"));

    m_mgr->disable();
}

void TestAutostartManager::testEnableDisableCycle()
{
    // enable -> disable -> enable -> disable
    QVERIFY(m_mgr->enable());
    QVERIFY(m_mgr->isEnabled());

    QVERIFY(m_mgr->disable());
    QVERIFY(!m_mgr->isEnabled());

    QVERIFY(m_mgr->enable());
    QVERIFY(m_mgr->isEnabled());

    QVERIFY(m_mgr->disable());
    QVERIFY(!m_mgr->isEnabled());
}

QTEST_MAIN(TestAutostartManager)
#include "test_autostartmanager.moc"
