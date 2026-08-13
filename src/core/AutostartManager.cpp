#include "AutostartManager.h"
#include "Logging.h"

#include <DStandardPaths>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

DCORE_USE_NAMESPACE

// ---------------------------------------------------------------------------
// AutostartManager
// ---------------------------------------------------------------------------

AutostartManager::AutostartManager(QObject *parent)
    : QObject(parent)
{
}

QString AutostartManager::autostartDir() const
{
    // XDG autostart 目录: ~/.config/autostart
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
           + QStringLiteral("/autostart");
}

QString AutostartManager::desktopFilePath() const
{
    return autostartDir() + QStringLiteral("/edgebar.desktop");
}

bool AutostartManager::isEnabled() const
{
    return QFile::exists(desktopFilePath());
}

bool AutostartManager::enable()
{
    QDir dir;
    if (!dir.exists(autostartDir())) {
        if (!dir.mkpath(autostartDir())) {
            qCWarning(edgebarLog) << "Cannot create autostart dir:" << autostartDir();
            return false;
        }
    }

    // 写入 .desktop 文件
    QFile file(desktopFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCWarning(edgebarLog) << "Cannot create autostart desktop file";
        return false;
    }

    file.write("[Desktop Entry]\n");
    file.write("Type=Application\n");
    file.write("Name=EdgeBar\n");
    file.write("Name[zh_CN]=桌边栏\n");
    file.write("Comment=Screen edge smart panel\n");
    file.write("Comment[zh_CN]=屏幕边缘滑出的智能面板\n");
    file.write("Exec=edgebar\n");
    file.write("Icon=sidebar\n");
    file.write("Terminal=false\n");
    file.write("Categories=Utility;\n");
    file.write("X-GNOME-Autostart-enabled=true\n");
    file.close();

    qCInfo(edgebarLog) << "Autostart enabled:" << desktopFilePath();
    return true;
}

bool AutostartManager::disable()
{
    if (!isEnabled()) return true;

    if (!QFile::remove(desktopFilePath())) {
        qCWarning(edgebarLog) << "Cannot remove autostart desktop file";
        return false;
    }

    qCInfo(edgebarLog) << "Autostart disabled";
    return true;
}
