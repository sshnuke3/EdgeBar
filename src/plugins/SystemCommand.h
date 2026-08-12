#ifndef SYSTEMCOMMAND_H
#define SYSTEMCOMMAND_H

#include "ISearchPlugin.h"
#include <QList>

/**
 * @brief 系统命令插件
 *
 * 提供系统操作：锁屏、休眠、重启、关机、注销等。
 */
class SystemCommand : public ISearchPlugin
{
public:
    SystemCommand();

    QString id() const override { return QStringLiteral("system-command"); }
    QString name() const override { return QStringLiteral("系统操作"); }
    bool shouldActivate(const QString &query) const override;
    QList<SearchResult> search(const QString &query) override;
    void activate(const SearchResult &result) override;

private:
    struct Command {
        QString id;
        QString title;
        QString subtitle;
        QString iconName;
        QString exec;
    };
    QList<Command> m_commands;

    void initCommands();
};

#endif // SYSTEMCOMMAND_H
