#ifndef CLIPBOARDWIDGET_H
#define CLIPBOARDWIDGET_H

#include <DWidget>
#include <DListView>
#include <DLineEdit>
#include <DPushButton>
#include <DToolButton>

#include "core/ClipboardManager.h"

DWIDGET_USE_NAMESPACE

/**
 * @brief 剪贴板历史界面
 *
 * 搜索框 + 历史列表
 * 文本条目：显示预览文本 + 时间
 * 图片条目：显示缩略图 + 尺寸标签 + 时间
 * 点击复制到剪贴板
 * 支持置顶/删除/收藏（右键菜单）
 * 支持紧凑/标准模式切换 + 平滑滚动
 */
class ClipboardWidget : public DWidget
{
    Q_OBJECT
public:
    explicit ClipboardWidget(ClipboardManager *manager, QWidget *parent = nullptr);

    /// 设置紧凑模式
    void setCompactMode(bool compact);
    bool compactMode() const { return m_compactMode; }

private slots:
    void onHistoryChanged();
    void onSearchChanged(const QString &text);
    void onItemClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void onCompactToggled();

private:
    void setupUI();
    void refreshList(const QList<ClipboardManager::ClipItem> &items);

    ClipboardManager *m_manager;
    DLineEdit         *m_searchEdit = nullptr;
    DListView         *m_listView = nullptr;
    DToolButton       *m_compactBtn = nullptr;
    class QStandardItemModel *m_model = nullptr;
    class ClipItemDelegate *m_delegate = nullptr;
    QList<ClipboardManager::ClipItem> m_currentItems;

    bool m_compactMode = false;
};

#endif // CLIPBOARDWIDGET_H
