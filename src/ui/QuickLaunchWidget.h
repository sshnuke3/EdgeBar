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

private:
    void setupUI();
    void performSearch(const QString &query);

    DLineEdit          *m_searchEdit = nullptr;
    DListView          *m_resultList = nullptr;
    QStandardItemModel *m_model = nullptr;
    QList<ISearchPlugin*> m_plugins;
    QList<SearchResult>  m_currentResults;
};

#endif // QUICKLAUNCHWIDGET_H
