#include "ClipboardManager.h"
#include "Logging.h"

#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QDateTime>
#include <QDebug>
#include <QBuffer>
#include <QCryptographicHash>

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
//
// 优先检查图片，再检查文本：
//   - 图片：如果启用了图片历史且剪贴板有图片数据，走图片路径
//   - 文本：空文本或与上次相同则跳过，否则走文本路径
//
// ---------------------------------------------------------------------------

void ClipboardManager::onClipboardChanged()
{
    // 先检查图片（截图等操作只产生图片，不产生文本）
    if (m_enableImages) {
        const QMimeData *mime = m_clipboard->mimeData();
        if (mime && mime->hasImage()) {
            QImage image = m_clipboard->image();
            if (!image.isNull()) {
                QByteArray hash = imageHash(image);
                if (hash != m_lastImageHash) {
                    m_lastImageHash = hash;
                    m_lastText.clear();  // 清空文本去重基线，因为图片操作会同时清空文本
                    addImageItem(image);
                    return;
                }
            }
        }
    }

    // 文本路径
    QString text = m_clipboard->text();

    // 空文本或与上次相同则跳过
    if (text.isEmpty() || text == m_lastText) {
        return;
    }

    m_lastText = text;
    m_lastImageHash.clear();  // 清空图片去重基线

    // 去重：如果已存在相同文本，移到最前
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].type == TextClip && m_items[i].text == text) {
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
// imageHash: 计算图片内容的 hash 用于去重
// ---------------------------------------------------------------------------
//
// 将 QImage 转为 PNG 字节流后做 MD5，比逐像素比较快且足够可靠。
//
// ---------------------------------------------------------------------------

QByteArray ClipboardManager::imageHash(const QImage &image)
{
    QByteArray ba;
    QBuffer buf(&ba);
    buf.open(QIODevice::WriteOnly);
    // 降采样以加速 hash：缩到 64x64 再编码
    QImage scaled = image.scaled(64, 64, Qt::KeepAspectRatioByExpanding,
                                  Qt::FastTransformation);
    scaled.save(&buf, "PNG");
    return QCryptographicHash::hash(ba, QCryptographicHash::Md5);
}

// ---------------------------------------------------------------------------
// makeThumbnail: 生成保持比例的缩略图
// ---------------------------------------------------------------------------

QPixmap ClipboardManager::makeThumbnail(const QImage &src)
{
    if (src.width() <= THUMB_W && src.height() <= THUMB_H) {
        return QPixmap::fromImage(src);
    }
    return QPixmap::fromImage(
        src.scaled(THUMB_W, THUMB_H, Qt::KeepAspectRatio,
                    Qt::SmoothTransformation));
}

// ---------------------------------------------------------------------------
// addItem: 添加新文本条目到历史列表开头
// ---------------------------------------------------------------------------

void ClipboardManager::addItem(const QString &text)
{
    ClipItem item;
    item.id = m_nextId++;
    item.type = TextClip;
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
    if (m_items.size() > m_maxItems && !m_items.last().pinned) {
        m_items.removeLast();
    }

    emit historyChanged();
}

// ---------------------------------------------------------------------------
// addImageItem: 添加新图片条目到历史列表开头
// ---------------------------------------------------------------------------

void ClipboardManager::addImageItem(const QImage &image)
{
    ClipItem item;
    item.id = m_nextId++;
    item.type = ImageClip;
    item.thumbnail = makeThumbnail(image);

    // 生成标签："图片 1920×1080"
    item.imageLabel = QStringLiteral("图片 %1×%2")
                          .arg(image.width()).arg(image.height());

    item.timestamp = QDateTime::currentSecsSinceEpoch();

    // 注：图片去重已在 onClipboardChanged() 中通过 m_lastImageHash 完成，
    // 此处直接插入新条目

    m_items.prepend(item);

    if (m_items.size() > m_maxItems && !m_items.last().pinned) {
        m_items.removeLast();
    }

    qCInfo(edgebarLog) << "Clipboard image added:" << item.imageLabel
                       << "items:" << m_items.size();
    emit historyChanged();
}

// ---------------------------------------------------------------------------
// setMaxItems: 设置最大历史条目数
// ---------------------------------------------------------------------------

void ClipboardManager::setMaxItems(int max)
{
    m_maxItems = qMax(1, max);
    while (m_items.size() > m_maxItems && !m_items.last().pinned) {
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
// 文本类型用 setText()，图片类型用 setPixmap()。
// 同时同步去重基线以做双重保险。
//
// ---------------------------------------------------------------------------

void ClipboardManager::copyToClipboard(int id)
{
    for (const ClipItem &item : m_items) {
        if (item.id == id) {
            m_clipboard->blockSignals(true);

            if (item.type == ImageClip) {
                m_clipboard->setPixmap(item.thumbnail);
                m_lastImageHash = imageHash(item.thumbnail.toImage());
                m_lastText.clear();
            } else {
                m_clipboard->setText(item.text);
                m_lastText = item.text;
                m_lastImageHash.clear();
            }

            m_clipboard->blockSignals(false);
            return;
        }
    }
}

// ---------------------------------------------------------------------------
// filteredItems: 关键字过滤（不区分大小写），置顶条目排在前面
// ---------------------------------------------------------------------------
//
// 文本条目：匹配 text 字段
// 图片条目：匹配 "图片" 关键词或留空时全部展示
//
// ---------------------------------------------------------------------------

QList<ClipboardManager::ClipItem> ClipboardManager::filteredItems(const QString &keyword) const
{
    QList<ClipItem> pinned;
    QList<ClipItem> normal;
    QString kw = keyword.toLower();

    for (const ClipItem &item : m_items) {
        bool match = false;
        if (item.type == TextClip) {
            match = item.text.toLower().contains(kw);
        } else {
            // 图片条目：空关键词时全部展示，否则匹配"图片"标签
            match = kw.isEmpty() || QStringLiteral("图片").contains(kw)
                    || item.imageLabel.toLower().contains(kw);
        }

        if (match) {
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
