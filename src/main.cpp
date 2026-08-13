#include <DApplication>
#include <DLog>

#include "ui/MainWindow.h"
#include "core/Logging.h"
#include "core/IconHelper.h"

DCORE_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    DApplication app(argc, argv);
    app.setApplicationName("EdgeBar");
    app.setOrganizationName("deepin");
    app.setApplicationVersion("1.0.0");
    app.setProductName(QStringLiteral("桌边栏 EdgeBar"));
    app.setProductIcon(edgebarFindIcon("sidebar"));
    app.setApplicationDescription(
        QStringLiteral("屏幕边缘滑出的智能面板：系统监控、剪贴板、快速启动、番茄钟。"));
    app.setApplicationLicense("GPLv3");

    // 日志
    DLogManager::registerConsoleAppender();

    // 翻译
    app.loadTranslator();

    // 单实例（key 与 DConfig appId 保持一致）
    if (!app.setSingleInstance(kEdgeBarAppId)) {
        qCWarning(edgebarLog) << "EdgeBar is already running";
        return 0;
    }

    // 主窗口（DConfig 在构造函数中加载）
    MainWindow w;
    w.show();

    qCInfo(edgebarLog) << "EdgeBar started. Move mouse to screen edge to activate.";

    return app.exec();
}
