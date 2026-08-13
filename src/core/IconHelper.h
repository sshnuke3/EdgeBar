#ifndef EDGE_ICON_HELPER_H
#define EDGE_ICON_HELPER_H

#include <QIcon>

// DIconTheme 在 DTK6 中引入；DTK5 回退到 QIcon::fromTheme
// 优先检测 DTK_VERSION_MAJOR >= 6，其次用 __has_include 探测头文件
#if (defined(DTK_VERSION_MAJOR) && DTK_VERSION_MAJOR >= 6) || __has_include(<DIconTheme>)
#include <DIconTheme>
inline QIcon edgebarFindIcon(const QString &iconName, const QIcon &fallback = QIcon())
{
    return DIconTheme::findQIcon(iconName, fallback);
}
#else
inline QIcon edgebarFindIcon(const QString &iconName, const QIcon &fallback = QIcon())
{
    QIcon icon = QIcon::fromTheme(iconName);
    if (icon.isNull() && !fallback.isNull())
        return fallback;
    return icon;
}
#endif

#endif // EDGE_ICON_HELPER_H
