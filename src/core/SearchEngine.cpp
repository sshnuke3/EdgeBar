#include "SearchEngine.h"

#include <QString>

// ---------------------------------------------------------------------------
// fuzzyScore: 子序列模糊匹配评分
// ---------------------------------------------------------------------------
//
// 算法说明：
//   遍历 target 字符串，按顺序查找 query 中的每个字符是否出现。
//   若 query 的所有字符都能在 target 中按顺序找到（子序列匹配），
//   则根据匹配质量计算评分；否则返回 0。
//
// 评分规则：
//   - 基础分 = 匹配字符数（每个匹配的字符 +1）
//   - 连续匹配加分：与前一个匹配位置相邻时 +5
//   - 首字母匹配加分：query 的第一个字符匹配 target 的第一个字符时 +10
//   - 词边界匹配加分：匹配位置在字符串开头或非字母数字字符之后时 +8
//   - 完全匹配高分：query 与 target 完全相等时 +100（另加字符数）
//
// ---------------------------------------------------------------------------

int SearchEngine::fuzzyScore(const QString &query, const QString &target)
{
    // 空查询或空目标均视为不匹配
    if (query.isEmpty() || target.isEmpty()) {
        return 0;
    }

    // 统一转为小写后比较
    QString q = query.toLower();
    QString t = target.toLower();

    // 完全匹配：最高分
    if (q == t) {
        return 100 + q.length();
    }

    int score = 0;
    int qi = 0;                    // query 当前匹配位置
    int matchedCount = 0;          // 已匹配的字符数
    int prevMatchTargetIndex = -1; // 上一次匹配在 target 中的索引

    for (int ti = 0; ti < t.length() && qi < q.length(); ++ti) {
        if (t[ti] == q[qi]) {
            matchedCount++;

            // 连续匹配加分：本次匹配紧接上一次匹配之后
            if (prevMatchTargetIndex == ti - 1) {
                score += 5;
            }

            // 首字母匹配加分：query 的第一个字符匹配 target 的第一个字符
            if (qi == 0 && ti == 0) {
                score += 10;
            }

            // 词边界匹配加分：匹配位置在字符串开头，或前一个字符为非字母数字
            // （即处于一个词的起始边界：空格、连字符、下划线等之后）
            if (ti == 0 || !t[ti - 1].isLetterOrNumber()) {
                score += 8;
            }

            prevMatchTargetIndex = ti;
            qi++;
        }
    }

    // 未完全匹配 query 的所有字符 —— 不匹配
    if (qi < q.length()) {
        return 0;
    }

    // 加上基础分（匹配字符数）
    score += matchedCount;

    return score;
}

// ---------------------------------------------------------------------------
// containsMatch: 简单包含匹配（不区分大小写）
// ---------------------------------------------------------------------------

bool SearchEngine::containsMatch(const QString &query, const QString &target)
{
    return target.toLower().contains(query.toLower());
}
