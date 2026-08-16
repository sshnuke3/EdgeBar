#include "ProcessManagerWidget.h"
#include "core/Logging.h"

#include <DMainWindow>
#include <DPushButton>
#include <DIconButton>
#include <DLabel>
#include <DPalette>
#include <DGuiApplicationHelper>
#include <DPaletteHelper>

#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>

ProcessManagerWidget::ProcessManagerWidget(SystemMonitor *monitor, DMainWindow *parent)
    : DDialog(parent)
    , m_monitor(monitor)
{
    setWindowTitle(QStringLiteral("进程管理"));
    setModal(false);

    setupUI();

    // 首次刷新
    refreshProcessList();

    // 自动刷新定时器（2 秒）
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &ProcessManagerWidget::refreshProcessList);
    m_refreshTimer->start(2000);

    // 进程被结束时刷新列表
    connect(m_monitor, &SystemMonitor::processKilled,
            this, [this](int pid, const QString &name) {
        m_statusLabel->setText(
            QStringLiteral("已结束: %1 (PID: %2)").arg(name).arg(pid));
    });
}

void ProcessManagerWidget::setupUI()
{
    m_contentWidget = new DWidget(this);
    auto *layout = new QVBoxLayout(m_contentWidget);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    // 状态标签
    m_statusLabel = new DLabel(m_contentWidget);
    m_statusLabel->setText(QStringLiteral("双击进程行可结束进程"));
    m_statusLabel->setStyleSheet(QStringLiteral("color: gray; font-size: 12px;"));
    layout->addWidget(m_statusLabel);

    // 进程表格
    m_table = new QTableWidget(m_contentWidget);
    m_table->setColumnCount(5);
    m_table->setHorizontalHeaderLabels({
        QStringLiteral("PID"),
        QStringLiteral("进程名"),
        QStringLiteral("CPU%"),
        QStringLiteral("内存"),
        QStringLiteral("操作")
    });
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->setMinimumSize(460, 320);

    // 双击结束进程
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, [this](int row, int col) {
        Q_UNUSED(col);
        onKillButtonClicked(row);
    });

    layout->addWidget(m_table);

    // 底部按钮栏
    auto *btnLayout = new QHBoxLayout();

    auto *refreshBtn = new DPushButton(QStringLiteral("刷新"), m_contentWidget);
    connect(refreshBtn, &DPushButton::clicked, this, &ProcessManagerWidget::refreshProcessList);
    btnLayout->addWidget(refreshBtn);

    btnLayout->addStretch();

    auto *closeBtn = new DPushButton(QStringLiteral("关闭"), m_contentWidget);
    connect(closeBtn, &DPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);

    layout->addLayout(btnLayout);

    addContent(m_contentWidget);
}

void ProcessManagerWidget::refreshProcessList()
{
    if (!m_monitor) return;

    auto processes = m_monitor->topCpuProcesses(15);

    // 保存当前选中的 PID
    int selectedPid = -1;
    int curRow = m_table->currentRow();
    if (curRow >= 0 && curRow < m_table->rowCount()) {
        auto *pidItem = m_table->item(curRow, 0);
        if (pidItem) selectedPid = pidItem->text().toInt();
    }

    m_table->setRowCount(processes.size());

    int newSelectedRow = -1;

    for (int i = 0; i < processes.size(); ++i) {
        const auto &proc = processes[i];

        // PID
        auto *pidItem = new QTableWidgetItem(QString::number(proc.pid));
        pidItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 0, pidItem);

        // 进程名
        auto *nameItem = new QTableWidgetItem(proc.name);
        m_table->setItem(i, 1, nameItem);

        // CPU%
        auto *cpuItem = new QTableWidgetItem(
            QString::number(proc.cpuPercent, 'f', 1) + "%");
        cpuItem->setTextAlignment(Qt::AlignCenter);
        // CPU 高占用标红
        if (proc.cpuPercent > 80) {
            cpuItem->setForeground(QBrush(QColor(231, 76, 60)));
        } else if (proc.cpuPercent > 50) {
            cpuItem->setForeground(QBrush(QColor(245, 167, 38)));
        }
        m_table->setItem(i, 2, cpuItem);

        // 内存（直接使用 SystemMonitor 提供的 RSS 数据，避免重复读取 /proc）
        auto *memItem = new QTableWidgetItem(formatMemSize(proc.rssBytes));
        memItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 3, memItem);

        // 结束按钮
        auto *killBtn = new DPushButton(QStringLiteral("结束"));
        killBtn->setProperty("row", i);
        connect(killBtn, &DPushButton::clicked, this, [this, i]() {
            onKillButtonClicked(i);
        });
        m_table->setCellWidget(i, 4, killBtn);

        // 恢复选中
        if (proc.pid == selectedPid) {
            newSelectedRow = i;
        }
    }

    if (newSelectedRow >= 0) {
        m_table->selectRow(newSelectedRow);
    }

    m_statusLabel->setText(
        QStringLiteral("共 %1 个进程 | 双击行或点击「结束」按钮").arg(processes.size()));
}

void ProcessManagerWidget::onKillButtonClicked(int row)
{
    if (row < 0 || row >= m_table->rowCount()) return;

    auto *pidItem = m_table->item(row, 0);
    auto *nameItem = m_table->item(row, 1);
    if (!pidItem || !nameItem) return;

    int pid = pidItem->text().toInt();
    QString name = nameItem->text();

    // 确认对话框
    auto *msgBox = new QMessageBox(this);
    msgBox->setIcon(QMessageBox::Warning);
    msgBox->setWindowTitle(QStringLiteral("确认结束进程"));
    msgBox->setText(QStringLiteral("确定要结束进程 \"%1\" (PID: %2) 吗？\n\n"
                                   "这将向进程发送 SIGTERM 信号。").arg(name).arg(pid));
    msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox->setDefaultButton(QMessageBox::No);

    if (msgBox->exec() == QMessageBox::Yes) {
        if (m_monitor->killProcess(pid)) {
            m_statusLabel->setText(
                QStringLiteral("已发送结束信号: %1 (PID: %2)").arg(name).arg(pid));
            // 延迟刷新列表，等待进程退出
            QTimer::singleShot(500, this, &ProcessManagerWidget::refreshProcessList);
        } else {
            m_statusLabel->setText(
                QStringLiteral("结束失败: %1 (PID: %2) — 权限不足或进程已退出").arg(name).arg(pid));
        }
    }
}

QString ProcessManagerWidget::formatMemSize(qint64 bytes) const
{
    if (bytes < 1024)         return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024)  return QString::number(bytes / 1024) + " KB";
    if (bytes < 1024LL * 1024 * 1024)
        return QString::number(bytes / (1024 * 1024)) + " MB";
    return QString::number(bytes / (1024LL * 1024 * 1024), 'f', 1) + " GB";
}
