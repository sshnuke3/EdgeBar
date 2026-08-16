#ifndef GRANDSEARCHADAPTOR_H
#define GRANDSEARCHADAPTOR_H

#include <QDBusAbstractAdaptor>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

class ClipboardManager;
class AppLauncher;
class SystemCommand;

/**
 * @brief dde-grand-search 全局搜索插件 DBus 适配器
 *
 * 实现 dde-grand-search V1.0 插件接口协议：
 *   - Search(String json) → String json
 *   - Stop(String json) → Boolean
 *   - Action(String json) → Boolean
 *
 * 搜索范围：
 *   1. 已安装应用程序（扫描 .desktop 文件）
 *   2. 系统操作（锁屏/关机/重启等）
 *   3. 剪贴板历史（EdgeBar 运行期间记录）
 *
 * 安装配置文件 edgebar-search.conf 到
 *   /usr/lib/x86_64-linux-gnu/dde-grand-search-daemon/plugins/searcher/
 * 全局搜索后端重启后即可在搜索结果中展示 EdgeBar 提供的搜索项。
 */
class GrandSearchAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.edgebar.SearchPlugin")
    Q_CLASSINFO("D-Bus Introspection",
        "<interface name='org.deepin.edgebar.SearchPlugin'>\n"
        "  <method name='Search'>\n"
        "    <arg name='json' type='s' direction='in'/>\n"
        "    <arg name='result' type='s' direction='out'/>\n"
        "  </method>\n"
        "  <method name='Stop'>\n"
        "    <arg name='json' type='s' direction='in'/>\n"
        "    <arg name='result' type='b' direction='out'/>\n"
        "  </method>\n"
        "  <method name='Action'>\n"
        "    <arg name='json' type='s' direction='in'/>\n"
        "    <arg name='result' type='b' direction='out'/>\n"
        "  </method>\n"
        "</interface>\n"
    )

public:
    GrandSearchAdaptor(QObject *parent,
                        ClipboardManager *clipboard,
                        AppLauncher *appLauncher,
                        SystemCommand *systemCommand);

public slots:
    /// 执行搜索，返回 JSON 格式的搜索结果
    QString Search(const QString &json);

    /// 中断搜索任务
    bool Stop(const QString &json);

    /// 对搜索结果执行操作（如打开应用、执行命令、复制剪贴板）
    bool Action(const QString &json);

private:
    ClipboardManager *m_clipboard;
    AppLauncher *m_appLauncher;
    SystemCommand *m_systemCommand;

    /// 构建搜索结果 JSON
    QJsonObject buildResult(const QString &mId, const QString &keyword);

    /// 搜索应用程序
    QJsonArray searchApps(const QString &keyword);

    /// 搜索系统命令
    QJsonArray searchCommands(const QString &keyword);

    /// 搜索剪贴板历史
    QJsonArray searchClipboard(const QString &keyword);
};

#endif // GRANDSEARCHADAPTOR_H
