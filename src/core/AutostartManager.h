#ifndef EDGE_AUTOSTART_MANAGER_H
#define EDGE_AUTOSTART_MANAGER_H

#include <QObject>

/**
 * @brief 开机自启动管理器
 *
 * 通过在 ~/.config/autostart/ 目录下创建/删除 .desktop 文件
 * 实现 EdgeBar 的开机自启动管理。
 *
 * 使用 DStandardPaths (dtkcore) 获取标准路径。
 */
class AutostartManager : public QObject
{
    Q_OBJECT
public:
    explicit AutostartManager(QObject *parent = nullptr);

    /// 检查是否已启用自启动
    bool isEnabled() const;

    /// 启用开机自启动
    bool enable();

    /// 禁用开机自启动
    bool disable();

private:
    /// 获取 autostart 目录路径
    QString autostartDir() const;

    /// 获取 autostart .desktop 文件路径
    QString desktopFilePath() const;
};

#endif // EDGE_AUTOSTART_MANAGER_H
