#include "ClipboardWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStandardItem>
#include <QDebug>
#include <QDateTime>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QMenu>
#include <QAction>

// ---- 自定义委托：绘制剪贴板条目（文本 + 图片） ----
class ClipItemDelegate : public QStyledItemDelegate
{
public:
    explicit ClipItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    enum DataRole {
        PreviewRole    = Qt::DisplayRole,
        PinnedRole     = Qt::UserRole + 1,
        TimestampRole  = Qt::UserRole + 2,
        TypeRole       = Qt::UserRole + 3,   // 0=text, 1=image
        ThumbRole      = Qt::UserRole + 4,   // QPixmap
        LabelRole      = Qt::UserRole + 5,   // "图片 1920×1080"
    };

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);

        QRect rect = option.rect;

        // 选中/悬停背景
        if (option.state & QStyle::State_Selected) {
            QPainterPath path;
            path.addRoundedRect(rect.adjusted(4, 2, -4, -2), 6, 6);
            painter->fillPath(path, option.palette.brush(QPalette::Highlight));
        } else if (option.state & QStyle::State_MouseOver) {
            QPainterPath path;
            path.addRoundedRect(rect.adjusted(4, 2, -4, -2), 6, 6);
            QColor c = option.palette.color(QPalette::Window);
            c.setAlpha(120);
            painter->fillPath(path, c);
        }

        QColor textColor;
        if (option.state & QStyle::State_Selected)
            textColor = option.palette.color(QPalette::HighlightedText);
        else
            textColor = option.palette.color(QPalette::Text);

        int type = index.data(TypeRole).toInt();

        if (type == 1) {
            // ---- 图片条目 ----
            drawImageItem(painter, option, index, rect, textColor);
        } else {
            // ---- 文本条目 ----
            drawTextItem(painter, option, index, rect, textColor);
        }

        // ---- 公共：时间戳 ----
        drawTimestamp(painter, option, index, rect, textColor);

        // ---- 公共：置顶标记 ----
        bool pinned = index.data(PinnedRole).toBool();
        if (pinned) {
            drawPinMark(painter, rect);
        }

        painter->restore();
    }

    void drawTextItem(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index, const QRect &rect,
                      const QColor &textColor) const
    {
        Q_UNUSED(option)

        QString preview = index.data(PreviewRole).toString();
        QFont font = QApplication::font();
        font.setPointSizeF(font.pointSizeF() * 0.9);
        painter->setFont(font);
        painter->setPen(textColor);

        QFontMetrics fm(font);
        QString elided = fm.elidedText(preview, Qt::ElideRight, rect.width() - 24);

        painter->drawText(QRect(rect.left() + 12, rect.top() + 6,
                                rect.width() - 24, rect.height() - 12),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          elided);
    }

    void drawImageItem(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index, const QRect &rect,
                      const QColor &textColor) const
    {
        Q_UNUSED(option)

        QPixmap thumb = index.data(ThumbRole).value<QPixmap>();
        QString label = index.data(LabelRole).toString();

        // 缩略图区域：左侧 90x54
        int thumbW = 90;
        int thumbH = 54;
        int thumbX = rect.left() + 8;
        int thumbY = rect.top() + (rect.height() - thumbH) / 2;

        // 绘制缩略图边框
        QRectF thumbRect(thumbX, thumbY, thumbW, thumbH);
        QPainterPath borderPath;
        borderPath.addRoundedRect(thumbRect, 4, 4);
        painter->setPen(QPen(QColor(0, 0, 0, 30), 1));
        painter->setBrush(QColor(255, 255, 255, 15));
        painter->drawPath(borderPath);

        // 绘制缩略图（保持比例居中）
        if (!thumb.isNull()) {
            painter->save();
            painter->setClipPath(borderPath);
            QPixmap scaledThumb = thumb.scaled(
                thumbW, thumbH, Qt::KeepAspectRatio,
                Qt::SmoothTransformation);
            int tx = thumbX + (thumbW - scaledThumb.width()) / 2;
            int ty = thumbY + (thumbH - scaledThumb.height()) / 2;
            painter->drawPixmap(tx, ty, scaledThumb);
            painter->restore();
        }

        // 标签文字：缩略图右侧
        QFont font = QApplication::font();
        font.setPointSizeF(font.pointSizeF() * 0.9);
        painter->setFont(font);
        painter->setPen(textColor);

        painter->drawText(QRect(thumbX + thumbW + 8, thumbY,
                                rect.width() - thumbW - 32, thumbH),
                          Qt::AlignLeft | Qt::AlignVCenter, label);
    }

    void drawTimestamp(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index, const QRect &rect,
                      const QColor &textColor) const
    {
        Q_UNUSED(option)

        qint64 ts = index.data(TimestampRole).toLongLong();
        if (ts <= 0) return;

        QDateTime dt = QDateTime::fromSecsSinceEpoch(ts);
        QString timeStr = dt.toString("HH:mm");

        QFont smallFont = QApplication::font();
        smallFont.setPointSizeF(smallFont.pointSizeF() * 0.7);
        painter->setFont(smallFont);

        QColor subColor = textColor;
        subColor.setAlpha(120);
        painter->setPen(subColor);
        painter->drawText(QRect(rect.right() - 50, rect.top() + 4, 40, 16),
                          Qt::AlignRight | Qt::AlignTop, timeStr);
    }

    void drawPinMark(QPainter *painter, const QRect &rect) const
    {
        QPainterPath pinPath;
        int px = rect.right() - 12;
        int py = rect.top() + 8;
        pinPath.moveTo(px, py);
        pinPath.lineTo(px + 8, py);
        pinPath.lineTo(px + 4, py + 8);
        pinPath.closeSubpath();
        QColor pinColor(245, 167, 38);
        painter->fillPath(pinPath, pinColor);
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        int type = index.data(TypeRole).toInt();
        // 图片条目需要更高行高以容纳缩略图
        return QSize(250, type == 1 ? 66 : 48);
    }
};

// ---- ClipboardWidget ----

ClipboardWidget::ClipboardWidget(ClipboardManager *manager, QWidget *parent)
    : DWidget(parent)
    , m_manager(manager)
{
    setupUI();
    onHistoryChanged();
}

void ClipboardWidget::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    // 搜索框
    m_searchEdit = new DLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索剪贴板…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedHeight(32);
    layout->addWidget(m_searchEdit);

    // 列表
    m_listView = new DListView(this);
    m_model = new QStandardItemModel(this);
    m_listView->setModel(m_model);
    m_listView->setItemDelegate(new ClipItemDelegate(m_listView));
    // 图片条目高度不同，关闭统一尺寸
    m_listView->setUniformItemSizes(false);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 右键菜单
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_listView, 1);

    connect(m_searchEdit, &DLineEdit::textChanged,
            this, &ClipboardWidget::onSearchChanged);
    connect(m_listView, &DListView::clicked,
            this, &ClipboardWidget::onItemClicked);
    connect(m_listView, &DListView::customContextMenuRequested,
            this, &ClipboardWidget::onContextMenu);

    // 连接数据变化
    connect(m_manager, &ClipboardManager::historyChanged,
            this, &ClipboardWidget::onHistoryChanged);
}

void ClipboardWidget::onHistoryChanged()
{
    QString keyword = m_searchEdit->text().trimmed();
    if (keyword.isEmpty())
        refreshList(m_manager->items());
    else
        refreshList(m_manager->filteredItems(keyword));
}

void ClipboardWidget::onSearchChanged(const QString &text)
{
    if (text.trimmed().isEmpty())
        refreshList(m_manager->items());
    else
        refreshList(m_manager->filteredItems(text.trimmed()));
}

void ClipboardWidget::refreshList(const QList<ClipboardManager::ClipItem> &items)
{
    m_currentItems = items;
    m_model->clear();

    for (const auto &item : items) {
        auto *stdItem = new QStandardItem;

        if (item.type == ClipboardManager::ImageClip) {
            // 图片条目
            stdItem->setText(item.imageLabel);
            stdItem->setData(1, ClipItemDelegate::TypeRole);
            stdItem->setData(item.thumbnail, ClipItemDelegate::ThumbRole);
            stdItem->setData(item.imageLabel, ClipItemDelegate::LabelRole);
        } else {
            // 文本条目
            stdItem->setText(item.preview);
            stdItem->setData(0, ClipItemDelegate::TypeRole);
        }

        stdItem->setData(item.pinned, ClipItemDelegate::PinnedRole);
        stdItem->setData(item.timestamp, ClipItemDelegate::TimestampRole);
        stdItem->setEditable(false);
        m_model->appendRow(stdItem);
    }
}

void ClipboardWidget::onItemClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    int row = index.row();
    if (row < 0 || row >= m_currentItems.size()) return;

    m_manager->copyToClipboard(m_currentItems[row].id);
}

void ClipboardWidget::onContextMenu(const QPoint &pos)
{
    QModelIndex index = m_listView->indexAt(pos);
    if (!index.isValid()) return;

    int row = index.row();
    if (row < 0 || row >= m_currentItems.size()) return;

    const auto &item = m_currentItems[row];

    QMenu menu(this);
    QAction *pinAct = menu.addAction(
        item.pinned ? QStringLiteral("取消置顶") : QStringLiteral("置顶"));
    menu.addSeparator();
    QAction *delAct = menu.addAction(QStringLiteral("删除"));

    QAction *ret = menu.exec(m_listView->viewport()->mapToGlobal(pos));
    if (ret == pinAct) {
        m_manager->togglePin(item.id);
    } else if (ret == delAct) {
        m_manager->remove(item.id);
    }
}
