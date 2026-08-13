# EdgeBar 桌边栏 — 开发过程说明

| 项目 | 内容 |
|------|------|
| 参赛作品 | EdgeBar（桌边栏） |
| 参赛方向 | 方向三：DTK 原生应用开发 |
| GitHub | https://github.com/sshnuke3/EdgeBar |
| 版本 | v1.1.0 |
| 日期 | 2025-08-13 |

---

## 一、选题思路

### 1.1 痛点分析

deepin 桌面环境在日常使用中，以下场景频繁切换应用造成效率损失：

- 查看系统资源需打开系统监视器（重量级应用）
- 回溯剪贴板历史无原生方案（只能再次复制覆盖）
- 快速启动应用需回到桌面或打开启动器（全屏遮挡当前工作）
- 番茄工作法需安装第三方工具（不集成桌面环境）

这四个需求都是"短暂查看/操作即离开"的高频动作，适合用轻量级边缘面板承载。

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
阶段6: 打磨优化 ──▶ 阶段7: 文档与提交材料
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

4 项配置通过 deepin 标准 DConfig 服务持久化：

| 配置键 | 类型 | 默认值 | 作用 |
|--------|------|--------|------|
| `edgeSide` | string | "right" | 面板停靠边缘 |
| `autoHide` | bool | true | 自动隐藏开关 |
| `monitorInterval` | int | 2000 | 监控采样间隔(ms) |
| `maxClipboardItems` | int | 50 | 剪贴板历史上限 |

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

剪贴板监听采用 300ms 防抖定时器 + 文本去重，避免快速连续复制产生冗余条目。

---

## 四、项目结构

```
EdgeBar/
├── src/
│   ├── core/                        # 核心层（无 UI 依赖）
│   │   ├── SystemMonitor.h/.cpp     # /proc 读取 CPU/内存/网络/温度
│   │   ├── ClipboardManager.h/.cpp  # 剪贴板历史管理
│   │   ├── SearchEngine.h/.cpp      # 模糊搜索评分
│   │   └── Logging.h/.cpp           # Qt 日志分类
│   ├── plugins/                     # 插件层（可扩展）
│   │   ├── ISearchPlugin.h          # 插件接口
│   │   ├── AppLauncher.h/.cpp       # .desktop 应用启动器
│   │   └── SystemCommand.h/.cpp     # 系统命令
│   ├── ui/                          # 表现层
│   │   ├── MainWindow.h/.cpp       # 边缘面板 + Tab + DConfig
│   │   ├── SystemMonitorWidget.h/.cpp  # QPainter 自绘图表
│   │   ├── ClipboardWidget.h/.cpp    # 剪贴板历史列表
│   │   ├── QuickLaunchWidget.h/.cpp # 搜索 + 结果列表
│   │   └── PomodoroWidget.h/.cpp     # 番茄钟
│   └── main.cpp                     # 入口
├── docs/                            # 设计文档
│   ├── requirements.md              # 需求规格说明书
│   ├── outline-design.md            # 概要设计文档
│   └── detailed-design.md           # 详细设计文档
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
| 源文件(.h/.cpp) | 22 个 |
| 代码行数 | ~2500 行 |
| 设计文档 | 3 份（672 行） |
| 功能模块 | 4 个 |
| 内置插件 | 2 个 |
| DConfig 配置项 | 4 项 |

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
| 系统监控 | 内存进度条 + 数值 | 自动刷新 |
| 系统监控 | 网络双曲线图 + 速率 | 自动刷新 |
| 系统监控 | 温度指示 | 自动刷新 |
| 剪贴板 | 自动保存历史（50 条） | 后台监听 |
| 剪贴板 | 搜索过滤 | 输入关键词 |
| 剪贴板 | 点击复制 | 点击条目 |
| 剪贴板 | 置顶管理 | 右键菜单 |
| 快速启动 | 模糊搜索应用 | 输入关键词 |
| 快速启动 | 系统命令 | 输入关键词 |
| 快速启动 | 点击执行 | 点击结果 |
| 番茄钟 | 25 分钟专注 | 点击开始 |
| 番茄钟 | 5/15 分钟休息 | 自动切换 |
| 番茄钟 | 暂停/重置 | 点击按钮 |
| 全局 | Esc 隐藏面板 | 键盘 |
| 全局 | Tab 切换模块 | 键盘 |
| 全局 | Ctrl+Tab 反向切换 | 键盘 |

---

## 六、对照评分标准自评

| 评分维度 | 权重 | 自评 | 依据 |
|----------|------|------|------|
| 功能完整性 | 30% | 9/10 | 4 大模块 30+ 功能点，DConfig 配置生效 |
| 真实使用价值 | 20% | 8/10 | 系统监控+剪贴板+快速启动为日常高频需求 |
| 创新性 | 15% | 7/10 | 四合一边缘面板 + 纯自绘图表 + 插件架构 |
| UI/UX 设计 | 15% | 8/10 | DTK 毛玻璃 + 图标 Tab + 键盘快捷键 + 主题自适应 |
| 代码质量 | 20% | 8/10 | 三层架构 + 三份设计文档 + 日志分类 + 国际化 |
| **加权总分** | 100% | **~80** | |

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
| `dtk-development` | DTK 开发规范参考（通过 deepin-skills 仓库学习） |
| `dde-control-center-development` | 控制中心方向了解（对比后未选此方向） |
| `dde-tray-development` | 托盘插件方向了解（对比后未选此方向） |
| `dde-shell-development` | DDE Shell 扩展方向了解（对比后未选此方向） |

开发过程中通过参考 deepin-skills 仓库的 dtk-codeviewer 示例项目，学习了 DTK 应用的代码规范、文档结构和 DConfig 用法。
