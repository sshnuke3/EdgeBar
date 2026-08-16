# EdgeBar 概要设计文档

| 项目 | 内容 |
|------|------|
| 项目名称 | EdgeBar（桌边栏） |
| 文档版本 | v2.0.0 |
| 编写日期 | 2025-08-16 |
| 对应需求文档 | requirements.md v1.0.0 |

---

## 1. 系统架构

### 1.1 架构总览

EdgeBar 采用分层的多模块架构，自底向上分为核心层（core）、插件层（plugins）、搜索层（search）和表现层（ui），各层之间通过明确的接口与信号槽机制解耦。

```
┌──────────────────────────────────────────────────────────────────┐
│                          表现层 (ui)                              │
│  MainWindow · SystemMonitorWidget · ClipboardWidget             │
│  QuickLaunchWidget · PomodoroWidget                             │
│  ProcessManagerWidget · MiniCountdown · DesktopWidget           │
│  HealthReminderWidget                                           │
├──────────────────────────────────────────────────────────────────┤
│                        搜索层 (search)                            │
│  GrandSearchAdaptor                                             │
├──────────────────┬───────────────────────────────────────────────┤
│  插件层 (plugins) │              核心层 (core)                      │
│  ISearchPlugin   │  SystemMonitor · ClipboardManager            │
│  AppLauncher     │  SearchEngine · NotificationManager          │
│  SystemCommand   │  AutostartManager · IconHelper · Logging     │
├──────────────────┴───────────────────────────────────────────────┤
│                 Qt / DTK / Linux 内核接口                         │
│  /proc  /proc/[pid]/stat  /proc/pressure  /sys  QClipboard      │
│  QProcess  DConfig  DBus  dde-grand-search                        │
└──────────────────────────────────────────────────────────────────┘
```

### 1.2 分层职责

| 层次 | 目录 | 职责 |
|------|------|------|
| 核心层 | `src/core/` | 提供数据采集与算法实现，不依赖 UI 框架 |
| 插件层 | `src/plugins/` | 定义搜索插件接口并实现内置插件，可独立扩展 |
| 搜索层 | `src/search/` | dde-grand-search DBus 适配器，实现全局搜索插件协议 |
| 表现层 | `src/ui/` | 负责界面布局、自绘图表、交互逻辑与边缘动画 |

---

## 2. 模块划分与职责

### 2.1 核心层模块

**SystemMonitor（系统监控）**：定时读取 `/proc/stat`、`/proc/meminfo`、`/proc/net/dev`、`/sys/class/thermal` 采集系统资源数据。通过 `start(intervalMs)` 启动定时采集，`Q_PROPERTY` 暴露各项属性。`statsUpdated()` 信号在每次采集完成后发出，驱动 UI 刷新。维护 CPU、网络上下行各 60 个采样点的历史缓冲。

**ClipboardManager（剪贴板管理）**：监听 `QClipboard::dataChanged` 信号，经防抖处理后保存历史记录。提供 `items()`、`filteredItems(keyword)`、`togglePin(id)`、`copyToClipboard(id)` 等接口。`ClipItem` 结构体包含 `id`、`text`、`preview`、`pinned`、`timestamp` 字段。

**SearchEngine（搜索引擎）**：提供静态方法 `fuzzyScore()` 和 `containsMatch()`，实现子序列模糊匹配与评分。无状态、纯函数设计，可被任意插件复用。

**NotificationManager（通知管理）**：桌面通知管理，支持 D-Bus freedesktop 通知协议（`org.freedesktop.Notifications`）并附带提示音效，负责番茄钟完成、健康提醒等场景的通知发送。

**AutostartManager（自启管理）**：管理 XDG 自启目录（`~/.config/autostart`）下的 `.desktop` 文件，提供开机自启的启用与禁用，避免污染系统级自启配置。

**IconHelper（图标助手）**：基于 DTK 图标引擎进行主题图标解析，将图标名或文件路径统一转换为可在控件中绘制的 `QIcon`，支持高分辨率与主题切换。

**Logging（日志）**：基于 `Q_LOGGING_CATEGORY` 提供分类日志输出，按模块（core/plugins/search/ui）分别打标，便于调试与问题定位。

### 2.2 插件层模块

**ISearchPlugin（插件接口）**：定义搜索插件的抽象接口，规范 `id()`、`name()`、`shouldActivate()`、`search()`、`activate()` 五个方法，支持第三方扩展。

**AppLauncher（应用启动器）**：扫描 `/usr/share/applications` 和 `~/.local/share/applications` 下 `.desktop` 文件构建应用列表。对 `Name` 和 `Comment` 分别评分取较高值，返回前 8 条结果。

**SystemCommand（系统命令）**：提供 9 条内置系统命令（锁屏、注销、待机、休眠、重启、关机、控制中心、终端、文件管理器），使用 `loginctl`、`systemctl`、`qdbus` 等 deepin 原生工具。

### 2.3 表现层模块

| 模块 | 基类 | 职责 |
|------|------|------|
| MainWindow | DMainWindow | 主窗口容器，管理 Tab 切换、边缘动画、毛玻璃效果 |
| SystemMonitorWidget | DWidget | 自绘 CPU 仪表盘、内存进度条、网络曲线图、温度指示 |
| ClipboardWidget | DWidget | 搜索框 + 历史列表，自定义委托绘制条目 |
| QuickLaunchWidget | DWidget | 搜索框 + 结果列表，注册并调度搜索插件 |
| PomodoroWidget | DWidget | 番茄钟自绘圆形进度环与控制按钮 |
| ProcessManagerWidget | DDialog | 进程列表表格，支持选中进程并发送 kill 信号终止 |
| MiniCountdown | QWidget | 面板隐藏时的浮动迷你倒计时窗口 |
| DesktopWidget | QWidget | 6 模式浮动桌面小部件（CPU/内存/网络/时钟/番茄/饮水） |
| HealthReminderWidget | DWidget | 饮水与久站提醒间隔设置 |

### 2.4 搜索层模块

**GrandSearchAdaptor（全局搜索适配器）**：继承自 `QDBusAbstractAdaptor`，遵循 dde-grand-search V1.0 插件协议对外暴露 `Search()`、`Stop()`、`Action()` 三个方法。`Search()` 接收搜索词后同步检索应用、系统命令与剪贴板历史三类数据源，将结果聚合为 JSON 返回；`Stop()` 中止当前搜索；`Action()` 在用户点击结果时执行对应插件（应用启动、命令执行或剪贴板写入）。

---

## 3. 数据流设计

### 3.1 系统监控数据流

```
/proc/stat, /proc/meminfo, /proc/net/dev, /sys/class/thermal
    │  SystemMonitor::poll()  (定时器 2000ms)
    ▼
readCpuStat / readMemInfo / readNetDev / readTemperature
    │  emit statsUpdated()
    ▼
SystemMonitorWidget::onStatsUpdated() ──▶ update() ──▶ QPainter 自绘图表
```

### 3.2 剪贴板数据流

```
QClipboard::dataChanged ──▶ 防抖定时器启动(300ms, 重新计时)
    │  超时后触发
    ▼
onClipboardChanged()
    ├── 空文本/重复? ──是──▶ 跳过
    ├── 已存在相同文本? ──是──▶ 移至列表顶部
    └── addItem() ──▶ 超出 50 条淘汰最旧非置顶 ──▶ emit historyChanged()
                                                              │
                                                              ▼
                                               ClipboardWidget::refreshList()
```

### 3.3 快速启动数据流

```
用户输入 ──▶ QuickLaunchWidget::performSearch(query)
    │  遍历已注册插件 (AppLauncher, SystemCommand)
    ▼
plugin->shouldActivate(query)? ──false──▶ 跳过
    │ true
    ▼
plugin->search(query) ──▶ SearchEngine::fuzzyScore() 评分
    │  合并所有结果，按 score 降序排序
    ▼
用户点击 ──▶ plugin->activate(result) ──▶ QProcess::startDetached()
```

### 3.4 番茄钟数据流

```
点击"开始" ──▶ QTimer(1000ms) ──▶ onTick() ──▶ m_remaining--
    │
    ├── remaining > 0? ──是──▶ update() 重绘进度环
    └── remaining == 0?
        ├── 专注结束 ──▶ 切换至休息(5min 或 15min)
        └── 休息结束 ──▶ 切换至专注(25min), m_focusCount++
```

### 3.5 进程管理数据流

```
SystemMonitor::topCpuProcesses() ──▶ ProcessManagerWidget::refreshProcessList()
    │
    ▼
QTableWidget 展示进程列表（PID/名称/CPU/内存）
    │
    ▼
用户选中进程并点击"结束" ──▶ SystemMonitor::killProcess(pid, SIGTERM) ──▶ emit processKilled(pid, name)
```

### 3.6 全局搜索数据流

```
dde-grand-search-daemon ──▶ DBus 调用 ──▶ GrandSearchAdaptor::Search(query)
    │
    ▼
检索应用 / 系统命令 / 剪贴板历史三类数据源
    │
    ▼
聚合结果为 JSON 返回给 dde-grand-search-daemon 展示
    │
    ▼
用户点击结果 ──▶ GrandSearchAdaptor::Action(itemPath)
```

### 3.7 桌面小部件数据流

```
SystemMonitor 信号 statsUpdated() ──▶ DesktopWidget::onTick()
    │
    ▼
根据当前模式（CPU/内存/网络/时钟/番茄/饮水）
    │
    ▼
update() ──▶ QPainter 按模式重绘对应图形
```

---

## 4. 接口设计

### 4.1 ISearchPlugin 插件接口

```cpp
class ISearchPlugin {
public:
    virtual ~ISearchPlugin() = default;
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual bool shouldActivate(const QString &query) const = 0;
    virtual QList<SearchResult> search(const QString &query) = 0;
    virtual void activate(const SearchResult &result) = 0;
};
```

### 4.2 SearchResult 数据结构

| 字段 | 类型 | 说明 |
|------|------|------|
| id | QString | 插件内唯一标识 |
| title | QString | 主标题 |
| subtitle | QString | 副标题 |
| iconName | QString | 主题图标名或文件路径 |
| pluginId | QString | 来源插件 ID |
| data | QVariant | 插件自定义数据（如 Exec 命令） |
| score | int | 相关度评分，越高越靠前 |

### 4.3 核心信号与 MainWindow 接口

| 类 | 信号/方法 | 说明 |
|------|-----------|------|
| SystemMonitor | `statsUpdated()` | 每次采集周期完成时发出 |
| SystemMonitor | `processKilled(pid, name)` | 通过 `killProcess()` 终止进程后发出 |
| SystemMonitor | `memPressureChanged(level)` | 内存压力等级变化时发出 |
| ClipboardManager | `historyChanged()` | 历史记录变更时发出 |
| MainWindow | `setEdgeSide(LeftEdge/RightEdge)` | 设置面板停靠边缘 |
| MainWindow | `setActiveTab(SystemTab/ClipboardTab/LaunchTab/PomodoroTab)` | 切换标签页 |

---

## 5. 技术选型说明

### 5.1 核心技术选型

| 技术领域 | 选型 | 选型理由 |
|----------|------|----------|
| 编程语言 | C++17 | 性能要求高，需直接访问 `/proc` 文件系统 |
| UI 框架 | Qt5 / Qt6 | 跨版本兼容，CMake 自动检测主版本号 |
| 控件库 | DTK (DWidget/DLineEdit/DListView) | deepin 原生控件，保证视觉一致性 |
| 构建系统 | CMake 3.16+ | 跨平台构建，支持 AUTOMOC 自动化 |
| 配置管理 | DConfig (dsg.config) | deepin 标准配置服务 |
| 打包 | Debian debhelper 13 | deepin 标准打包格式 |

### 5.2 关键技术决策

**自绘图表 vs 第三方图表库**：选择 `QPainter` 自绘。EdgeBar 图表需求简单（圆形仪表、进度条、曲线图），自绘完全满足，减少外部依赖，可精确控制 DTK 主题色适配。

**防抖定时器 vs 直接处理**：剪贴板监听采用 300ms 防抖。部分应用快速连续写入剪贴板，直接处理会产生冗余条目。防抖确保只在用户停止操作后处理最终值，实现简单（`QTimer::setSingleShot(true)`）。

**插件接口 vs 硬编码搜索**：采用 `ISearchPlugin` 接口。应用列表与系统命令的搜索逻辑独立，分别封装为插件降低耦合。接口预留扩展点，未来可添加文件搜索、网页搜索等插件。

**边缘检测定时器 vs 事件过滤器**：采用 300ms 定时器轮询鼠标位置。全局事件过滤器可能影响性能且需 X11/Wayland 兼容性处理。定时器方案简单可靠，CPU 开销可忽略。

**进程终止采用 SIGTERM 而非 SIGKILL**：`killProcess()` 使用 POSIX `kill(pid, SIGTERM)`，允许目标进程优雅退出并释放资源，而非 `SIGKILL` 强制终止。终止前弹出确认对话框，防止误操作结束关键进程。

**dde-grand-search 采用 Manual 模式**：插件以 Manual 模式注册，即始终被加载且同步执行搜索。相较于按需触发（Trigger）模式，Manual 模式实现更简单，更契合 EdgeBar 作为常驻工具的定位，且搜索数据源均为本地、延迟可忽略。

**桌面小部件作为独立 QWidget**：`DesktopWidget` 是独立浮动的 `QWidget`，使用 `Qt::WindowStaysOnTopHint | Qt::Tool` 窗口标志，未嵌入 `MainWindow`，因此当面板隐藏或退出边缘动画时仍可常驻桌面显示。

---

## 6. 依赖关系

### 6.1 编译依赖

| 依赖 | 用途 | 包名（Qt5 / Qt6） |
|------|------|---------------------|
| Qt Core | 基础类型、信号槽、文件 IO | qtbase5-dev / qt6-base-dev |
| Qt Widgets | QWidget 基础设施 | qtbase5-dev / qt6-base-dev |
| DTK Core | DLog 日志、DConfig 配置 | libdtkcore-dev / libdtkcore6-dev |
| DTK Gui | DPlatformHandle 平台特性 | libdtkgui-dev / libdtkgui6-dev |
| DTK Widget | DWidget 等控件 | libdtkwidget-dev / libdtkwidget6-dev |

### 6.2 运行时依赖

| 依赖 | 用途 |
|------|------|
| deepin-session-manager | 注销操作（qdbus） |
| systemd | 待机/休眠/重启/关机（systemctl） |
| loginctl | 锁屏（loginctl lock-session） |
| dde-control-center | 控制中心启动 |
| deepin-terminal | 终端启动 |
| dde-file-manager | 文件管理器启动 |
| dde-grand-search-daemon | 全局搜索集成（DBus 插件协议） |
| /proc 文件系统 | 进程信息与内存压力采集 |
