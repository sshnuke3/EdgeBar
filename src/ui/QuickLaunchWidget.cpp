#include "QuickLaunchWidget.h"
#include "core/SearchEngine.h"

#include <QVBoxLayout>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QIcon>
#include <algorithm>

// ---- 简化委托 ----
class LaunchItemDelegate : public QStyledItemDelegate
{
public:
    explicit LaunchItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent) {}

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        QRect rect = option.rect;

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

        // 图标
        QString iconName = index.data(Qt::UserRole + 1).toString();
        QIcon icon = QIcon::fromTheme(iconName);
        if (icon.isNull()) icon = QIcon::fromTheme("application-x-executable");

        int iconSize = 24;
        icon.paint(painter, rect.left() + 8, rect.top() + 6, iconSize, iconSize);

        // 标题
        QColor textColor;
        if (option.state & QStyle::State_Selected)
            textColor = option.palette.color(QPalette::HighlightedText);
        else
            textColor = option.palette.color(QPalette::Text);

        QFont font = QApplication::font();
        font.setPointSizeF(font.pointSizeF() * 0.9);
        painter->setFont(font);
        painter->setPen(textColor);

        QFontMetrics fm(font);
        QString title = index.data(Qt::DisplayRole).toString();
        int textWidth = rect.width() - 44;
        QString elided = fm.elidedText(title, Qt::ElideRight, textWidth);
        painter->drawText(QRect(rect.left() + 40, rect.top() + 4,
                                textWidth, rect.height() - 8),
                          Qt::AlignLeft | Qt::AlignVCenter, elided);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        return QSize(250, 36);
    }
};

// ---- QuickLaunchWidget ----

QuickLaunchWidget::QuickLaunchWidget(QWidget *parent)
    : DWidget(parent)
{
    setupUI();
}

void QuickLaunchWidget::setupUI()
{
    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(4);
    layout->setContentsMargins(6, 6, 6, 6);

    m_searchEdit = new DLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索应用或命令…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setFixedHeight(32);
    layout->addWidget(m_searchEdit);

    m_resultList = new DListView(this);
    m_model = new QStandardItemModel(this);
    m_resultList->setModel(m_model);
    m_resultList->setItemDelegate(new LaunchItemDelegate(m_resultList));
    m_resultList->setUniformItemSizes(true);
    m_resultList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_resultList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_resultList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_resultList, 1);

    connect(m_searchEdit, &DLineEdit::textChanged,
            this, &QuickLaunchWidget::onSearchChanged);
    connect(m_resultList, &DListView::clicked,
            this, &QuickLaunchWidget::onItemClicked);
}

void QuickLaunchWidget::registerPlugin(ISearchPlugin *plugin)
{
    if (plugin) m_plugins.append(plugin);
}

void QuickLaunchWidget::onSearchChanged(const QString &text)
{
    performSearch(text.trimmed());
}

void QuickLaunchWidget::performSearch(const QString &query)
{
    m_model->clear();
    m_currentResults.clear();

    if (query.isEmpty()) return;

    for (auto *plugin : m_plugins) {
        if (!plugin->shouldActivate(query)) continue;
        QList<SearchResult> results = plugin->search(query);
        for (auto &r : results) {
            r.pluginId = plugin->id();
            m_currentResults.append(r);
        }
    }

    std::sort(m_currentResults.begin(), m_currentResults.end(),
              [](const SearchResult &a, const SearchResult &b) {
                  return a.score > b.score;
              });

    for (const auto &r : m_currentResults) {
        auto *item = new QStandardItem;
        item->setText(r.title);
        item->setData(r.iconName, Qt::UserRole + 1);
        item->setEditable(false);
        m_model->appendRow(item);
    }

    if (m_model->rowCount() > 0)
        m_resultList->setCurrentIndex(m_model->index(0, 0));
}

void QuickLaunchWidget::onItemClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    int row = index.row();
    if (row < 0 || row >= m_currentResults.size()) return;

    const SearchResult &result = m_currentResults[row];
    for (auto *plugin : m_plugins) {
        if (plugin->id() == result.pluginId) {
            plugin->activate(result);
            return;
        }
    }
}
