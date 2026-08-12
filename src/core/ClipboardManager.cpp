#include "ClipboardManager.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDebug>

// ---------------------------------------------------------------------------
// 构造函数
// ---------------------------------------------------------------------------
//
// 初始化剪贴板监听与防抖定时器：
//   - dataChanged 信号触发防抖定时器（每次变化重新计时）
//   - 防抖定时器超时后执行实际处理 onClipboardChanged()
//
// ---------------------------------------------------------------------------

ClipboardManager::ClipboardManager(QObject *parent)
    : QObject(parent)
{
    m_clipboard = QGuiApplication::clipboard();

    // 防抖定时器：单次触发，300ms 间隔
    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(300);

    // 超时后执行实际处理
    connect(&m_debounceTimer, &QTimer::timeout, this, [this]() {
        onClipboardChanged();
    });

    // 剪贴板数据变化时启动防抖定时器（重新计时）
    connect(m_clipboard, &QClipboard::dataChanged, this, [this]() {
        m_debounceTimer.start();
    });
}

// ---------------------------------------------------------------------------
// onClipboardChanged: 实际处理剪贴板变化（由防抖定时器触发）
// ---------------------------------------------------------------------------

void ClipboardManager::onClipboardChanged()
{
    QString text = m_clipboard->text();

    // 空文本或与上次相同则跳过
    if (text.isEmpty() || text == m_lastText) {
        return;
    }

    m_lastText = text;

    // 去重：如果已存在相同文本，移到最前
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].text == text) {
            ClipItem item = m_items.takeAt(i);
            item.timestamp = QDateTime::currentSecsSinceEpoch();
            m_items.prepend(item);
            emit historyChanged();
            return;
        }
    }

    // 新条目
    addItem(text);
}

// ---------------------------------------------------------------------------
// addItem: 添加新条目到历史列表开头
// ---------------------------------------------------------------------------

void ClipboardManager::addItem(const QString &text)
{
    ClipItem item;
    item.id = m_nextId++;
    item.text = text;

    // 预览：替换换行为空格，截断到 PREVIEW_LEN 个字符
    QString preview = text;
    preview.replace(QChar('\n'), QChar(' '));
    preview.replace(QChar('\r'), QChar(' '));
    if (preview.length() > PREVIEW_LEN) {
        preview = preview.left(PREVIEW_LEN);
    }
    item.preview = preview;

    item.timestamp = QDateTime::currentSecsSinceEpoch();

    // 插入到开头（最新的在最前）
    m_items.prepend(item);

    // 超过上限且最后一条非置顶时，删除最后一条
    if (m_items.size() > MAX_ITEMS && !m_items.last().pinned) {
        m_items.removeLast();
    }

    emit historyChanged();
}

// ---------------------------------------------------------------------------
// togglePin: 置顶 / 取消置顶
// ---------------------------------------------------------------------------
//
// 翻转指定条目的 pinned 状态。
// 置顶后将其移到列表最前，取消置顶则保持当前位置。
//
// ---------------------------------------------------------------------------

void ClipboardManager::togglePin(int id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            m_items[i].pinned = !m_items[i].pinned;

            // 置顶后移到列表最前
            if (m_items[i].pinned) {
                ClipItem item = m_items.takeAt(i);
                m_items.prepend(item);
            }

            emit historyChanged();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// remove: 删除指定条目
// ---------------------------------------------------------------------------

void ClipboardManager::remove(int id)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) {
            m_items.removeAt(i);
            emit historyChanged();
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// clearAll: 清空所有非置顶条目（保留 pinned）
// ---------------------------------------------------------------------------

void ClipboardManager::clearAll()
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!m_items[i].pinned) {
            m_items.removeAt(i);
        }
    }
    emit historyChanged();
}

// ---------------------------------------------------------------------------
// copyToClipboard: 将指定条目复制到剪贴板
// ---------------------------------------------------------------------------
//
// 使用 blockSignals 阻止 dataChanged 信号，避免触发 onClipboardChanged 循环。
// 同时同步 m_lastText 以做双重保险。
//
// ---------------------------------------------------------------------------

void ClipboardManager::copyToClipboard(int id)
{
    for (const ClipItem &item : m_items) {
        if (item.id == id) {
            m_clipboard->blockSignals(true);
            m_clipboard->setText(item.text);
            m_clipboard->blockSignals(false);
            m_lastText = item.text;  // 同步 lastText 避免重复添加
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// filteredItems: 关键字过滤（不区分大小写），置顶条目排在前面
// ---------------------------------------------------------------------------

QList<ClipboardManager::ClipItem> ClipboardManager::filteredItems(const QString &keyword) const
{
    QList<ClipItem> pinned;
    QList<ClipItem> normal;
    QString kw = keyword.toLower();

    for (const ClipItem &item : m_items) {
        if (item.text.toLower().contains(kw)) {
            if (item.pinned) {
                pinned.append(item);
            } else {
                normal.append(item);
            }
        }
    }

    // 置顶的排在前，非置顶的在后
    pinned.append(normal);
    return pinned;
}
