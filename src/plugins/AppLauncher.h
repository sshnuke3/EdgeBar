#ifndef APPLAUNCHER_H
#define APPLAUNCHER_H

#include "ISearchPlugin.h"
#include <QList>

struct AppEntry {
    QString name;        // 应用名称
    QString exec;       // 启动命令
    QString icon;       // 图标名
    QString desktopPath; // .desktop 路径
    QString comment;    // 描述
};

/**
 * @brief 应用启动器插件
 *
 * 扫描 /usr/share/applications 和 ~/.local/share/applications，
 * 提供应用搜索和启动。
 */
class AppLauncher : public ISearchPlugin
{
public:
    AppLauncher();

    QString id() const override { return QStringLiteral("app-launcher"); }
    QString name() const override { return QStringLiteral("应用程序"); }
    bool shouldActivate(const QString &query) const override;
    QList<SearchResult> search(const QString &query) override;
    void activate(const SearchResult &result) override;

    /// 重新扫描应用列表
    void refresh();

private:
    QList<AppEntry> m_apps;

    void scanDesktopFiles();
    QString parseDesktopValue(const QString &line, const QString &key) const;
    QString localizedValue(const QString &name, const QString &comment) const;
};

#endif // APPLAUNCHER_H
