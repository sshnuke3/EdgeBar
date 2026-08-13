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
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QGuiApplication>

// ---------------------------------------------------------------------------
// SmoothScrollListView: 平滑滚动列表
// ---------------------------------------------------------------------------
//
// 重写 wheelEvent，用 QPropertyAnimation 动画化 scrollbar 的滑动，
// 替代默认的"瞬移"行为，减轻视觉疲劳。
//
// ---------------------------------------------------------------------------

class SmoothScrollListView : public DListView
{
public:
    explicit SmoothScrollListView(QWidget *parent = nullptr)
        : DListView(parent)
    {
        // 隐藏水平滚动条
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        // 允许鼠标追踪以实现悬停效果
        setMouseTracking(true);
    }

protected:
    void wheelEvent(QWheelEvent *event) override
    {
        if (m_anim && m_anim->state() == QAbstractAnimation::Running) {
            // 动画进行中，累积增量
            m_pendingDelta += event->angleDelta().y();
            event->accept();
            return;
        }

        m_pendingDelta = event->angleDelta().y();

        QScrollBar *vbar = verticalScrollBar();
        if (!vbar) {
            DListView::wheelEvent(event);
            return;
        }

        int currentValue = vbar->value();
        // 每次滚动 3 个条目的高度（约 120px）
        int step = 120;
        int targetValue = currentValue - (m_pendingDelta > 0 ? step : -step);
        targetValue = qBound(vbar->minimum(), targetValue, vbar->maximum());

        if (targetValue == currentValue) {
            event->accept();
            return;
        }

        if (!m_anim) {
            m_anim = new QPropertyAnimation(vbar, "value", this);
            m_anim->setDuration(250);
            m_anim->setEasingCurve(QEasingCurve::OutCubic);
        }

        m_anim->stop();
        m_anim->setStartValue(currentValue);
        m_anim->setEndValue(targetValue);
        m_anim->start();

        event->accept();
    }

private:
    QPropertyAnimation *m_anim = nullptr;
    int m_pendingDelta = 0;
};

// ---------------------------------------------------------------------------
// ClipItemDelegate: 自定义委托（支持紧凑模式）
// ---------------------------------------------------------------------------

class ClipItemDelegate : public QStyledItemDelegate
{
public:
    explicit ClipItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void setCompactMode(bool compact) { m_compact = compact; }

    enum DataRole {
        PreviewRole    = Qt::DisplayRole,
        PinnedRole     = Qt::UserRole + 1,
        TimestampRole  = Qt::UserRole + 2,
        TypeRole       = Qt::UserRole + 3,   // 0=text, 1=image
        ThumbRole      = Qt::UserRole + 4,   // QPixmap
        LabelRole      = Qt::UserRole + 5,   // "图片 1920×1080"
        StarRole       = Qt::UserRole + 6,   // 收藏标记
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

        // 置顶条目的左侧色条
        bool pinned = index.data(PinnedRole).toBool();
        if (pinned) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(QColor(245, 167, 38, 200));
            painter->drawRoundedRect(QRect(rect.left() + 2, rect.top() + 4, 3, rect.height() - 8), 1, 1);
        }

        QColor textColor;
        if (option.state & QStyle::State_Selected)
            textColor = option.palette.color(QPalette::HighlightedText);
        else
            textColor = option.palette.color(QPalette::Text);

        int type = index.data(TypeRole).toInt();

        if (type == 1) {
            drawImageItem(painter, option, index, rect, textColor);
        } else {
            drawTextItem(painter, option, index, rect, textColor);
        }

        drawTimestamp(painter, option, index, rect, textColor);

        if (pinned) {
            drawPinMark(painter, rect);
        }

        // 收藏星标
        bool starred = index.data(StarRole).toBool();
        if (starred) {
            drawStarMark(painter, rect);
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
        font.setPointSizeF(font.pointSizeF() * (m_compact ? 0.82 : 0.9));
        painter->setFont(font);
        painter->setPen(textColor);

        QFontMetrics fm(font);
        int leftPad = 12;
        int rightPad = 52;  // 留出时间戳空间
        QString elided = fm.elidedText(preview, Qt::ElideRight,
                                       rect.width() - leftPad - rightPad);

        int topPad = m_compact ? 3 : 6;
        int textH = m_compact ? rect.height() - 6 : rect.height() - 12;

        // 紧凑模式：单行；标准模式：允许换行
        if (m_compact) {
            painter->drawText(QRect(rect.left() + leftPad, rect.top() + topPad,
                                    rect.width() - leftPad - rightPad, textH),
                              Qt::AlignLeft | Qt::AlignVCenter, elided);
        } else {
            // 标准模式：最多 2 行
            QRect textRect(rect.left() + leftPad, rect.top() + topPad,
                           rect.width() - leftPad - rightPad, textH);
            painter->drawText(textRect,
                              Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                              elided);
        }
    }

    void drawImageItem(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index, const QRect &rect,
                      const QColor &textColor) const
    {
        Q_UNUSED(option)

        QPixmap thumb = index.data(ThumbRole).value<QPixmap>();
        QString label = index.data(LabelRole).toString();

        // 紧凑模式：更小的缩略图
        int thumbW = m_compact ? 48 : 90;
        int thumbH = m_compact ? 30 : 54;
        int thumbX = rect.left() + 8;
        int thumbY = rect.top() + (rect.height() - thumbH) / 2;

        QRectF thumbRect(thumbX, thumbY, thumbW, thumbH);
        QPainterPath borderPath;
        borderPath.addRoundedRect(thumbRect, 4, 4);
        painter->setPen(QPen(QColor(0, 0, 0, 30), 1));
        painter->setBrush(QColor(255, 255, 255, 15));
        painter->drawPath(borderPath);

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

        QFont font = QApplication::font();
        font.setPointSizeF(font.pointSizeF() * (m_compact ? 0.75 : 0.9));
        painter->setFont(font);
        painter->setPen(textColor);

        // 紧凑模式：标签简化
        if (m_compact) {
            // 只显示 "IMG" 标签
            painter->drawText(QRect(thumbX + thumbW + 6, thumbY,
                                    rect.width() - thumbW - 60, thumbH),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              QStringLiteral("图片"));
        } else {
            painter->drawText(QRect(thumbX + thumbW + 8, thumbY,
                                    rect.width() - thumbW - 32, thumbH),
                              Qt::AlignLeft | Qt::AlignVCenter, label);
        }
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
        smallFont.setPointSizeF(smallFont.pointSizeF() * (m_compact ? 0.62 : 0.7));
        painter->setFont(smallFont);

        QColor subColor = textColor;
        subColor.setAlpha(120);
        painter->setPen(subColor);
        painter->drawText(QRect(rect.right() - 48, rect.top() + 3, 38, 14),
                          Qt::AlignRight | Qt::AlignTop, timeStr);
    }

    void drawPinMark(QPainter *painter, const QRect &rect) const
    {
        QPainterPath pinPath;
        int px = rect.right() - 12;
        int py = rect.top() + 6;
        int sz = m_compact ? 6 : 8;
        pinPath.moveTo(px, py);
        pinPath.lineTo(px + sz, py);
        pinPath.lineTo(px + sz / 2, py + sz);
        pinPath.closeSubpath();
        QColor pinColor(245, 167, 38);
        painter->fillPath(pinPath, pinColor);
    }

    void drawStarMark(QPainter *painter, const QRect &rect) const
    {
        // 右下角小星标
        int sx = rect.right() - 16;
        int sy = rect.bottom() - 12;
        QPainterPath starPath;
        starPath.moveTo(sx, sy - 4);
        starPath.lineTo(sx + 1, sy - 1);
        starPath.lineTo(sx + 4, sy - 1);
        starPath.lineTo(sx + 2, sy + 1);
        starPath.lineTo(sx + 3, sy + 4);
        starPath.lineTo(sx, sy + 2);
        starPath.lineTo(sx - 3, sy + 4);
        starPath.lineTo(sx - 2, sy + 1);
        starPath.lineTo(sx - 4, sy - 1);
        starPath.lineTo(sx - 1, sy - 1);
        starPath.closeSubpath();
        painter->fillPath(starPath, QColor(241, 196, 15));
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        int type = index.data(TypeRole).toInt();
        if (m_compact) {
            // 紧凑模式：文本 26px，图片 36px
            return QSize(250, type == 1 ? 36 : 26);
        }
        // 标准模式：文本 48px，图片 66px
        return QSize(250, type == 1 ? 66 : 48);
    }

private:
    bool m_compact = false;
};

// ---------------------------------------------------------------------------
// ClipboardWidget
// ---------------------------------------------------------------------------

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

    // 搜索栏 + 紧凑按钮
    auto *searchLayout = new QHBoxLayout;
    searchLayout->setSpacing(4);

    m_searchEdit = new DLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索剪贴板…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedHeight(32);
    searchLayout->addWidget(m_searchEdit, 1);

    // 紧凑模式切换按钮
    m_compactBtn = new DToolButton(this);
    m_compactBtn->setText(QStringLiteral("紧凑"));
    m_compactBtn->setCheckable(true);
    m_compactBtn->setToolTip(QStringLiteral("切换紧凑/标准显示模式"));
    m_compactBtn->setFixedSize(40, 32);
    searchLayout->addWidget(m_compactBtn);

    layout->addLayout(searchLayout);

    // 平滑滚动列表
    m_listView = new SmoothScrollListView(this);
    m_model = new QStandardItemModel(this);
    m_listView->setModel(m_model);
    m_delegate = new ClipItemDelegate(m_listView);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setUniformItemSizes(false);
    m_listView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_listView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listView->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_listView->setContextMenuPolicy(Qt::CustomContextMenu);
    // 启用平滑滚动：设置 ScrollMode 为 PerPixel
    m_listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_listView, 1);

    connect(m_searchEdit, &DLineEdit::textChanged,
            this, &ClipboardWidget::onSearchChanged);
    connect(m_listView, &DListView::clicked,
            this, &ClipboardWidget::onItemClicked);
    connect(m_listView, &DListView::customContextMenuRequested,
            this, &ClipboardWidget::onContextMenu);
    connect(m_compactBtn, &DToolButton::clicked,
            this, &ClipboardWidget::onCompactToggled);
    connect(m_manager, &ClipboardManager::historyChanged,
            this, &ClipboardWidget::onHistoryChanged);
}

void ClipboardWidget::setCompactMode(bool compact)
{
    if (m_compactMode == compact) return;
    m_compactMode = compact;
    m_compactBtn->setChecked(compact);

    // 更新委托
    if (m_delegate) {
        m_delegate->setCompactMode(compact);
    }

    // 刷新列表以更新行高
    refreshList(m_currentItems);
}

void ClipboardWidget::onCompactToggled()
{
    setCompactMode(m_compactBtn->isChecked());
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

    // 分离 pinned 和普通条目
    QList<ClipboardManager::ClipItem> pinnedItems;
    QList<ClipboardManager::ClipItem> normalItems;
    for (const auto &item : items) {
        if (item.pinned)
            pinnedItems.append(item);
        else
            normalItems.append(item);
    }

    // 先添加 pinned 条目
    for (const auto &item : pinnedItems) {
        auto *stdItem = new QStandardItem;

        if (item.type == ClipboardManager::ImageClip) {
            stdItem->setText(item.imageLabel);
            stdItem->setData(1, ClipItemDelegate::TypeRole);
            stdItem->setData(item.thumbnail, ClipItemDelegate::ThumbRole);
            stdItem->setData(item.imageLabel, ClipItemDelegate::LabelRole);
        } else {
            stdItem->setText(item.preview);
            stdItem->setData(0, ClipItemDelegate::TypeRole);
        }

        stdItem->setData(item.pinned, ClipItemDelegate::PinnedRole);
        stdItem->setData(item.timestamp, ClipItemDelegate::TimestampRole);
        stdItem->setEditable(false);
        m_model->appendRow(stdItem);
    }

    // 再添加普通条目
    for (const auto &item : normalItems) {
        auto *stdItem = new QStandardItem;

        if (item.type == ClipboardManager::ImageClip) {
            stdItem->setText(item.imageLabel);
            stdItem->setData(1, ClipItemDelegate::TypeRole);
            stdItem->setData(item.thumbnail, ClipItemDelegate::ThumbRole);
            stdItem->setData(item.imageLabel, ClipItemDelegate::LabelRole);
        } else {
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
    menu.addSeparator();
    QAction *copyAct = menu.addAction(QStringLiteral("复制内容"));

    QAction *ret = menu.exec(m_listView->viewport()->mapToGlobal(pos));
    if (ret == pinAct) {
        m_manager->togglePin(item.id);
    } else if (ret == delAct) {
        m_manager->remove(item.id);
    } else if (ret == copyAct) {
        m_manager->copyToClipboard(item.id);
    }
}
