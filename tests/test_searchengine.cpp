#include <QtTest/QtTest>
#include "core/SearchEngine.h"

class TestSearchEngine : public QObject
{
    Q_OBJECT

private slots:
    // 完全匹配应该是最高分
    void testExactMatch();

    // 子序列匹配基本功能
    void testSubsequenceMatch();

    // 不匹配的子序列返回 0
    void testNoMatch();

    // 空输入返回 0
    void testEmptyInput();

    // 大小写不敏感
    void testCaseInsensitive();

    // 首字母匹配比非首字母得分高
    void testFirstCharBonus();

    // 连续匹配比分散匹配得分高
    void testConsecutiveBonus();

    // 词边界匹配加分
    void testWordBoundaryBonus();

    // containsMatch 基本功能
    void testContainsMatch();

    // containsMatch 大小写不敏感
    void testContainsMatchCaseInsensitive();
};

void TestSearchEngine::testExactMatch()
{
    int score = SearchEngine::fuzzyScore("terminal", "terminal");
    QVERIFY(score >= 108);  // 100 + 8 chars
    QCOMPARE(score, 100 + 8);
}

void TestSearchEngine::testSubsequenceMatch()
{
    // "term" 是 "terminal" 的子序列
    int score = SearchEngine::fuzzyScore("term", "terminal");
    QVERIFY(score > 0);
}

void TestSearchEngine::testNoMatch()
{
    // "xyz" 不是 "terminal" 的子序列
    int score = SearchEngine::fuzzyScore("xyz", "terminal");
    QCOMPARE(score, 0);
}

void TestSearchEngine::testEmptyInput()
{
    QCOMPARE(SearchEngine::fuzzyScore("", "terminal"), 0);
    QCOMPARE(SearchEngine::fuzzyScore("term", ""), 0);
    QCOMPARE(SearchEngine::fuzzyScore("", ""), 0);
}

void TestSearchEngine::testCaseInsensitive()
{
    int lower = SearchEngine::fuzzyScore("term", "terminal");
    int upper = SearchEngine::fuzzyScore("TERM", "Terminal");
    QCOMPARE(lower, upper);
}

void TestSearchEngine::testFirstCharBonus()
{
    // 首字母匹配 vs 中间匹配，同一 target
    int fromStart = SearchEngine::fuzzyScore("te", "terminal");
    int fromMid = SearchEngine::fuzzyScore("er", "terminal");
    QVERIFY(fromStart > fromMid);
}

void TestSearchEngine::testConsecutiveBonus()
{
    // "term" 连续匹配 "terminal" 开头
    int consecutive = SearchEngine::fuzzyScore("term", "terminal");
    // "tm" 非连续匹配（t-...-m 在 terminal 中分散）
    int scattered = SearchEngine::fuzzyScore("tm", "terminal");
    // 连续的应该得分更高（4字符 vs 2字符，但每字符连续 +5）
    QVERIFY(consecutive > scattered);
}

void TestSearchEngine::testWordBoundaryBonus()
{
    // "ba" 在 "badminton" 开头匹配（首字母+词边界）
    int atBoundary = SearchEngine::fuzzyScore("ba", "badminton");
    // "ad" 在 "badminton" 中从位置1开始匹配（非词边界）
    int midWord = SearchEngine::fuzzyScore("ad", "badminton");
    // 词边界匹配应该有额外加分
    QVERIFY(atBoundary > midWord);
}

void TestSearchEngine::testContainsMatch()
{
    QVERIFY(SearchEngine::containsMatch("term", "terminal"));
    QVERIFY(!SearchEngine::containsMatch("xyz", "terminal"));
}

void TestSearchEngine::testContainsMatchCaseInsensitive()
{
    QVERIFY(SearchEngine::containsMatch("TERM", "terminal"));
    QVERIFY(SearchEngine::containsMatch("term", "TERMINAL"));
}

QTEST_MAIN(TestSearchEngine)
#include "test_searchengine.moc"
