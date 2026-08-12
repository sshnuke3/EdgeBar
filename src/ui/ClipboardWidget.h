#ifndef CLIPBOARDWIDGET_H
#define CLIPBOARDWIDGET_H

#include <DWidget>
#include <DListView>
#include <DLineEdit>
#include <DPushButton>

#include "core/ClipboardManager.h"

DWIDGET_USE_NAMESPACE

/**
 * @brief 剪贴板历史界面
 *
 * 搜索框 + 历史列表
 * 每条显示预览文本 + 时间
 * 点击复制到剪贴板
 * 支持置顶/删除
 */
class ClipboardWidget : public DWidget
{
    Q_OBJECT
public:
    explicit ClipboardWidget(ClipboardManager *manager, QWidget *parent = nullptr);

private slots:
    void onHistoryChanged();
    void onSearchChanged(const QString &text);
    void onItemClicked(const QModelIndex &index);

private:
    void setupUI();
    void refreshList(const QList<ClipboardManager::ClipItem> &items);

    ClipboardManager *m_manager;
    DLineEdit         *m_searchEdit = nullptr;
    DListView         *m_listView = nullptr;
    class QStandardItemModel *m_model = nullptr;
    QList<ClipboardManager::ClipItem> m_currentItems;
};

#endif // CLIPBOARDWIDGET_H
