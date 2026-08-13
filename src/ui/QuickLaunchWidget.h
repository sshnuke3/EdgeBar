#ifndef QUICKLAUNCHWIDGET_H
#define QUICKLAUNCHWIDGET_H

#include <DWidget>
#include <DLineEdit>
#include <DListView>

#include "plugins/ISearchPlugin.h"

DWIDGET_USE_NAMESPACE

/**
 * @brief 快速启动界面
 *
 * 搜索框 + 结果列表，复用 GlobalLauncher 的插件架构
 * 右键空白区域弹出系统快捷操作菜单（锁屏/注销/挂起/亮度）
 */
class QuickLaunchWidget : public DWidget
{
    Q_OBJECT
public:
    explicit QuickLaunchWidget(QWidget *parent = nullptr);

    void registerPlugin(ISearchPlugin *plugin);

private slots:
    void onSearchChanged(const QString &text);
    void onItemClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);

private:
    void setupUI();
    void performSearch(const QString &query);
    void executeSystemAction(const QString &action);

    DLineEdit          *m_searchEdit = nullptr;
    DListView          *m_resultList = nullptr;
    QStandardItemModel *m_model = nullptr;
    QList<ISearchPlugin*> m_plugins;
    QList<SearchResult>  m_currentResults;
};

#endif // QUICKLAUNCHWIDGET_H
