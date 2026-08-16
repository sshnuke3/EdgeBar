#include "GrandSearchAdaptor.h"
#include "core/ClipboardManager.h"
#include "core/SearchEngine.h"
#include "core/Logging.h"
#include "plugins/AppLauncher.h"
#include "plugins/SystemCommand.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QProcess>
#include <QFileInfo>

GrandSearchAdaptor::GrandSearchAdaptor(QObject *parent,
                                       ClipboardManager *clipboard,
                                       AppLauncher *appLauncher,
                                       SystemCommand *systemCommand)
    : QDBusAbstractAdaptor(parent)
    , m_clipboard(clipboard)
    , m_appLauncher(appLauncher)
    , m_systemCommand(systemCommand)
{
    setAutoRelaySignals(false);
}

// ---------------------------------------------------------------------------
// Search: 解析输入 JSON，执行搜索，返回结果 JSON
// ---------------------------------------------------------------------------
//
// 输入格式（V1.0 协议）：
//   {"ver": "1.0", "mID": "...", "cont": "search keyword"}
//
// 输出格式：
//   {"ver": "1.0", "mID": "...", "cont": [
//     {"group": "...", "items": [
//       {"item": "id", "name": "名称", "icon": "icon", "type": "type"}
//     ]}
//   ]}
//
// 搜索范围：
//   1. 应用程序 — 扫描 /usr/share/applications 下的 .desktop 文件
//   2. 系统操作 — 锁屏/关机/重启等内置命令
//   3. 剪贴板历史 — EdgeBar 运行期间记录的剪贴板条目
//
// ---------------------------------------------------------------------------

QString GrandSearchAdaptor::Search(const QString &json)
{
    // 解析输入
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        qCWarning(edgebarLog) << "Search: invalid JSON input";
        return QStringLiteral("{\"ver\":\"1.0\",\"mID\":\"\",\"cont\":[]}");
    }

    QJsonObject input = doc.object();
    QString mId = input.value(QStringLiteral("mID")).toString();
    QString keyword = input.value(QStringLiteral("cont")).toString();

    qCDebug(edgebarLog) << "GrandSearch: searching for" << keyword;

    QJsonObject result = buildResult(mId, keyword);
    return QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Compact));
}

// ---------------------------------------------------------------------------
// Stop: 中断搜索（搜索是同步的，直接返回 true）
// ---------------------------------------------------------------------------

bool GrandSearchAdaptor::Stop(const QString &json)
{
    Q_UNUSED(json);
    return true;
}

// ---------------------------------------------------------------------------
// Action: 对搜索结果执行操作
// ---------------------------------------------------------------------------
//
// 输入格式：
//   {"ver": "1.0", "action": "openitem", "item": "app:firefox.desktop"}
//
// item ID 格式：
//   app:<desktop-filename>  → 启动应用
//   cmd:<command-id>         → 执行系统命令
//   clip:<item-id>           → 复制剪贴板条目
//
// ---------------------------------------------------------------------------

bool GrandSearchAdaptor::Action(const QString &json)
{
    QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isObject()) {
        qCWarning(edgebarLog) << "Action: invalid JSON input";
        return false;
    }

    QJsonObject input = doc.object();
    QString action = input.value(QStringLiteral("action")).toString();
    QString itemId = input.value(QStringLiteral("item")).toString();

    if (action != QStringLiteral("openitem")) {
        qCWarning(edgebarLog) << "Action: unsupported action" << action;
        return false;
    }

    qCInfo(edgebarLog) << "GrandSearch Action: item =" << itemId;

    // 根据 item ID 前缀分发
    if (itemId.startsWith(QStringLiteral("app:"))) {
        // 启动应用：item ID 格式 "app:<desktop-filename>"
        QString desktopFile = itemId.mid(4);

        // 通过 AppLauncher 查找并启动
        if (m_appLauncher) {
            auto results = m_appLauncher->search(desktopFile.left(desktopFile.indexOf('.')));
            if (!results.isEmpty()) {
                m_appLauncher->activate(results.first());
                return true;
            }
        }

        // 直接通过 .desktop 文件启动
        QString exec = QStringLiteral("xdg-open /usr/share/applications/%1").arg(desktopFile);
        return QProcess::startDetached(exec);
    }

    if (itemId.startsWith(QStringLiteral("cmd:"))) {
        // 执行系统命令
        QString cmdId = itemId.mid(4);
        if (m_systemCommand) {
            auto results = m_systemCommand->search(cmdId);
            if (!results.isEmpty()) {
                m_systemCommand->activate(results.first());
                return true;
            }
        }
        return false;
    }

    if (itemId.startsWith(QStringLiteral("clip:"))) {
        // 复制剪贴板条目
        bool ok = false;
        int clipId = itemId.mid(5).toInt(&ok);
        if (ok && m_clipboard) {
            m_clipboard->copyToClipboard(clipId);
            qCInfo(edgebarLog) << "Clipboard item copied:" << clipId;
            return true;
        }
        return false;
    }

    qCWarning(edgebarLog) << "Action: unknown item format" << itemId;
    return false;
}

// ---------------------------------------------------------------------------
// buildResult: 构建搜索结果 JSON
// ---------------------------------------------------------------------------

QJsonObject GrandSearchAdaptor::buildResult(const QString &mId, const QString &keyword)
{
    QJsonObject result;
    result.insert(QStringLiteral("ver"), QStringLiteral("1.0"));
    result.insert(QStringLiteral("mID"), mId);

    QJsonArray groups;

    // 1. 搜索应用程序
    QJsonArray appItems = searchApps(keyword);
    if (!appItems.isEmpty()) {
        QJsonObject appGroup;
        appGroup.insert(QStringLiteral("group"), QStringLiteral("应用程序"));
        appGroup.insert(QStringLiteral("items"), appItems);
        groups.append(appGroup);
    }

    // 2. 搜索系统操作
    QJsonArray cmdItems = searchCommands(keyword);
    if (!cmdItems.isEmpty()) {
        QJsonObject cmdGroup;
        cmdGroup.insert(QStringLiteral("group"), QStringLiteral("系统操作"));
        cmdGroup.insert(QStringLiteral("items"), cmdItems);
        groups.append(cmdGroup);
    }

    // 3. 搜索剪贴板历史
    QJsonArray clipItems = searchClipboard(keyword);
    if (!clipItems.isEmpty()) {
        QJsonObject clipGroup;
        clipGroup.insert(QStringLiteral("group"), QStringLiteral("剪贴板历史"));
        clipGroup.insert(QStringLiteral("items"), clipItems);
        groups.append(clipGroup);
    }

    result.insert(QStringLiteral("cont"), groups);
    return result;
}

// ---------------------------------------------------------------------------
// searchApps: 搜索已安装应用程序
// ---------------------------------------------------------------------------

QJsonArray GrandSearchAdaptor::searchApps(const QString &keyword)
{
    QJsonArray items;

    if (!m_appLauncher || keyword.isEmpty()) return items;

    auto results = m_appLauncher->search(keyword);
    for (const auto &r : results) {
        QJsonObject item;
        // item ID 格式: app:<desktop-filename>
        // 从 desktopPath 提取文件名
        QString fileName = QFileInfo(r.id).fileName();
        item.insert(QStringLiteral("item"), QStringLiteral("app:") + fileName);
        item.insert(QStringLiteral("name"), r.title);
        item.insert(QStringLiteral("icon"), r.iconName);
        item.insert(QStringLiteral("type"), QStringLiteral("application/x-desktop"));
        items.append(item);
    }

    return items;
}

// ---------------------------------------------------------------------------
// searchCommands: 搜索系统命令
// ---------------------------------------------------------------------------

QJsonArray GrandSearchAdaptor::searchCommands(const QString &keyword)
{
    QJsonArray items;

    if (!m_systemCommand || keyword.isEmpty()) return items;

    auto results = m_systemCommand->search(keyword);
    for (const auto &r : results) {
        QJsonObject item;
        item.insert(QStringLiteral("item"), QStringLiteral("cmd:") + r.id);
        item.insert(QStringLiteral("name"), r.title);
        item.insert(QStringLiteral("icon"), r.iconName);
        item.insert(QStringLiteral("type"), QStringLiteral("application/x-edgebar-command"));
        items.append(item);
    }

    return items;
}

// ---------------------------------------------------------------------------
// searchClipboard: 搜索剪贴板历史
// ---------------------------------------------------------------------------

QJsonArray GrandSearchAdaptor::searchClipboard(const QString &keyword)
{
    QJsonArray items;

    if (!m_clipboard || keyword.isEmpty()) return items;

    auto clipItems = m_clipboard->filteredItems(keyword);

    int count = 0;
    for (const auto &clip : clipItems) {
        if (clip.type != ClipboardManager::TextClip) continue; // 只搜索文本
        if (count >= 5) break;  // 最多返回 5 条

        QJsonObject item;
        item.insert(QStringLiteral("item"), QStringLiteral("clip:") + QString::number(clip.id));

        // 截断显示
        QString preview = clip.text;
        if (preview.length() > 40) {
            preview = preview.left(40) + QStringLiteral("...");
        }
        item.insert(QStringLiteral("name"), preview);
        item.insert(QStringLiteral("icon"), QStringLiteral("edit-paste"));
        item.insert(QStringLiteral("type"), QStringLiteral("text/plain"));

        items.append(item);
        count++;
    }

    return items;
}
