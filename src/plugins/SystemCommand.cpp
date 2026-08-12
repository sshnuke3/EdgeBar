#include "SystemCommand.h"
#include "ISearchPlugin.h"
#include "core/SearchEngine.h"

#include <QProcess>
#include <QDebug>
#include <algorithm>

// ---------------------------------------------------------------------------
// 构造函数：初始化系统命令列表
// ---------------------------------------------------------------------------

SystemCommand::SystemCommand()
{
    initCommands();
}

// ---------------------------------------------------------------------------
// initCommands: 初始化内置系统命令
// ---------------------------------------------------------------------------
//
// 命令列表：
//   1. lock      - 锁屏          loginctl lock-session
//   2. logout    - 注销          qdbus org.deepin.SessionManager ... Logout
//   3. suspend   - 待机          systemctl suspend
//   4. hibernate - 休眠          systemctl hibernate
//   5. reboot    - 重启          systemctl reboot
//   6. shutdown  - 关机          systemctl poweroff
//   7. settings  - 控制中心      dde-control-center
//   8. terminal  - 终端          deepin-terminal
//   9. files     - 文件管理器    dde-file-manager
//
// ---------------------------------------------------------------------------

void SystemCommand::initCommands()
{
    // 锁屏
    {
        Command cmd;
        cmd.id = QStringLiteral("lock");
        cmd.title = QStringLiteral("锁屏");
        cmd.subtitle = QStringLiteral("锁定当前会话");
        cmd.iconName = QStringLiteral("system-lock-screen");
        cmd.exec = QStringLiteral("loginctl lock-session");
        m_commands.append(cmd);
    }

    // 注销
    {
        Command cmd;
        cmd.id = QStringLiteral("logout");
        cmd.title = QStringLiteral("注销");
        cmd.subtitle = QStringLiteral("注销当前用户");
        cmd.iconName = QStringLiteral("system-log-out");
        cmd.exec = QStringLiteral("qdbus org.deepin.SessionManager /org/deepin/SessionManager Logout");
        m_commands.append(cmd);
    }

    // 待机
    {
        Command cmd;
        cmd.id = QStringLiteral("suspend");
        cmd.title = QStringLiteral("待机");
        cmd.subtitle = QStringLiteral("系统挂起到内存");
        cmd.iconName = QStringLiteral("system-suspend");
        cmd.exec = QStringLiteral("systemctl suspend");
        m_commands.append(cmd);
    }

    // 休眠
    {
        Command cmd;
        cmd.id = QStringLiteral("hibernate");
        cmd.title = QStringLiteral("休眠");
        cmd.subtitle = QStringLiteral("系统休眠到磁盘");
        cmd.iconName = QStringLiteral("system-hibernate");
        cmd.exec = QStringLiteral("systemctl hibernate");
        m_commands.append(cmd);
    }

    // 重启
    {
        Command cmd;
        cmd.id = QStringLiteral("reboot");
        cmd.title = QStringLiteral("重启");
        cmd.subtitle = QStringLiteral("重启系统");
        cmd.iconName = QStringLiteral("system-reboot");
        cmd.exec = QStringLiteral("systemctl reboot");
        m_commands.append(cmd);
    }

    // 关机
    {
        Command cmd;
        cmd.id = QStringLiteral("shutdown");
        cmd.title = QStringLiteral("关机");
        cmd.subtitle = QStringLiteral("关闭系统");
        cmd.iconName = QStringLiteral("system-shutdown");
        cmd.exec = QStringLiteral("systemctl poweroff");
        m_commands.append(cmd);
    }

    // 控制中心
    {
        Command cmd;
        cmd.id = QStringLiteral("settings");
        cmd.title = QStringLiteral("控制中心");
        cmd.subtitle = QStringLiteral("打开 deepin 控制中心");
        cmd.iconName = QStringLiteral("preferences-system");
        cmd.exec = QStringLiteral("dde-control-center");
        m_commands.append(cmd);
    }

    // 终端
    {
        Command cmd;
        cmd.id = QStringLiteral("terminal");
        cmd.title = QStringLiteral("终端");
        cmd.subtitle = QStringLiteral("打开 deepin 终端");
        cmd.iconName = QStringLiteral("terminal");
        cmd.exec = QStringLiteral("deepin-terminal");
        m_commands.append(cmd);
    }

    // 文件管理器
    {
        Command cmd;
        cmd.id = QStringLiteral("files");
        cmd.title = QStringLiteral("文件管理器");
        cmd.subtitle = QStringLiteral("打开 deepin 文件管理器");
        cmd.iconName = QStringLiteral("folder");
        cmd.exec = QStringLiteral("dde-file-manager");
        m_commands.append(cmd);
    }
}

// ---------------------------------------------------------------------------
// shouldActivate: 查询长度 >= 1 时激活
// ---------------------------------------------------------------------------

bool SystemCommand::shouldActivate(const QString &query) const
{
    return query.length() >= 1;
}

// ---------------------------------------------------------------------------
// search: 模糊匹配命令标题，返回 score > 0 的结果
// ---------------------------------------------------------------------------
//
// 使用 SearchEngine::fuzzyScore 对每个命令的 title 进行匹配，
// 只返回评分 > 0 的结果，并按评分降序排序。
//
// ---------------------------------------------------------------------------

QList<SearchResult> SystemCommand::search(const QString &query)
{
    QList<SearchResult> results;

    for (const Command &cmd : m_commands) {
        int score = SearchEngine::fuzzyScore(query, cmd.title);
        if (score > 0) {
            SearchResult result;
            result.id = cmd.id;
            result.title = cmd.title;
            result.subtitle = cmd.subtitle;
            result.iconName = cmd.iconName;
            result.pluginId = id();
            result.data = cmd.exec;
            result.score = score;
            results.append(result);
        }
    }

    // 按评分降序排序
    std::sort(results.begin(), results.end(),
              [](const SearchResult &a, const SearchResult &b) {
                  return a.score > b.score;
              });

    return results;
}

// ---------------------------------------------------------------------------
// activate: 执行系统命令
// ---------------------------------------------------------------------------
//
// exec 可能包含空格分隔的参数（如 "qdbus org.deepin.SessionManager ..."），
// 使用 split 拆分为程序名和参数列表后传给 QProcess::startDetached。
//
// ---------------------------------------------------------------------------

void SystemCommand::activate(const SearchResult &result)
{
    QString exec = result.data.toString();
    if (exec.isEmpty()) {
        return;
    }

    // 将 exec 按空格拆分为程序名和参数
    QStringList parts = exec.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return;
    }

    QString program = parts.takeFirst();
    qDebug() << "[SystemCommand] executing:" << program << parts;
    QProcess::startDetached(program, parts);
}
