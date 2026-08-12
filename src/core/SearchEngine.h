#ifndef SEARCHENGINE_H
#define SEARCHENGINE_H

#include <QString>

/**
 * @brief 模糊搜索引擎
 *
 * 提供静态方法进行模糊匹配和评分。
 * 算法：子序列匹配 + 连续匹配加分 + 首字母加分
 */
class SearchEngine
{
public:
    /**
     * @brief 模糊匹配评分
     * @param query 查询字符串（已转为小写）
     * @param target 目标字符串（已转为小写）
     * @return 评分，0 表示不匹配，越高越匹配
     */
    static int fuzzyScore(const QString &query, const QString &target);

    /**
     * @brief 简单包含匹配（不区分大小写）
     */
    static bool containsMatch(const QString &query, const QString &target);
};

#endif // SEARCHENGINE_H
