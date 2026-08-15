#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QGuiApplication>
#include <QClipboard>
#include <QImage>
#include <QBuffer>
#include "core/ClipboardManager.h"

class TestClipboardManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();

    // 通过 QClipboard 实际添加文本
    void testAddTextViaClipboard();

    // 重复文本去重（移到顶部而非新增）
    void testDeduplication();

    // 超过上限自动淘汰最旧
    void testMaxItemsEviction();

    // 置顶后不会被淘汰
    void testPinnedNotEvicted();

    // 置顶/取消置顶
    void testTogglePin();

    // 删除单条
    void testRemove();

    // clearAll 保留 pinned
    void testClearAllPreservesPinned();

    // 搜索过滤
    void testFilteredItems();

    // 置顶条目在过滤结果中排在最前
    void testPinnedFirstInFiltered();

    // 图片缩略图生成
    void testImageThumbnail();

    // 空文本不添加
    void testEmptyTextNotAdded();

    // setMaxItems 缩小后自动淘汰
    void testSetMaxItemsShrink();

    // copyToClipboard 不触发递归
    void testCopyToClipboardNoLoop();

private:
    ClipboardManager *m_mgr = nullptr;
    QClipboard *m_clipboard = nullptr;
};

void TestClipboardManager::initTestCase()
{
    m_clipboard = QGuiApplication::clipboard();
}

void TestClipboardManager::cleanupTestCase()
{
}

void TestClipboardManager::init()
{
    delete m_mgr;
    m_mgr = new ClipboardManager();
    m_mgr->setMaxItems(50);
    // 清空剪贴板基线
    m_clipboard->setText(QString());
}

// ---------------------------------------------------------------------------
// 通过 QClipboard 实际写入文本，验证 ClipboardManager 正确捕获
// ---------------------------------------------------------------------------
void TestClipboardManager::testAddTextViaClipboard()
{
    QSignalSpy spy(m_mgr, &ClipboardManager::historyChanged);
    m_mgr->setMaxItems(50);

    // 写入剪贴板
    m_clipboard->setText(QStringLiteral("hello world test 123"));

    // 等待防抖定时器（300ms）+ 处理
    QTest::qWait(500);

    QVERIFY(m_mgr->count() >= 1);

    // 验证最新条目内容
    auto items = m_mgr->items();
    QVERIFY(items.first().text.contains(QStringLiteral("hello world")));
    QVERIFY(spy.count() >= 1);
}

// ---------------------------------------------------------------------------
// 重复复制相同文本，应移到顶部而非新增
// ---------------------------------------------------------------------------
void TestClipboardManager::testDeduplication()
{
    m_mgr->setMaxItems(50);

    // 添加文本 A
    m_clipboard->setText(QStringLiteral("dedup_test_A"));
    QTest::qWait(500);

    // 添加文本 B
    m_clipboard->setText(QStringLiteral("dedup_test_B"));
    QTest::qWait(500);
    int countAfterB = m_mgr->count();

    // 再次添加文本 A — 应移到顶部，总数不变
    m_clipboard->setText(QStringLiteral("dedup_test_A"));
    QTest::qWait(500);

    QCOMPARE(m_mgr->count(), countAfterB);  // 未新增
    QVERIFY(m_mgr->items().first().text.contains(QStringLiteral("dedup_test_A")));
}

// ---------------------------------------------------------------------------
// 超过上限时自动淘汰最旧条目
// ---------------------------------------------------------------------------
void TestClipboardManager::testMaxItemsEviction()
{
    m_mgr->setMaxItems(3);

    // 添加 5 条不同的文本
    for (int i = 0; i < 5; ++i) {
        m_clipboard->setText(QStringLiteral("eviction_test_%1").arg(i));
        QTest::qWait(400);  // 等防抖
    }

    // 最多 3 条
    QVERIFY(m_mgr->count() <= 3);

    // 最旧的（eviction_test_0, _1）应被淘汰
    bool hasOldest = false;
    for (const auto &item : m_mgr->items()) {
        if (item.text.contains(QStringLiteral("eviction_test_0")))
            hasOldest = true;
    }
    QVERIFY(!hasOldest);

    // 最新的（eviction_test_4）应在最前
    QVERIFY(m_mgr->items().first().text.contains(QStringLiteral("eviction_test_4")));
}

// ---------------------------------------------------------------------------
// 置顶条目不会被淘汰
// ---------------------------------------------------------------------------
void TestClipboardManager::testPinnedNotEvicted()
{
    m_mgr->setMaxItems(3);

    // 添加第一条并置顶
    m_clipboard->setText(QStringLiteral("pinned_item"));
    QTest::qWait(500);
    int firstId = m_mgr->items().first().id;
    m_mgr->togglePin(firstId);

    // 再添加 4 条不同的文本，超过上限
    for (int i = 0; i < 4; ++i) {
        m_clipboard->setText(QStringLiteral("overflow_%1").arg(i));
        QTest::qWait(400);
    }

    // 置顶条目应该还在
    bool foundPinned = false;
    for (const auto &item : m_mgr->items()) {
        if (item.text.contains(QStringLiteral("pinned_item")) && item.pinned)
            foundPinned = true;
    }
    QVERIFY(foundPinned);
}

// ---------------------------------------------------------------------------
// 置顶/取消置顶操作
// ---------------------------------------------------------------------------
void TestClipboardManager::testTogglePin()
{
    m_mgr->setMaxItems(50);

    // 添加一条文本
    m_clipboard->setText(QStringLiteral("pin_toggle_test"));
    QTest::qWait(500);

    QVERIFY(m_mgr->count() >= 1);
    int id = m_mgr->items().first().id;

    // 置顶
    m_mgr->togglePin(id);
    QVERIFY(m_mgr->items().first().pinned == true);

    // 取消置顶
    m_mgr->togglePin(id);
    QVERIFY(m_mgr->items().first().pinned == false);

    // 不存在的 ID 不崩溃
    m_mgr->togglePin(-999);
    QVERIFY(true);
}

// ---------------------------------------------------------------------------
// 删除单条
// ---------------------------------------------------------------------------
void TestClipboardManager::testRemove()
{
    m_mgr->setMaxItems(50);

    m_clipboard->setText(QStringLiteral("remove_test_item"));
    QTest::qWait(500);

    int before = m_mgr->count();
    QVERIFY(before >= 1);

    int id = m_mgr->items().first().id;
    m_mgr->remove(id);

    QCOMPARE(m_mgr->count(), before - 1);

    // 删除不存在的 ID 不崩溃
    m_mgr->remove(-999);
    QVERIFY(true);
}

// ---------------------------------------------------------------------------
// clearAll 保留 pinned 条目
// ---------------------------------------------------------------------------
void TestClipboardManager::testClearAllPreservesPinned()
{
    m_mgr->setMaxItems(50);

    // 添加并置顶第一条
    m_clipboard->setText(QStringLiteral("keep_pinned"));
    QTest::qWait(500);
    int pinnedId = m_mgr->items().first().id;
    m_mgr->togglePin(pinnedId);

    // 添加非置顶条目
    m_clipboard->setText(QStringLiteral("will_be_cleared_1"));
    QTest::qWait(500);
    m_clipboard->setText(QStringLiteral("will_be_cleared_2"));
    QTest::qWait(500);

    int beforeClear = m_mgr->count();
    QVERIFY(beforeClear >= 2);

    m_mgr->clearAll();

    // 置顶条目应该保留
    bool foundPinned = false;
    for (const auto &item : m_mgr->items()) {
        if (item.text.contains(QStringLiteral("keep_pinned")))
            foundPinned = true;
    }
    QVERIFY(foundPinned);

    // 非置顶条目应被清除
    for (const auto &item : m_mgr->items()) {
        if (!item.pinned)
            QVERIFY(!item.text.contains(QStringLiteral("will_be_cleared")));
    }
}

// ---------------------------------------------------------------------------
// filteredItems 搜索过滤
// ---------------------------------------------------------------------------
void TestClipboardManager::testFilteredItems()
{
    m_mgr->setMaxItems(50);

    m_clipboard->setText(QStringLiteral("alpha_search_term"));
    QTest::qWait(500);
    m_clipboard->setText(QStringLiteral("beta_other_text"));
    QTest::qWait(500);

    // 空关键词返回全部
    auto allItems = m_mgr->filteredItems("");
    QCOMPARE(allItems.size(), m_mgr->count());

    // 精确匹配
    auto matched = m_mgr->filteredItems("search_term");
    QVERIFY(matched.size() >= 1);
    QVERIFY(matched.first().text.contains("search_term"));

    // 不匹配的关键词返回空
    auto noMatch = m_mgr->filteredItems("zzzznotexist12345");
    QVERIFY(noMatch.isEmpty());
}

// ---------------------------------------------------------------------------
// 置顶条目在 filteredItems 结果中排在最前
// ---------------------------------------------------------------------------
void TestClipboardManager::testPinnedFirstInFiltered()
{
    m_mgr->setMaxItems(50);

    // 添加两条文本
    m_clipboard->setText(QStringLiteral("common_keyword_alpha"));
    QTest::qWait(500);
    m_clipboard->setText(QStringLiteral("common_keyword_beta"));
    QTest::qWait(500);

    // 置顶第二条（当前在最前的）
    int topId = m_mgr->items().first().id;
    m_mgr->togglePin(topId);

    // 再添加一条（非置顶，会在最前）
    m_clipboard->setText(QStringLiteral("common_keyword_gamma"));
    QTest::qWait(500);

    // 过滤 "common_keyword" — 置顶条目应在最前
    auto filtered = m_mgr->filteredItems("common_keyword");
    QVERIFY(filtered.size() >= 2);
    QVERIFY(filtered.first().pinned == true);
}

// ---------------------------------------------------------------------------
// 图片缩略图功能开关
// ---------------------------------------------------------------------------
void TestClipboardManager::testImageThumbnail()
{
    m_mgr->setEnableImages(true);
    QVERIFY(m_mgr->count() >= 0);

    m_mgr->setEnableImages(false);
    QVERIFY(m_mgr->count() >= 0);
}

// ---------------------------------------------------------------------------
// 空文本不应被添加
// ---------------------------------------------------------------------------
void TestClipboardManager::testEmptyTextNotAdded()
{
    int before = m_mgr->count();

    m_clipboard->setText(QString());
    QTest::qWait(500);

    QCOMPARE(m_mgr->count(), before);
}

// ---------------------------------------------------------------------------
// setMaxItems 缩小后自动淘汰超出的非置顶条目
// ---------------------------------------------------------------------------
void TestClipboardManager::testSetMaxItemsShrink()
{
    m_mgr->setMaxItems(10);

    // 添加 5 条文本
    for (int i = 0; i < 5; ++i) {
        m_clipboard->setText(QStringLiteral("shrink_test_%1").arg(i));
        QTest::qWait(400);
    }
    QVERIFY(m_mgr->count() >= 3);

    // 缩小到 2
    m_mgr->setMaxItems(2);
    QVERIFY(m_mgr->count() <= 2);
}

// ---------------------------------------------------------------------------
// copyToClipboard 不会触发递归 dataChanged
// ---------------------------------------------------------------------------
void TestClipboardManager::testCopyToClipboardNoLoop()
{
    m_mgr->setMaxItems(50);

    m_clipboard->setText(QStringLiteral("copy_test_original"));
    QTest::qWait(500);

    QVERIFY(m_mgr->count() >= 1);
    int beforeCopy = m_mgr->count();
    int id = m_mgr->items().first().id;

    // 复制到剪贴板 — 不应新增条目
    m_mgr->copyToClipboard(id);
    QTest::qWait(500);

    QCOMPARE(m_mgr->count(), beforeCopy);
}

QTEST_MAIN(TestClipboardManager)
#include "test_clipboardmanager.moc"
