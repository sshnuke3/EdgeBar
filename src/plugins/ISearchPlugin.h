#ifndef ISEARCHPLUGIN_H
#define ISEARCHPLUGIN_H

#include <QString>
#include <QList>
#include <QVariant>

/**
 * @brief 搜索结果项
 */
struct SearchResult
{
    QString  id;           // 插件内唯一标识
    QString  title;        // 主标题
    QString  subtitle;     // 副标题
    QString  iconName;     // 主题图标名或文件路径
    QString  pluginId;     // 来源插件 ID
    QVariant data;         // 插件自定义数据
    int      score = 0;   // 相关度（越高越靠前）
};

/**
 * @brief 搜索插件接口
 *
 * 所有内置/第三方插件实现此接口，注册到 MainWindow。
 * 用户输入查询时，MainWindow 遍历所有插件调用 search()。
 */
class ISearchPlugin
{
public:
    virtual ~ISearchPlugin() = default;

    /// 插件唯一标识
    virtual QString id() const = 0;

    /// 插件显示名称
    virtual QString name() const = 0;

    /// 是否应对当前查询生效（快速判断）
    virtual bool shouldActivate(const QString &query) const = 0;

    /// 执行搜索，返回结果列表
    virtual QList<SearchResult> search(const QString &query) = 0;

    /// 用户激活某条结果
    virtual void activate(const SearchResult &result) = 0;
};

#endif // ISEARCHPLUGIN_H
