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
    // Skill W-030: applicationName 必须与可执行文件名 (edgebar) 完全一致，
    // loadTranslator() 据此查找 edgebar_zh_CN.qm 翻译文件
    app.setApplicationName("edgebar");
    app.setOrganizationName("deepin");
    app.setApplicationVersion("1.0.0");
    app.setProductName(QStringLiteral("桌边栏 EdgeBar"));
    app.setProductIcon(edgebarFindIcon("sidebar"));
    app.setApplicationDescription(
        QStringLiteral("屏幕边缘滑出的智能面板：系统监控、剪贴板、快速启动、番茄钟。"));
    app.setApplicationLicense("GPLv3");

    // 日志：控制台 + 文件（Skill U-005）
    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

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

    // Skill W-025: 第二实例启动时恢复原窗口并激活
    // DTK5 信号 newInstanceStarted() 无参数；DTK6 可能带 args 参数
    QObject::connect(&app, &DApplication::newInstanceStarted,
                     &app, [&w]() {
        qCInfo(edgebarLog) << "New instance detected, activating window";
        // 从隐藏状态滑入
        if (w.autoHide() && w.isHidden_()) {
            w.slideIn();
        }
        w.show();
        w.raise();
        w.activateWindow();
    });

    qCInfo(edgebarLog) << "EdgeBar started. Move mouse to screen edge to activate.";

    return app.exec();
}
