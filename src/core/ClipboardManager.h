#ifndef CLIPBOARDMANAGER_H
#define CLIPBOARDMANAGER_H

#include <QObject>
#include <QClipboard>
#include <QList>
#include <QVariantMap>
#include <QTimer>
#include <QPixmap>
#include <QImage>

/**
 * @brief 剪贴板历史管理器
 *
 * 监听系统剪贴板变化，保存最近 N 条历史记录。
 * 支持文本和图片两种类型，支持置顶（pin）和搜索过滤。
 */
class ClipboardManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY historyChanged)

public:
    explicit ClipboardManager(QObject *parent = nullptr);

    /// 剪贴板条目类型
    enum ClipType { TextClip, ImageClip };
    Q_ENUM(ClipType)

    /// 历史记录条目
    struct ClipItem {
        int       id;
        ClipType  type = TextClip;

        // 文本数据
        QString   text;
        QString   preview;     // 截断预览

        // 图片数据
        QPixmap   thumbnail;   // 缩略图（最大 120x80）
        QString   imageLabel;  // "图片 1920×1080"

        bool      pinned = false;
        qint64    timestamp;
    };

    /// 获取全部历史
    const QList<ClipItem> &items() const { return m_items; }

    /// 获取过滤后的历史
    QList<ClipItem> filteredItems(const QString &keyword) const;

    /// 条目数量
    int count() const { return m_items.size(); }

    /// 设置最大条目数
    void setMaxItems(int max);

    /// 设置是否启用图片历史
    void setEnableImages(bool enabled) { m_enableImages = enabled; }

    /// 置顶/取消置顶
    void togglePin(int id);

    /// 删除单条
    void remove(int id);

    /// 清空所有（保留 pinned）
    void clearAll();

public slots:
    /// 将指定条目复制到剪贴板
    void copyToClipboard(int id);

signals:
    void historyChanged();

private slots:
    void onClipboardChanged();

private:
    QClipboard *m_clipboard;
    QList<ClipItem> m_items;
    int m_nextId = 1;
    QString m_lastText;
    QByteArray m_lastImageHash;
    QTimer m_debounceTimer;

    int m_maxItems = 50;
    bool m_enableImages = true;
    static constexpr int PREVIEW_LEN = 80;
    static constexpr int THUMB_W = 120;
    static constexpr int THUMB_H = 80;

    void addItem(const QString &text);
    void addImageItem(const QImage &image);

    /// 生成缩略图（保持比例，最大 THUMB_W×THUMB_H）
    static QPixmap makeThumbnail(const QImage &src);

    /// 计算图片 hash 用于去重
    static QByteArray imageHash(const QImage &image);
};

#endif // CLIPBOARDMANAGER_H
