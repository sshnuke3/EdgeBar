#include "ClipboardWidget.h"

#include <QVBoxLayout>
#include <QStandardItem>
#include <QDebug>
#include <QDateTime>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>

// ---- 自定义委托：绘制剪贴板条目 ----
class ClipItemDelegate : public QStyledItemDelegate
{
public:
    explicit ClipItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

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

        // 预览文本
        QString preview = index.data(Qt::DisplayRole).toString();
        bool pinned = index.data(Qt::UserRole + 1).toBool();

        QColor textColor;
        if (option.state & QStyle::State_Selected)
            textColor = option.palette.color(QPalette::HighlightedText);
        else
            textColor = option.palette.color(QPalette::Text);

        painter->setPen(textColor);
        QFont font = QApplication::font();
        font.setPointSizeF(font.pointSizeF() * 0.9);
        painter->setFont(font);

        QFontMetrics fm(font);
        QString elided = fm.elidedText(preview, Qt::ElideRight, rect.width() - 24);

        painter->drawText(QRect(rect.left() + 12, rect.top() + 6,
                                rect.width() - 24, rect.height() - 12),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                          elided);

        // 时间戳
        qint64 ts = index.data(Qt::UserRole + 2).toLongLong();
        if (ts > 0) {
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

        // 置顶标记
        if (pinned) {
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

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(250, 48);
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
    m_listView->setUniformItemSizes(true);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_listView, 1);

    connect(m_searchEdit, &DLineEdit::textChanged,
            this, &ClipboardWidget::onSearchChanged);
    connect(m_listView, &DListView::clicked,
            this, &ClipboardWidget::onItemClicked);

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
        stdItem->setText(item.preview);
        stdItem->setData(item.pinned, Qt::UserRole + 1);
        stdItem->setData(item.timestamp, Qt::UserRole + 2);
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
