#ifndef PROCESSMANAGERWIDGET_H
#define PROCESSMANAGERWIDGET_H

#include <DDialog>
#include <DWidget>
#include <DMainWindow>

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QLabel>
#include <QPushButton>

#include "core/SystemMonitor.h"

DWIDGET_USE_NAMESPACE

/**
 * @brief 进程管理器对话框
 *
 * 显示 CPU 占用最高的进程列表，支持：
 *  - 查看 PID、进程名、CPU%、内存
 *  - 结束指定进程（SIGTERM）
 *  - 自动刷新（2秒）
 *  - 手动刷新
 *
 * 差异化：DeskMon 仅监控，EdgeBar 可直接交互结束进程
 */
class ProcessManagerWidget : public DDialog
{
    Q_OBJECT
public:
    explicit ProcessManagerWidget(SystemMonitor *monitor, DMainWindow *parent = nullptr);

private slots:
    void refreshProcessList();
    void onKillButtonClicked(int row);

private:
    SystemMonitor *m_monitor;
    DWidget *m_contentWidget;
    QTableWidget *m_table;
    QLabel *m_statusLabel;
    QTimer *m_refreshTimer;

    void setupUI();
    QString formatMemSize(qint64 pages) const;
};

#endif // PROCESSMANAGERWIDGET_H
