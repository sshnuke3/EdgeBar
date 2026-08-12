#include "AppLauncher.h"
#include "ISearchPlugin.h"
#include "core/SearchEngine.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QSet>
#include <QDebug>
#include <algorithm>

// ---------------------------------------------------------------------------
// 构造函数：扫描 .desktop 文件构建应用列表
// ---------------------------------------------------------------------------

AppLauncher::AppLauncher()
{
    refresh();
}

// ---------------------------------------------------------------------------
// parseDesktopValue: 从 .desktop 文件行中提取 "key=" 之后的值
// ---------------------------------------------------------------------------

QString AppLauncher::parseDesktopValue(const QString &line, const QString &key) const
{
    QString prefix = key + QLatin1Char('=');
    if (line.startsWith(prefix)) {
        return line.mid(prefix.length());
    }
    return QString();
}

// ---------------------------------------------------------------------------
// localizedValue: 返回本地化显示名称（当前直接返回 name，预留扩展）
// ---------------------------------------------------------------------------

QString AppLauncher::localizedValue(const QString &name, const QString &comment) const
{
    Q_UNUSED(comment)
    return name;
}

// ---------------------------------------------------------------------------
// scanDesktopFiles: 扫描标准目录下的 .desktop 文件
// ---------------------------------------------------------------------------
//
// 扫描路径：
//   1. /usr/share/applications        （系统级应用）
//   2. ~/.local/share/applications    （用户级应用）
//
// 解析规则：
//   - 解析 Name=, Exec=, Icon=, Comment= 行（取等号后的值）
//   - 跳过 NoDisplay=true 的条目
//   - 去掉 Exec 中的 %f %F %u %U 等字段代码
//   - 按 name 去重
//
// ---------------------------------------------------------------------------

void AppLauncher::scanDesktopFiles()
{
    m_apps.clear();

    QStringList searchPaths;
    searchPaths << QStringLiteral("/usr/share/applications");
    searchPaths << QDir::homePath() + QStringLiteral("/.local/share/applications");

    QSet<QString> seenNames;

    for (const QString &path : searchPaths) {
        QDir dir(path);
        if (!dir.exists()) {
            continue;
        }

        dir.setNameFilters(QStringList() << QStringLiteral("*.desktop"));
        const QStringList files = dir.entryList(QDir::Files);

        for (const QString &fileName : files) {
            QString filePath = dir.absoluteFilePath(fileName);
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }

            AppEntry entry;
            entry.desktopPath = filePath;
            bool noDisplay = false;

            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine().trimmed();

                // 跳过空行、注释和节头（如 [Desktop Entry]）
                if (line.isEmpty()
                    || line.startsWith(QLatin1Char('#'))
                    || line.startsWith(QLatin1Char('['))) {
                    continue;
                }

                // 检查 NoDisplay
                if (line.startsWith(QStringLiteral("NoDisplay="))) {
                    QString value = line.mid(QStringLiteral("NoDisplay=").length()).toLower();
                    if (value == QStringLiteral("true") || value == QStringLiteral("1")) {
                        noDisplay = true;
                    }
                    continue;
                }

                // 解析 Name（仅取第一个未本地化的 Name=）
                QString nameVal = parseDesktopValue(line, QStringLiteral("Name"));
                if (!nameVal.isNull() && entry.name.isEmpty()) {
                    entry.name = nameVal;
                    continue;
                }

                // 解析 Exec
                QString execVal = parseDesktopValue(line, QStringLiteral("Exec"));
                if (!execVal.isNull()) {
                    entry.exec = execVal;
                    continue;
                }

                // 解析 Icon
                QString iconVal = parseDesktopValue(line, QStringLiteral("Icon"));
                if (!iconVal.isNull()) {
                    entry.icon = iconVal;
                    continue;
                }

                // 解析 Comment
                QString commentVal = parseDesktopValue(line, QStringLiteral("Comment"));
                if (!commentVal.isNull() && entry.comment.isEmpty()) {
                    entry.comment = commentVal;
                    continue;
                }
            }

            file.close();

            // 跳过隐藏条目
            if (noDisplay) {
                continue;
            }

            // 跳过缺少名称或启动命令的条目
            if (entry.name.isEmpty() || entry.exec.isEmpty()) {
                continue;
            }

            // 去掉 Exec 中的字段代码（%f %F %u %U 等）
            // freedesktop 规范定义的字段代码均以 % 开头后跟一个字母
            QString cleanExec;
            int i = 0;
            while (i < entry.exec.size()) {
                if (entry.exec.at(i) == QLatin1Char('%')
                    && i + 1 < entry.exec.size()
                    && entry.exec.at(i + 1).isLetter()) {
                    i += 2;  // 跳过字段代码
                } else {
                    cleanExec.append(entry.exec.at(i));
                    ++i;
                }
            }
            entry.exec = cleanExec.simplified();

            // 按 name 去重
            if (seenNames.contains(entry.name)) {
                continue;
            }
            seenNames.insert(entry.name);

            m_apps.append(entry);
        }
    }
}

// ---------------------------------------------------------------------------
// refresh: 重新扫描应用列表
// ---------------------------------------------------------------------------

void AppLauncher::refresh()
{
    scanDesktopFiles();
    qDebug() << "[AppLauncher] scanned" << m_apps.size() << "applications";
}

// ---------------------------------------------------------------------------
// shouldActivate: 查询长度 >= 1 时激活
// ---------------------------------------------------------------------------

bool AppLauncher::shouldActivate(const QString &query) const
{
    return query.length() >= 1;
}

// ---------------------------------------------------------------------------
// search: 模糊匹配应用名称和描述，返回 top 8
// ---------------------------------------------------------------------------
//
// 对每个应用分别用 fuzzyScore 匹配 name 和 comment，
// 取较高分作为最终评分，按评分降序返回前 8 条结果。
//
// ---------------------------------------------------------------------------

QList<SearchResult> AppLauncher::search(const QString &query)
{
    QList<SearchResult> results;

    for (const AppEntry &app : m_apps) {
        int nameScore = SearchEngine::fuzzyScore(query, app.name);
        int commentScore = SearchEngine::fuzzyScore(query, app.comment);
        int score = qMax(nameScore, commentScore);

        if (score > 0) {
            SearchResult result;
            result.id = app.desktopPath;
            result.title = app.name;
            result.subtitle = app.comment;
            result.iconName = app.icon;
            result.pluginId = id();
            result.data = app.exec;
            result.score = score;
            results.append(result);
        }
    }

    // 按评分降序排序
    std::sort(results.begin(), results.end(),
              [](const SearchResult &a, const SearchResult &b) {
                  return a.score > b.score;
              });

    // 返回前 8 条
    if (results.size() > 8) {
        results = results.mid(0, 8);
    }

    return results;
}

// ---------------------------------------------------------------------------
// activate: 启动选中的应用
// ---------------------------------------------------------------------------

void AppLauncher::activate(const SearchResult &result)
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
    qDebug() << "[AppLauncher] launching:" << program << parts;
    QProcess::startDetached(program, parts);
}
