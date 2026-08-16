# EdgeBar 桌边栏 — 开发过程说明

| 项目 | 内容 |
|------|------|
| 参赛作品 | EdgeBar（桌边栏） |
| 参赛方向 | 方向三：DTK 原生应用开发 |
| GitHub | https://github.com/sshnuke3/EdgeBar |
| 版本 | v2.0.0 |
| 日期 | 2025-08-16 |

---

## 一、选题思路

### 1.1 痛点分析

deepin 桌面环境在日常使用中，以下场景频繁切换应用造成效率损失：

- 查看系统资源需打开系统监视器（重量级应用）
- 回溯剪贴板历史无原生方案（只能再次复制覆盖）
- 快速启动应用需回到桌面或打开启动器（全屏遮挡当前工作）
- 番茄工作法需安装第三方工具（不集成桌面环境）
- 系统卡顿时无法快速定位和结束高CPU进程（需打开系统监视器）
- 全局搜索无法搜索剪贴板历史和自定义命令
- 桌面缺少轻量级信息小组件（时钟/网速/CPU一览）

这七个需求都是"短暂查看/操作即离开"的高频动作，适合用轻量级边缘面板承载。

### 1.2 方案选型

在比赛指定的四个方向中，经过对比分析后选择**方向三（DTK 原生应用）**：

| 方向 | 开发成本 | 创新空间 | 评审竞争 | 最终选择 |
|------|----------|----------|----------|----------|
| 控制中心插件 | 中 | 低 | 少 | ✗ 受众窄 |
| 任务栏托盘插件 | 中高 | 中 | 中 | ✗ 接口复杂 |
| **DTK 原生应用** | **低** | **高** | **多** | **✓** |
| DDE Shell 扩展 | 低 | 中 | 少 | ✗ LayerShell 依赖性强 |

选择理由：DTK 原生应用开发自由度最高，可以实现完整的四模块集成，且 DTK 控件库和毛玻璃效果能保证与 deepin 桌面的视觉一致性。

### 1.3 参考资料利用

开发过程中参考了以下官方资料：

| 资料 | 用途 |
|------|------|
| [deepin-skills GitHub](https://github.com/linuxdeepin/deepin-skills) | 确认四个参赛方向的技术要求 |
| [uosdn 开发说明](https://uosdn.uniontech.com/#document2?dirid=6a5f5ae10787ec7a397ebc9c) | DTK 开发规范与打包流程 |
| [dtk-codeviewer 示例项目](https://github.com/linuxdeepin/deepin-skills/tree/master/examples/dtk-codeviewer) | 代码质量标杆、文档规范、DConfig 用法 |
| [论坛实战教程](https://bbs.deepin.org/post/300316) | 四个方向的实战代码参考 |
| [比赛原帖](https://bbs.deepin.org.cn/post/300665) | 评分标准与提交要求 |

特别地，dtk-codeviewer 示例项目携带了三份设计文档（requirements.md / outline-design.md / detailed-design.md），这直接指导了 EdgeBar 的文档编写规范。

---

## 二、开发流程

### 2.1 开发阶段总览

```
阶段1: 项目架构设计 ──▶ 阶段2: 核心层实现 ──▶ 阶段3: UI 层实现
                                                        │
阶段5: GitHub 推送 ◀── 阶段4: 编译打包 ◀─────────────────┘
        │
        ▼
阶段6: 打磨优化 ──▶ 阶段7: 功能迭代与差异化增强 ──▶ 阶段8: 文档与提交材料
```

### 2.2 阶段一：项目架构设计

**目标**：确定三层架构、模块划分和技术选型。

**决策记录**：

| 决策 | 选项 | 最终选择 | 理由 |
|------|------|----------|------|
| 架构分层 | 单层 / 两层 / 三层 | 三层（core/plugins/ui） | 核心逻辑与 UI 解耦，插件可扩展 |
| 图表方案 | 第三方库 / QPainter 自绘 | QPainter 自绘 | 依赖少、图表简单、主题色可控 |
| 搜索方案 | 精确匹配 / 模糊匹配 | 子序列模糊匹配 + 评分 | 容错性好，用户体验优于精确匹配 |
| 配置方案 | QSettings / DConfig | DConfig | deepin 标准配置服务，评审加分项 |
| 打包方式 | make / dpkg-buildpackage | dpkg-buildpackage + debhelper 13 | deepin 标准打包流程 |
| Qt 版本 | Qt5 / Qt6 | 双版本兼容 | CMake 自动检测，覆盖 deepin 20/23 |

**关键设计**：ISearchPlugin 插件接口。定义 `id()`、`name()`、`shouldActivate()`、`search()`、`activate()` 五个抽象方法，内置 AppLauncher 和 SystemCommand 两个实现。该设计复用了此前开发的 GlobalLauncher 项目代码，经过验证的插件架构直接迁移。

### 2.3 阶段二：核心层实现

**SystemMonitor — 系统资源采集**

通过读取 Linux 标准 `/proc` 文件系统实现：

| 数据 | 数据源 | 算法 |
|------|--------|------|
| CPU 使用率 | `/proc/stat` 第一行 | `(diffTotal - diffIdle) * 100 / diffTotal` |
| 内存使用率 | `/proc/meminfo` | `(MemTotal - MemAvailable) * 100 / MemTotal` |
| 网络速率 | `/proc/net/dev` | `bytes差值 / 时间差 / 1024` (KB/s) |
| 温度 | `/sys/class/thermal/thermal_zone0/temp` | `rawValue / 1000` (°C) |

维护 60 个采样点的历史缓冲用于绘制趋势曲线。温度读取提供双路径回退，全部失败时安全置零。

**ClipboardManager — 剪贴板历史管理**

关键算法 — 防抖去重：

```
QClipboard::dataChanged
    │
    ▼
m_debounceTimer.start(300ms)  ◀── 300ms 内再次变化则重新计时
    │  超时
    ▼
onClipboardChanged()
    ├── 空文本或与上次相同 ──────▶ 跳过
    ├── 已存在相同文本 ──────────▶ 移至列表顶部（去重）
    └── 新文本 ──▶ addItem() ──▶ 超出 50 条淘汰最旧非置顶
```

防循环处理：点击复制到剪贴板时用 `blockSignals(true/false)` 包裹 `setText()`，避免触发 dataChanged 导致重复添加。

**SearchEngine — 模糊搜索引擎**

子序列匹配 + 多维度评分：

| 评分规则 | 分值 | 条件 |
|----------|------|------|
| 完全匹配 | 100 + 字符数 | query == target |
| 基础分 | +1/字符 | 每个匹配的字符 |
| 连续匹配 | +5 | 匹配位置紧接上一次之后 |
| 首字母匹配 | +10 | query 首字符匹配 target 首字符 |
| 词边界匹配 | +8 | 匹配位置在开头或非字母数字之后 |

例如查询 "term" 匹配 "Terminal"：首字母(+10) + 4 个连续匹配(+20) + 基础分(4) = 34 分。

### 2.4 阶段三：UI 层实现

**MainWindow — 边缘滑出面板**

- 窗口属性：无边框 + 置顶 + Tool 类型，280x640 固定尺寸
- DTK 毛玻璃：`DPlatformHandle` 设置 18px 圆角、模糊背景、阴影
- 边缘检测：300ms 定时器轮询鼠标位置，靠近边缘 4px 范围触发滑入
- 滑动动画：`QPropertyAnimation` 200ms `OutCubic` 缓动

**SystemMonitorWidget — 纯 QPainter 自绘图表**

四个绘制区域，全部使用 `QPainter` 自绘：

| 图表 | 绘制方式 |
|------|----------|
| CPU 仪表盘 | `drawArc()` 圆环 + 颜色分级(绿/橙/红) + 历史曲线 `QPainterPath` |
| 内存进度条 | `drawRoundedRect()` 填充条 + 颜色分级 + 数值文本 |
| 网络流量图 | 双 `QPainterPath` 曲线 + 下载蓝色填充 + 上传绿色线条 |
| 温度指示 | 大号数值 + 温度计风格进度条 |

**ClipboardWidget — 剪贴板历史列表**

自定义 `QStyledItemDelegate` 绘制每条记录：预览文本 + 时间戳 + 置顶标记。搜索框实时过滤，点击即复制。

**PomodoroWidget — 番茄钟**

圆形进度环 + 倒计时数字，专注红色/休息绿色区分。状态机：Idle → Focus(25min) → Break(5min) → 循环，每 4 轮进入 LongBreak(15min)。

### 2.5 阶段四：编译与打包

**编译环境**：

```
OS:         Debian-based (deepin container)
Compiler:   g++ (C++17)
Qt:         Qt 5.15.2
DTK:        DTK5 (libdtkcore5/libdtkgui5/libdtkwidget5)
CMake:      3.16+
```

**遇到的编译问题与解决**：

| 问题 | 原因 | 解决方案 |
|------|------|----------|
| `DPushButton` 前向声明失败 | DTK5 中 DPushButton 是 typedef 非 class | 改为 `#include <DPushButton>` |
| `DConfig` 命名空间不匹配 | DConfig 在 `Dtk::Core` 命名空间下 | 使用 `Dtk::Core::DConfig` |
| `DConfig` 构造函数参数不符 | DTK5 API 签名与预期不同 | 调整为三参数构造 `DConfig("edgebar", QString(), this)` |
| `QPropertyAnimation` 前向声明失败 | 需要 Complete type | 添加 `#include <QPropertyAnimation>` |
| LinguistTools 包缺失 | 构建环境未安装 qttools5-dev | CMake `find_package QUIET` + 条件编译 |

**Debian 打包**：

使用 `dpkg-buildpackage -us -uc -b -d` 构建 .deb 包。打包配置包括：
- `debian/control` — 双依赖声明（`qtbase5-dev | qt6-base-dev`）
- `debian/rules` — CMake 构建参数传递
- `debian/changelog` — 版本历史
- `debian/source/format` — `3.0 (quilt)`

### 2.6 阶段五：GitHub 推送

源码推送到 https://github.com/sshnuke3/EdgeBar ，包含完整项目结构和提交历史。

### 2.7 阶段六：打磨优化

对照评分标准和 dtk-codeviewer 示例标杆，进行了以下打磨：

| 改进项 | Before | After | 评分维度 |
|--------|--------|-------|----------|
| 设计文档 | 无 | 三份完整文档（需求/概要/详细） | 代码质量 +3~4 分 |
| DConfig | JSON 写了代码没读 | 真正运行时读取 4 项配置 | 功能完整性 +1 分 |
| Tab 图标 | 纯文字 | 图标 + 文字 + tooltip | UI/UX +1 分 |
| 键盘快捷键 | 无 | Esc 隐藏 / Tab 切换 / Ctrl+Tab 反向 | UI/UX +0.5 分 |
| 深色主题 | 亮色硬编码 | 深浅色自适应 Tab 样式 | UI/UX +0.5 分 |
| Qt 日志 | qWarning 无分类 | `Q_LOGGING_CATEGORY(edgebarLog)` | 代码质量 +0.5 分 |
| 国际化 | 无 | `edgebar_zh_CN.ts` 翻译文件 | 代码质量 +0.5 分 |

### 2.8 阶段七：功能迭代与差异化增强

在打磨优化阶段完成后，针对用户反馈和竞品对比，进行了第三轮功能迭代，新增三个差异化功能模块：

**功能1：进程管理器**

| 维度 | 说明 |
|------|------|
| 差异化 | DeskMon 等竞品仅监控进程，EdgeBar 可直接结束进程 |
| 实现 | `SystemMonitor::topCpuProcesses()` 遍历 `/proc/[pid]/stat` 计算单进程 CPU%，`ProcessManagerWidget` 以 DDialog 展示表格，`killProcess(pid)` 通过 POSIX kill 发送 SIGTERM |
| 来源 | [系统监视器建议帖](https://bbs.deepin.org/phone/post/300480) |

**功能2：全局搜索插件**

| 维度 | 说明 |
|------|------|
| 方向 | 基于 dde-grand-search 框架开发搜索插件 |
| 实现 | `GrandSearchAdaptor` 实现 `QDBusAbstractAdaptor`，遵循 V1.0 协议（Search/Stop/Action），搜索范围覆盖应用/系统命令/剪贴板历史 |
| 配置文件 | `edgebar-search.conf` 安装到 dde-grand-search-daemon 插件目录 |
| 参考 | [dde-grand-search 插件开发教程](https://bbs.deepin.org/phone/post/233965) |

**功能3：桌面小组件增强**

| 维度 | 说明 |
|------|------|
| 实现 | `DesktopWidget` 独立悬浮窗，6 种模式（CPU仪表/内存仪表/网络监控/时钟/番茄钟/喝水提醒） |
| 交互 | 右键切换模式，1 秒刷新，半透明毛玻璃背景 |
| 来源 | 用户和版主对桌面小组件的需求反馈 |

---

## 三、技术亮点

### 3.1 纯自绘图表

系统监控和番茄钟的全部可视化均使用 `QPainter` 自绘，零第三方图表依赖：

- CPU 圆形仪表盘：`drawArc()` + `QPainterPath` 历史曲线
- 网络双曲线图：`QPainterPath` + 透明填充
- 温度计指示条：`drawRoundedRect()` + 颜色分级
- 番茄钟进度环：`drawArc()` + 角度计算

优势：减少依赖、精确控制 DTK 主题色适配、安装包体积小。

### 3.2 插件架构

`ISearchPlugin` 接口实现了搜索功能的可扩展架构：

```cpp
class ISearchPlugin {
    virtual QString id() const = 0;
    virtual QString name() const = 0;
    virtual bool shouldActivate(const QString &query) const = 0;
    virtual QList<SearchResult> search(const QString &query) = 0;
    virtual void activate(const SearchResult &result) = 0;
};
```

内置两个插件：`AppLauncher`（扫描 .desktop 文件）和 `SystemCommand`（9 条系统命令）。第三方开发者可实现该接口添加文件搜索、网页搜索等扩展。

### 3.3 DConfig 持久化配置

8 项配置通过 deepin 标准 DConfig 服务持久化：

| 配置键 | 类型 | 默认值 | 作用 |
|--------|------|--------|------|
| `edgeSide` | string | "right" | 面板停靠边缘 |
| `autoHide` | bool | true | 自动隐藏开关 |
| `monitorInterval` | int | 2000 | 监控采样间隔(ms) |
| `maxClipboardItems` | int | 50 | 剪贴板历史上限 |
| `enableClipboardImages` | bool | true | 剪贴板图片历史开关 |
| `netSpeedUnit` | string | "byte" | 网速单位: byte(KB/s) / bit(kbps) |
| `cpuAlertThreshold` | int | 80 | CPU 进程告警阈值(%, 0=禁用) |
| `trafficThresholdMB` | int | 0 | 每日流量预警阈值(MB, 0=禁用) |

运行时通过 `DConfig("edgebar")` 读取，配置文件安装至 `/usr/share/dsg/configs/edgebar.json`。

### 3.4 双版本兼容

CMake 自动检测 Qt 版本，适配 DTK5/DTK6：

```cmake
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
if(QT_VERSION_MAJOR EQUAL 6)
    set(DTK_SUFFIX 6)
    set(DTK_LIBS Dtk6::Core Dtk6::Gui Dtk6::Widget)
else()
    set(DTK_SUFFIX "")
    set(DTK_LIBS ${DtkWidget_LIBRARIES})
endif()
```

### 3.5 防抖去重算法

剪贴板监听采用 300ms 防抖定时器，支持文本和图片双重去重：
- 文本去重：与 `m_lastText` 比较，相同则跳过
- 图片去重：将图片降采样至 64×64 后做 MD5 hash，与 `m_lastImageHash` 比较，相同则跳过

### 3.6 剪贴板图片支持

剪贴板历史支持文本和图片两种类型（来源：[deepin 12月需求池](https://bbs.deepin.org.cn/phone/zh/post/294790) — "剪贴板图片多次编辑"）：

| 功能 | 实现 |
|------|------|
| 图片监听 | `QMimeData::hasImage()` 检测，`QClipboard::image()` 读取 |
| 缩略图生成 | `QImage::scaled()` 保持比例缩至 120×80 |
| 图片去重 | 64×64 降采样 → PNG 编码 → MD5 hash 比较 |
| 缩略图渲染 | `ClipItemDelegate::drawImageItem()` QPainter 自绘 |
| 点击复制 | `QClipboard::setPixmap()` 写回剪贴板 |
| 右键菜单 | 置顶/删除操作，文本图片通用 |

### 3.7 网速单位切换

来源：[deepin 4月需求池](https://www.deepin.org/zh/user-issues-requirement-feedback-progress-sync-202604/) — 网速单位 bit→Byte 切换。

`formatSpeed()` 根据 `m_useByteUnit` 标志切换显示：
- Byte 模式：`KB/s` → `MB/s`
- Bit 模式：`KB/s × 8 = kbps` → `Mbps`

### 3.8 CPU 高占用进程告警

来源：[系统监视器建议帖](https://bbs.deepin.org/phone/post/300480) — CPU 高占用持续时长过滤。

| 功能 | 实现 |
|------|------|
| 进程扫描 | 遍历 `/proc/[pid]/stat`，解析 utime+stime |
| CPU 计算 | `(procTime增量 / totalJiffies增量) × 100` |
| 告警阈值 | DConfig `cpuAlertThreshold`（0=禁用） |
| 告警展示 | 顶部橙色告警条：进程名 + CPU% |
| 进程清理 | map 超过 500 条时淘汰最旧 100 条 |

### 3.9 番茄钟任务标签

来源：[Pomodoro 自荐帖](https://bbs.deepin.org/phone/post/300072) — 番茄钟+待办任务结合。

| 功能 | 实现 |
|------|------|
| 任务输入 | DLineEdit 输入当前任务标签 |
| 任务锁定 | 专注开始时锁定标签，期间不可修改 |
| Session 记录 | 专注结束时记录 `{task, timestamp}` |
| 历史展示 | 底部显示最近 3 条完成记录（时间+任务名） |
| 任务标签框 | 圆环下方带背景色的任务标签 |

### 3.10 QuickLaunch 右键系统快捷操作

来源：[12月需求池](https://bbs.deepin.org.cn/phone/zh/post/294790) — 任务栏右键功能增强。

| 功能 | 实现 |
|------|------|
| 锁屏/注销/挂起/重启/关机 | `loginctl` / `systemctl` 命令 |
| 亮度调节 | `/sys/class/backlight/` 读写，fallback DBus |
| 菜单入口 | 列表区域右键弹出 |

### 3.11 历史数据导出 CSV

来源：[定时保存资源使用情况帖](https://bbs.deepin.org/post/267038)。

`SystemMonitor` 每次采样记录 `Snapshot` 结构体（时间戳/CPU/内存/网络/温度），上限 360 条（12分钟@2s）。右键系统监控组件弹出菜单，选择"导出历史数据"保存为 CSV 文件。

### 3.12 流量超额预警

来源：[lfxNet 推荐帖](https://bbs.deepin.org/zh/post/213210) — 流量预警功能。

`SystemMonitor` 累计当日上传+下载流量，跨天自动清零。当累计流量超过 DConfig `trafficThresholdMB` 阈值时，系统监控顶部显示红色告警条。

### 3.13 番茄钟迷你倒计时浮窗

来源：[番茄钟需求帖](https://bbs.deepin.org/phone/post/147835) — 任务栏/dock 显示倒计时。

面板隐藏时，在屏幕边缘显示 24×50px 的迷你浮窗，竖排显示 MM/SS 倒计时。专注模式红色背景，休息模式绿色背景。点击浮窗触发面板滑入。

### 3.14 进程管理器

`SystemMonitor::topCpuProcesses()` 遍历 `/proc/[pid]/stat` 计算单进程 CPU 占用率，`killProcess()` 使用 SIGTERM 优雅终止进程，`ProcessManagerWidget` 提供双击行和按钮两种结束进程方式。

### 3.15 全局搜索插件

`GrandSearchAdaptor` 实现 dde-grand-search V1.0 协议，采用 Manual 模式同步搜索，提供 Search/Stop/Action 三个方法，JSON 数据格式严格遵循协议规范，搜索结果覆盖应用、系统命令和剪贴板历史。

### 3.16 桌面小组件

`DesktopWidget` 支持 6 种模式切换（CPU仪表/内存仪表/网络监控/时钟/番茄钟/喝水提醒），每种模式使用 QPainter 自绘，独立于 MainWindow 持续显示，右键即可切换模式。

---

## 四、项目结构

```
EdgeBar/
├── src/
│   ├── core/                        # 核心层（无 UI 依赖）
│   │   ├── SystemMonitor.h/.cpp     # /proc 读取 CPU/内存/网络/温度
│   │   ├── ClipboardManager.h/.cpp  # 剪贴板历史管理
│   │   ├── SearchEngine.h/.cpp      # 模糊搜索评分
│   │   ├── NotificationManager.h/.cpp  # 系统通知管理
│   │   ├── AutostartManager.h/.cpp  # 开机自启管理
│   │   ├── IconHelper.h             # 图标资源辅助类
│   │   └── Logging.h/.cpp           # Qt 日志分类
│   ├── search/                      # 全局搜索层（dde-grand-search 集成）
│   │   └── GrandSearchAdaptor.h/.cpp  # dde-grand-search V1.0 协议适配器
│   ├── plugins/                     # 插件层（可扩展）
│   │   ├── ISearchPlugin.h          # 插件接口
│   │   ├── AppLauncher.h/.cpp       # .desktop 应用启动器
│   │   └── SystemCommand.h/.cpp     # 系统命令
│   ├── ui/                          # 表现层
│   │   ├── MainWindow.h/.cpp       # 边缘面板 + Tab + DConfig
│   │   ├── SystemMonitorWidget.h/.cpp  # QPainter 自绘图表
│   │   ├── ClipboardWidget.h/.cpp    # 剪贴板历史列表
│   │   ├── QuickLaunchWidget.h/.cpp # 搜索 + 结果列表
│   │   ├── PomodoroWidget.h/.cpp     # 番茄钟
│   │   ├── ProcessManagerWidget.h/.cpp  # 进程管理器（查看/结束进程）
│   │   ├── DesktopWidget.h/.cpp     # 桌面小组件（6 模式悬浮窗）
│   │   ├── HealthReminderWidget.h/.cpp  # 健康提醒（喝水/久坐）
│   │   └── MiniCountdown.h/.cpp     # 番茄钟迷你倒计时浮窗
│   └── main.cpp                     # 入口
├── plugins/
│   └── edgebar-search.conf          # dde-grand-search 插件配置
├── tests/                           # 单元测试
│   ├── tst_systemmonitor.cpp        # SystemMonitor 测试
│   ├── tst_clipboardmanager.cpp     # ClipboardManager 测试
│   ├── tst_searchengine.cpp         # SearchEngine 测试
│   ├── tst_applauncher.cpp          # AppLauncher 插件测试
│   ├── tst_systemcommand.cpp        # SystemCommand 插件测试
│   └── tst_pomodoro.cpp             # 番茄钟逻辑测试
├── docs/                            # 设计文档
│   ├── requirements.md              # 需求规格说明书
│   ├── outline-design.md            # 概要设计文档
│   ├── detailed-design.md           # 详细设计文档
│   └── development-process.md       # 开发过程说明
├── configs/edgebar.json             # DConfig 配置
├── translations/edgebar_zh_CN.ts    # 中英文翻译
├── debian/                          # Debian 打包
│   ├── control
│   ├── rules
│   ├── changelog
│   └── source/format
├── CMakeLists.txt                   # CMake 构建
├── build.sh                         # 构建脚本
├── edgebar.desktop                  # 桌面入口
├── .gitignore
└── README.md
```

**代码统计**：

| 维度 | 数量 |
|------|------|
| 源文件(.h/.cpp) | 39 个 |
| 代码行数 | ~7500 行 |
| 设计文档 | 4 份（1000+ 行） |
| 功能模块 | 7 个 |
| 内置插件 | 2 个 |
| DConfig 配置项 | 15 项 |
| 单元测试 | 6 个（~1350 行测试代码） |

---

## 五、功能演示

### 5.1 使用方式

1. 安装 .deb 包后，EdgeBar 自动注册到系统应用菜单
2. 启动后面板隐藏在屏幕右边缘
3. 鼠标移至屏幕右边缘 4px 范围，面板自动滑出
4. 顶部 Tab 切换四个功能模块

### 5.2 功能清单

| 模块 | 功能 | 操作方式 |
|------|------|----------|
| 系统监控 | CPU 圆形仪表盘 + 历史曲线 | 自动刷新 |
| 系统监控 | CPU 进程告警条 | 阈值告警 |
| 系统监控 | 内存进度条 + 数值 | 自动刷新 |
| 系统监控 | 网络双曲线图 + 速率 | 自动刷新 |
| 系统监控 | 网速单位切换 KB/s / kbps | DConfig 切换 |
| 系统监控 | 温度指示 | 自动刷新 |
| 系统监控 | CSV 历史数据导出 | 右键菜单 |
| 系统监控 | 流量超额预警 | 阈值告警 |
| 系统监控 | 进程管理器（查看/结束进程） | 右键监控页 |
| 剪贴板 | 自动保存历史（50 条） | 后台监听 |
| 剪贴板 | 图片历史（截图/复制图片） | 后台监听 |
| 剪贴板 | 搜索过滤（文本+图片） | 输入关键词 |
| 剪贴板 | 点击复制 | 点击条目 |
| 剪贴板 | 置顶管理 | 右键菜单 |
| 剪贴板 | 删除单条 | 右键菜单 |
| 快速启动 | 模糊搜索应用 | 输入关键词 |
| 快速启动 | 系统命令 | 输入关键词 |
| 快速启动 | 右键系统快捷操作 | 右键菜单 |
| 快速启动 | 锁屏/注销/挂起/重启/关机 | 右键菜单 |
| 快速启动 | 屏幕亮度调节 | 右键子菜单 |
| 快速启动 | 点击执行 | 点击结果 |
| 番茄钟 | 25分钟专注 + 5分钟休息循环 | 自动切换 |
| 番茄钟 | 任务标签绑定 | 输入框设置 |
| 番茄钟 | 专注历史记录 | 最近3条 |
| 番茄钟 | 长休息（第4个后15分钟） | 自动检测 |
| 番茄钟 | 开始/暂停/重置 | 按钮控制 |
| 番茄钟 | 番茄计数 | 完成累计 |
| 番茄钟 | 迷你倒计时浮窗 | 面板隐藏时 |
| 桌面小组件 | CPU/内存/网络/时钟/番茄/喝水 6模式 | 右键切换 |
| 全局搜索 | dde-grand-search 集成 | 系统搜索框 |
| 全局搜索 | 搜索应用/命令/剪贴板 | 输入关键词 |
| 全局搜索 | 点击结果执行操作 | 点击结果 |
| 健康提醒 | 喝水提醒 | 定时通知 |
| 健康提醒 | 久坐提醒 | 定时通知 |
| 全局 | Esc 隐藏面板 | 键盘 |
| 全局 | Tab 切换模块 | 键盘 |
| 全局 | Ctrl+Tab 反向切换 | 键盘 |

---

## 六、对照评分标准自评

| 评分维度 | 权重 | 自评 | 依据 |
|----------|------|------|------|
| 功能完整性 | 30% | 10/10 | 7 大模块 60+ 功能点，剪贴板图片+搜索+右键菜单，网速切换，CPU告警，CSV导出，流量预警，右键系统操作，番茄钟任务标签+迷你浮窗，进程管理器，全局搜索，桌面小组件 |
| 真实使用价值 | 20% | 10/10 | 全部功能来自 deepin 论坛真实用户需求，8+ 个需求帖一一对应 |
| 创新性 | 15% | 9/10 | 四合一边缘面板 + 纯自绘图表 + 插件架构 + 图片去重hash + 进程告警 + 番茄钟任务追踪 + 迷你浮窗 + 进程结束 + dde-grand-search 集成 + 6 模式桌面小组件 |
| UI/UX 设计 | 15% | 9/10 | DTK 毛玻璃 + 图标Tab + 键盘快捷键 + 主题自适应 + 右键菜单 + 双告警条 + 迷你浮窗 + 桌面小组件 + 进程管理器对话框 |
| 代码质量 | 20% | 9/10 | 三层架构 + 四份设计文档 + 日志分类 + 国际化 + 6 个单元测试 + Q_LOGGING_CATEGORY |
| **加权总分** | 100% | **~92** | |

---

## 七、开发环境与工具

| 工具 | 用途 |
|------|------|
| TraeWork (Code mode) | AI 辅助编码、架构设计、文档生成 |
| CMake 3.16+ | 构建系统 |
| g++ (C++17) | 编译器 |
| Qt 5.15 + DTK5 | UI 框架与控件库 |
| dpkg-buildpackage | Debian 打包 |
| Git + GitHub | 版本控制与源码托管 |

### 使用的 deepin Skills

| Skill | 用途 |
|-------|------|
| `dtk-development` | DTK 开发规范参考（通过 deepin-skills 仓库学习），全程遵循 skill 规范：DApplication 用法、DConfig 配置模式、DLogManager 日志记录、DDBusSender 调用 deepin Appearance 接口 |
| `dde-control-center-development` | 控制中心方向了解（对比后未选此方向） |
| `dde-tray-development` | 托盘插件方向了解（对比后未选此方向） |
| `dde-shell-development` | DDE Shell 扩展方向了解（对比后未选此方向） |

开发过程中通过参考 deepin-skills 仓库的 dtk-codeviewer 示例项目，学习了 DTK 应用的代码规范、文档结构和 DConfig 用法。dtk-development skill 的各项规范贯穿整个开发周期：使用 DApplication 管理应用生命周期与单实例，采用 DConfig 标准模式读写配置，通过 DLogManager 进行结构化日志输出，并使用 DDBusSender 与 deepin Appearance 接口交互实现主题适配。
