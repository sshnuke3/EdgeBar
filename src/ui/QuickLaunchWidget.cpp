#include "QuickLaunchWidget.h"
#include "core/SearchEngine.h"
#include "core/Logging.h"

#include <QVBoxLayout>
#include <QStandardItem>
#include <QStyledItemDelegate>
#include <QPainter>
#include <QPainterPath>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QAction>
#include <QProcess>
#include <QFile>
#include <QTextStream>
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
    // 右键菜单
    m_resultList->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_resultList, 1);

    connect(m_searchEdit, &DLineEdit::textChanged,
            this, &QuickLaunchWidget::onSearchChanged);
    connect(m_resultList, &DListView::clicked,
            this, &QuickLaunchWidget::onItemClicked);
    connect(m_resultList, &DListView::customContextMenuRequested,
            this, &QuickLaunchWidget::onContextMenu);
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

// ---------------------------------------------------------------------------
// onContextMenu: 右键菜单 — 系统快捷操作
// ---------------------------------------------------------------------------
//
// 论坛需求："任务栏右键功能增强"（12月需求池）
// 提供锁屏、注销、挂起、重启、关机，以及亮度调节。
// 点击空白区域或列表项均可弹出。
//
// ---------------------------------------------------------------------------

void QuickLaunchWidget::onContextMenu(const QPoint &pos)
{
    QMenu menu(this);

    // 系统操作组
    auto *lockAct = menu.addAction(QIcon::fromTheme("lock"),
                                    QStringLiteral("锁屏"));
    auto *logoutAct = menu.addAction(QIcon::fromTheme("system-log-out"),
                                      QStringLiteral("注销"));
    menu.addSeparator();
    auto *suspendAct = menu.addAction(QIcon::fromTheme("system-suspend"),
                                       QStringLiteral("挂起"));
    auto *rebootAct = menu.addAction(QIcon::fromTheme("system-reboot"),
                                      QStringLiteral("重启"));
    auto *shutdownAct = menu.addAction(QIcon::fromTheme("system-shutdown"),
                                        QStringLiteral("关机"));

    menu.addSeparator();

    // 亮度调节子菜单
    auto *brightMenu = menu.addMenu(QIcon::fromTheme("display"),
                                    QStringLiteral("屏幕亮度"));
    auto *brightUp = brightMenu->addAction(QStringLiteral("调亮 (+10%)"));
    auto *brightDown = brightMenu->addAction(QStringLiteral("调暗 (-10%)"));
    brightMenu->addSeparator();
    auto *brightMax = brightMenu->addAction(QStringLiteral("最亮"));
    auto *brightMin = brightMenu->addAction(QStringLiteral("最暗"));

    QAction *ret = menu.exec(m_resultList->viewport()->mapToGlobal(pos));
    if (!ret) return;

    QString action;
    if (ret == lockAct)         action = "lock";
    else if (ret == logoutAct)  action = "logout";
    else if (ret == suspendAct) action = "suspend";
    else if (ret == rebootAct)  action = "reboot";
    else if (ret == shutdownAct) action = "shutdown";
    else if (ret == brightUp)   action = "brightness_up";
    else if (ret == brightDown) action = "brightness_down";
    else if (ret == brightMax)  action = "brightness_max";
    else if (ret == brightMin)  action = "brightness_min";

    if (!action.isEmpty()) {
        executeSystemAction(action);
    }
}

// ---------------------------------------------------------------------------
// executeSystemAction: 执行系统快捷操作
// ---------------------------------------------------------------------------
//
// 使用 deepin/dde 提供的 DBus 接口或命令行工具：
//   锁屏     — loginctl lock-session
//   注销     — loginctl terminate-session
//   挂起     — systemctl suspend
//   重启     — systemctl reboot
//   关机     — systemctl poweroff
//   亮度调节 — /sys/class/backlight/ 读写
//
// ---------------------------------------------------------------------------

void QuickLaunchWidget::executeSystemAction(const QString &action)
{
    qCInfo(edgebarLog) << "System action:" << action;

    if (action == "lock") {
        QProcess::startDetached("loginctl", {"lock-session"});
    }
    else if (action == "logout") {
        QProcess::startDetached("loginctl", {"terminate-session", "auto"});
    }
    else if (action == "suspend") {
        QProcess::startDetached("systemctl", {"suspend"});
    }
    else if (action == "reboot") {
        QProcess::startDetached("systemctl", {"reboot"});
    }
    else if (action == "shutdown") {
        QProcess::startDetached("systemctl", {"poweroff"});
    }
    else if (action.startsWith("brightness")) {
        // 读取当前亮度
        QFile maxFile("/sys/class/backlight/intel_backlight/max_brightness");
        QFile curFile("/sys/class/backlight/intel_backlight/brightness");
        if (!maxFile.open(QIODevice::ReadOnly) || !curFile.open(QIODevice::ReadOnly)) {
            qCWarning(edgebarLog) << "Cannot read brightness sysfs";
            return;
        }
        int maxVal = maxFile.readAll().trimmed().toInt();
        int curVal = curFile.readAll().trimmed().toInt();
        maxFile.close();
        curFile.close();

        if (maxVal <= 0) return;

        int newVal = curVal;
        if (action == "brightness_up")
            newVal = curVal + maxVal / 10;
        else if (action == "brightness_down")
            newVal = curVal - maxVal / 10;
        else if (action == "brightness_max")
            newVal = maxVal;
        else if (action == "brightness_min")
            newVal = maxVal / 20;  // 5% minimum

        newVal = qBound(maxVal / 20, newVal, maxVal);

        // 写入新亮度（需要 root 权限，通常通过 pkexec 或用户有写权限）
        QFile writeFile("/sys/class/backlight/intel_backlight/brightness");
        if (writeFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            writeFile.write(QByteArray::number(newVal));
            writeFile.close();
            qCDebug(edgebarLog) << "Brightness set to" << newVal
                                << "/" << maxVal;
        } else {
            // 退用 dbus 方式
            QProcess::startDetached("dbus-send",
                {"--session", "--dest=com.deepin.daemon.Display",
                 "--type=method_call", "/com/deepin/daemon/Display",
                 "com.deepin.daemon.Display.SetBrightness",
                 "string:intel_backlight", "double:" +
                 QString::number(newVal * 100.0 / maxVal).toLatin1()});
            qCWarning(edgebarLog) << "Brightness sysfs write failed, tried dbus";
        }
    }
}
