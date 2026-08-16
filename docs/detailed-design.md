# EdgeBar 详细设计文档

| 项目 | 内容 |
|------|------|
| 项目名称 | EdgeBar（桌边栏） |
| 文档版本 | v2.0.0 |
| 编写日期 | 2025-08-16 |
| 对应概要设计 | outline-design.md v2.0.0 |

---

## 1. 类设计

### 1.1 核心层类设计

**SystemMonitor** — 继承 QObject，通过 QTimer 周期性采集系统资源。

| 成员 | 类型 | 说明 |
|------|------|------|
| m_timer | QTimer | 采样定时器 |
| m_prevIdle / m_prevTotal | qint64 | CPU 上次采样基线 |
| m_cpuUsage | float | CPU 使用率(%) |
| m_cpuHistory | QVector<float> | CPU 历史曲线（上限 60） |
| m_memTotal / m_memUsed | qint64 | 内存总量/已用(bytes) |
| m_prevRxBytes / m_prevTxBytes | qint64 | 网络上次采样基线 |
| m_prevNetTime | qint64 | 上次网络采样时间戳 |
| m_netUpload / m_netDownload | float | 网络速率(KB/s) |
| m_netUpHistory / m_netDownHistory | QVector<float> | 网络历史曲线（上限 60） |
| m_temperature | float | 温度(°C) |
| m_prevProcTimes | QMap<int, qint64> | 每进程上次 CPU 时间(utime+stime) |
| m_topProcess | ProcessInfo | 当前 CPU 占用最高的进程 |
| m_topMemProcesses | QList<MemProcessInfo> | 内存占用靠前的进程列表 |
| m_memPressureLevel | MemPressureLevel | 内存压力级别(None/Some/Full) |
| m_dailyRxBytes / m_dailyTxBytes | qint64 | 每日流量计数器 |
| m_snapshots | QVector<Snapshot> | 用于 CSV 导出的历史数据快照 |

关键方法：`start(intervalMs=2000)` 启动定时器，`poll()` 统一采集入口依次调用 `readCpuStat()`/`readMemInfo()`/`readNetDev()`/`readTemperature()` 后发出 `statsUpdated()` 信号。`HISTORY_SIZE` 常量固定为 60。

**ProcessInfo 结构体**：`{ pid(int), name(QString), cpuPercent(float), rssBytes(qint64), sustainedSeconds(int) }`，用于描述单个进程的实时资源占用，`sustainedSeconds` 记录该进程持续占用 CPU 的时间，配合 `cpuSustainedSeconds` 配置项触发高负载告警。

**MemProcessInfo 结构体**：`{ pid(int), name(QString), rssBytes(qint64), memPercent(float) }`，用于描述内存占用排名靠前的进程，`memPercent` 为该进程 RSS 占系统总内存的百分比。

**Snapshot 结构体**：`{ timestamp(qint64), cpu(float), mem(float), net(float), temp(float) }`，用于 CSV 导出，队列上限 360 条（约 12 分钟、2 秒采样间隔）。导出时按行写入 `timestamp,cpu,mem,net,temp` 格式。

新增方法说明：

- `topCpuProcesses(maxCount=10)`：遍历 `/proc/[pid]/stat`，读取每个进程的 `utime`(字段 11) 和 `stime`(字段 12)，结合上次采样基线 `m_prevProcTimes` 计算每进程 CPU 占用率，按降序排序返回前 `maxCount` 条 `ProcessInfo`。
- `killProcess(pid)`：通过 POSIX `kill(pid, SIGTERM)` 发送终止信号，成功后发出 `processKilled(pid)` 信号。失败时记录日志并返回错误码。
- CSV 导出：将 `m_snapshots` 队列中的 `Snapshot` 结构逐条写入 CSV 文件，首行为表头 `timestamp,cpu,mem,net,temp`，单队列上限 360 条，超出时丢弃最旧数据。

**ClipboardManager** — 继承 QObject，监听 QClipboard 变化。

| 成员 | 类型 | 说明 |
|------|------|------|
| m_clipboard | QClipboard* | 系统剪贴板引用 |
| m_items | QList<ClipItem> | 历史记录列表 |
| m_nextId | int | 自增 ID 分配器 |
| m_lastText | QString | 上次处理文本（去重用） |
| m_debounceTimer | QTimer | 防抖定时器(300ms, 单次) |

ClipItem 结构体：`id`(int)、`text`(QString)、`preview`(QString, 截断 80 字符)、`pinned`(bool)、`timestamp`(qint64)。`MAX_ITEMS=50`，`PREVIEW_LEN=80`。

**SearchEngine** — 静态工具类，无成员变量。提供 `fuzzyScore(query, target)` 和 `containsMatch(query, target)` 两个静态方法。

**NotificationManager** — 继承 QObject，封装 D-Bus freedesktop 通知接口（`org.freedesktop.Notifications`）。

| 成员 | 类型 | 说明 |
|------|------|------|
| m_iface | QDBusInterface* | freedesktop 通知接口代理 |
| m_lastId | uint | 上次发送通知的 ID，用于替换 |

方法：
- `showNotification(title, body, icon)`：调用 `Notify` 方法发送通知，`icon` 可为空使用默认图标。支持指定超时时间（默认 -1 表示系统默认），并可在通知上注册 Action 回调（如"打开"、"忽略"），通过 `NotificationClosed`/`ActionInvoked` 信号通知上层。
- `playSound()`：播放系统提示音，通过 freedesktop 声音主题或 Qt Multimedia 实现。

**AutostartManager** — 管理 XDG 自启动 `.desktop` 文件，路径为 `$XDG_CONFIG_HOME/autostart/edgebar.desktop`（未设置时回退到 `~/.config/autostart/`）。

| 成员 | 类型 | 说明 |
|------|------|------|
| m_desktopPath | QString | autostart .desktop 文件完整路径 |

方法：
- `isEnabled()`：检查 autostart 目录下是否存在 `edgebar.desktop` 且未设置 `Hidden=true`/`X-GNOME-Autostart-enabled=false`。
- `setEnabled(bool)`：`true` 调用 `createDesktopFile()`，`false` 调用 `removeDesktopFile()`。
- `createDesktopFile()`：写入标准 `.desktop` 内容，含 `Type=Application`、`Name=EdgeBar`、`Exec=edgebar`、`Icon=edgebar`、`X-GNOME-Autostart-enabled=true`。
- `removeDesktopFile()`：删除 autostart 目录下的 `.desktop` 文件。

**IconHelper** — 静态工具类，用于 DTK 主题图标解析。核心方法 `edgebarFindIcon(name)`：依次在当前 DTK 图标主题、`hicolor`、`bloom` 等内置主题以及应用自带 fallback 路径（如 `:/icons/`）中查找图标，找到则返回 `QIcon`，全部失败返回空图标，避免因主题缺失导致界面空白。

### 1.2 插件层类设计

`ISearchPlugin` 为抽象接口，定义 5 个纯虚函数：`id()`、`name()`、`shouldActivate(query)`、`search(query)`、`activate(result)`。

> 备注：`ISearchPlugin` 接口及其实现（AppLauncher、SystemCommand 等）同时被 `GrandSearchAdaptor`（见 1.4 节）复用，用于向 dde-grand-search 框架暴露搜索能力，避免搜索逻辑重复实现。

**AppLauncher** — 实现 ISearchPlugin，`id()` 返回 `"app-launcher"`。持有 `QList<AppEntry> m_apps`，AppEntry 含 `name`/`exec`/`icon`/`desktopPath`/`comment`。构造时调用 `refresh()` 扫描 `.desktop` 文件。

**SystemCommand** — 实现 ISearchPlugin，`id()` 返回 `"system-command"`。持有 `QList<Command> m_commands`，构造时调用 `initCommands()` 初始化 9 条命令。

### 1.3 表现层类设计

**MainWindow** — 继承 DMainWindow，窗口尺寸 280x640。

| 成员 | 说明 |
|------|------|
| m_centralWidget | DWidget，中央容器 |
| m_sysMonitorWidget / m_clipboardWidget / m_quickLaunchWidget / m_pomodoroWidget | 四个功能子组件 |
| m_sysMonitor / m_clipboard | 核心层数据对象 |
| m_edgeSide | 边缘位置(LeftEdge/RightEdge) |
| m_autoHide / m_hidden | 自动隐藏开关/当前隐藏状态 |
| m_edgeTimer | 鼠标边缘检测定时器(300ms) |
| m_slideAnim | QPropertyAnimation 滑动动画 |

枚举 `EdgeSide { LeftEdge, RightEdge }`，`TabIndex { SystemTab, ClipboardTab, LaunchTab, PomodoroTab }`。

**ProcessManagerWidget** — 继承 DDialog，进程管理对话框，用于查看并结束占用资源的进程。

| 成员 | 类型 | 说明 |
|------|------|------|
| m_monitor | SystemMonitor* | 数据来源，复用核心层采集结果 |
| m_table | QTableWidget* | 进程列表表格 |
| m_statusLabel | QLabel* | 底部状态提示（进程数/选中项） |
| m_refreshTimer | QTimer* | 自动刷新定时器(2s) |

表格为 5 列：PID、名称、CPU%、内存、操作（结束按钮）。交互逻辑：
- 双击某行或点击该行"结束"按钮，弹出确认对话框，确认后调用 `m_monitor->killProcess(pid)` 结束进程。
- `m_refreshTimer` 每 2 秒触发一次，调用 `m_monitor->topCpuProcesses()` 刷新表格内容并更新 `m_statusLabel`。

**DesktopWidget** — 继承 QWidget，桌面悬浮小组件，提供多种模式在桌面上常驻展示。

| 成员 | 类型 | 说明 |
|------|------|------|
| m_mode | WidgetMode | 当前显示模式 |
| m_expanded | bool | 是否展开（紧凑/展开两档尺寸） |
| m_updateTimer | QTimer* | 重绘定时器 |

`WidgetMode` 枚举：`CpuGaugeMode(0)`、`PomodoroMode(1)`、`WaterMode(2)`、`MemGaugeMode(3)`、`NetMode(4)`、`ClockMode(5)`。

尺寸常量：`COMPACT_SIZE = 120`、`EXPANDED_SIZE = 240`。窗口标志位 `Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool`，背景半透明（`setAttribute(Qt::WA_TranslucentBackground)`）。

6 个绘制方法分别对应 6 种模式：`drawCpuGauge`、`drawPomodoro`、`drawWater`、`drawMemory`、`drawNetwork`、`drawClock`（详见 6.3 节）。

**MiniCountdown** — 继承 QWidget，番茄钟面板隐藏时显示的迷你倒计时浮窗。尺寸 24x50px，窗口标志位同 DesktopWidget（无边框、置顶、Tool），仅显示 `MM:SS` 文本，便于在面板隐藏期间持续关注剩余时间。

**HealthReminderWidget** — 继承 DWidget，健康提醒组件，包含喝水间隔计时器和久坐起身间隔计时器。计时到达后通过 `NotificationManager` 发送系统通知（可选播放提示音），间隔时长由 `waterIntervalMin`/`standIntervalMin` 配置项控制。

### 1.4 搜索层类设计

**GrandSearchAdaptor** — 继承 `QDBusAbstractAdaptor`，作为 dde-grand-search 框架的搜索插件适配器，将 EdgeBar 内部搜索能力（复用 `ISearchPlugin` 实现）通过 D-Bus 暴露给全局搜索。

| 成员 | 说明 |
|------|------|
| D-Bus 接口名 | `org.deepin.edgebar.SearchPlugin` |
| D-Bus 对象路径 | `/org/deepin/edgebar/SearchPlugin` |

三个槽函数（均通过 D-Bus 导出）：

| 槽函数 | 签名 | 说明 |
|--------|------|------|
| `Search` | `QString Search(QString json)` | 接收搜索请求 JSON，返回结果 JSON |
| `Stop` | `bool Stop(QString json)` | 停止当前搜索任务 |
| `Action` | `bool Action(QString json)` | 处理结果项的后续动作 |

**Search 流程**：解析输入 JSON，字段为 `{ ver, mID, cont }`（`ver` 协议版本、`mID` 搜索会话 ID、`cont` 搜索内容）。随后调用各 `ISearchPlugin` 实现的 `shouldActivate(cont)`/`search(cont)`，分别搜索应用（AppLauncher）、命令（SystemCommand）、剪贴板历史（ClipboardManager），聚合结果后返回结果 JSON，包含匹配项列表（含类型、名称、图标、激活标识）。

**Action 流程**：处理 `openitem` 动作，根据结果项前缀分发：
- `app:` 前缀 -> 调用 AppLauncher 启动对应 `.desktop` 应用。
- `cmd:` 前缀 -> 调用 SystemCommand 执行对应系统命令。
- `clip:` 前缀 -> 调用 ClipboardManager 将对应历史条目写回剪贴板。

---

## 2. SystemMonitor 采集算法

### 2.1 CPU 使用率

读取 `/proc/stat` 第一行 `cpu user nice system idle iowait irq softirq steal`，解析 8 个字段：

```
total = sum(fields[0..7])
idle  = fields[3] + fields[4]   (idle + iowait)
diffTotal = total - m_prevTotal
diffIdle  = idle  - m_prevIdle
usage(%) = (diffTotal - diffIdle) * 100 / diffTotal
```

首次采样仅记录基线不计算。`usage` 追加至 `m_cpuHistory`，超 60 条移除最旧数据。

### 2.2 内存使用率

读取 `/proc/meminfo` 解析 `MemTotal` 和 `MemAvailable`（单位 kB）：

```
memTotal(bytes) = MemTotal * 1024
memUsed(bytes)  = (MemTotal - MemAvailable) * 1024
memUsage(%)     = (MemTotal - MemAvailable) * 100 / MemTotal
```

### 2.3 网络速率

读取 `/proc/net/dev`，取第一个非 `lo` 接口的 `rx_bytes`(字段0) 和 `tx_bytes`(字段8)：

```
secs = (currentMSecs - m_prevNetTime) / 1000.0
netDownload(KB/s) = (rxBytes - m_prevRxBytes) / secs / 1024.0
netUpload(KB/s)   = (txBytes - m_prevTxBytes) / secs / 1024.0
```

### 2.4 温度读取

按优先级尝试路径，值为毫摄氏度：

| 优先级 | 路径 |
|--------|------|
| 1 | `/sys/class/thermal/thermal_zone0/temp` |
| 2 | `/sys/class/hwmon/hwmon0/temp1_input` |

`temperature(°C) = rawValue / 1000.0`，全部失败时置 0。

### 2.5 进程 CPU 占用计算

读取 `/proc/[pid]/stat`，解析关键字段 `utime`(字段 11，用户态 jiffies) 与 `stime`(字段 12，内核态 jiffies)，单位为 `USER_HZ`（通常 100）。结合上次采样基线 `m_prevProcTimes[pid]` 计算：

```
procTime   = utime + stime
diffProc   = procTime - m_prevProcTimes[pid]
diffTotal  = total - m_prevTotal          // 复用 2.1 的全局 total 差值
cpuPercent = diffProc * 100 / diffTotal    // 占单核百分比，可超 100（多核）
```

更新 `m_prevProcTimes[pid] = procTime`。进程消失时从 `m_prevProcTimes` 移除对应键，避免基线泄漏。`topCpuProcesses()` 对全部存活进程按 `cpuPercent` 降序排序，取前 `maxCount` 条封装为 `ProcessInfo`。

### 2.6 内存压力（PSI）

读取 `/proc/pressure/memory`，文件内容形如：

```
some avg10=0.12 avg60=0.05 avg300=0.01 total=12345
full avg10=0.08 avg60=0.03 avg300=0.00 total=6789
```

解析 `some` 与 `full` 两行的 `avg10`（近 10 秒平均压力比例）和 `total`（累计阻塞时间，微秒）。映射为 `MemPressureLevel`：

| 条件 | 级别 |
|------|------|
| `full.avg10` 超过阈值 | `Full`（内存严重不足，存在 OOM 风险） |
| `some.avg10` 超过阈值 | `Some`（部分任务因内存等待） |
| 均未超阈值 | `None` |

阈值由配置项 `memPressureThreshold` 控制。级别变化时发出 `memPressureChanged(level)` 信号，UI 层据此切换内存指示条颜色或触发告警通知。

### 2.7 进程 RSS 内存

读取 `/proc/[pid]/stat` 的第 23 个字段 `rss`（单位为页），通过 `sysconf(_SC_PAGESIZE)` 获取页大小后换算为字节：

```
rssBytes = rss * sysconf(_SC_PAGESIZE)
memPercent = rssBytes * 100 / m_memTotal
```

`topMemProcesses()` 对全部存活进程按 `rssBytes` 降序排序，取前若干条封装为 `MemProcessInfo`，供进程管理界面与桌面内存小组件展示。

---

## 3. ClipboardManager 防抖去重算法

### 3.1 防抖机制

`QClipboard::dataChanged` 信号触发 `m_debounceTimer.start()`（单次触发 300ms）。若 300ms 内再次变化则重新计时，只有信号停止 300ms 后才执行 `onClipboardChanged()`。

### 3.2 去重逻辑

`onClipboardChanged()` 流程：

1. 读取剪贴板文本 `text`，若为空或等于 `m_lastText` 则跳过。
2. 更新 `m_lastText = text`。
3. 遍历 `m_items`，若存在相同文本：移除原条目，更新时间戳，`prepend` 到列表顶部，发出信号返回。
4. 不存在则调用 `addItem(text)`。

### 3.3 addItem 逻辑

分配递增 `id`，生成预览（替换 `\n`/`\r` 为空格，截断至 80 字符），记录时间戳，`prepend` 到列表。若超 50 条且最后一条未置顶则移除。

### 3.4 copyToClipboard 防循环

点击复制时 `blockSignals(true)` 阻止 `dataChanged` 信号，复制后 `blockSignals(false)` 恢复，同时同步 `m_lastText` 做双重保险。

---

## 4. SearchEngine 评分算法

### 4.1 算法流程

1. query 或 target 为空返回 0。
2. 统一转小写，若完全相等返回 `100 + q.length()`。
3. 子序列遍历 target 每个字符，按顺序匹配 query 字符：
   - 连续匹配（`prevMatchIndex == ti-1`）：+5
   - 首字母匹配（`qi==0 且 ti==0`）：+10
   - 词边界匹配（`ti==0 或 t[ti-1] 非字母数字`）：+8
4. 若 query 未完全匹配（`qi < q.length()`）返回 0。
5. 返回 `score + matchedCount`（基础分每字符 +1）。

### 4.2 评分规则汇总

| 规则 | 分值 | 条件 |
|------|------|------|
| 完全匹配 | 100 + 字符数 | query == target（不区分大小写） |
| 基础分 | +1 / 每字符 | 每个匹配的字符 |
| 连续匹配 | +5 | 匹配位置紧接上一次之后 |
| 首字母匹配 | +10 | query 首字符匹配 target 首字符 |
| 词边界匹配 | +8 | 匹配位置在开头或非字母数字之后 |

---

## 5. MainWindow 边缘动画状态机

### 5.1 状态与转换

| 状态 | 说明 |
|------|------|
| Hidden | 面板滑出屏幕外，仅露 4px |
| Shown | 面板完全可见 |
| Animating | 动画执行中 |

转换条件：鼠标靠近边缘 4px 范围 -> Hidden 转 Shown(`slideIn`)；鼠标离开且不在窗口内 -> Shown 转 Hidden(`slideOut`)。

### 5.2 边缘检测

`checkMousePosition()` 每 300ms 执行：获取鼠标坐标与屏幕区域，判断是否靠近边缘（RightEdge: `mouse.x() >= screen.right()-4`；LeftEdge: `mouse.x() <= screen.left()+4`）。靠近且隐藏则 `slideIn()`，不靠近且显示且鼠标不在窗口内则 `slideOut()`。

### 5.3 滑动动画参数

| 属性 | 值 |
|------|-----|
| 动画对象 | `QPropertyAnimation(this, "pos")` |
| 持续时间 | 200ms |
| 缓动曲线 | `QEasingCurve::OutCubic` |
| 垂直位置 | 屏幕垂直居中 |
| 隐藏位置(Right) | `x = screen.right() - 4` |
| 显示位置(Right) | `x = screen.right() - width - 4` |
| 隐藏位置(Left) | `x = screen.left() - width + 4` |
| 显示位置(Left) | `x = screen.left() + 4` |

---

## 6. 自绘图表绘制流程

### 6.1 SystemMonitorWidget

`paintEvent()` 将可用区域均分 4 等分，依次调用四个绘制方法：

| 方法 | 绘制内容 |
|------|----------|
| `drawCpuGauge` | 背景圆环 + 使用率弧线(颜色分级<50%绿/<80%橙/>=80%红) + 中央百分比 + 右侧历史曲线 |
| `drawMemBar` | 标签 + 数值文本 + 横向进度条(颜色分级<60%蓝/<85%橙/>=85%红) |
| `drawNetGraph` | 标签 + 速率文本 + 下载蓝色填充曲线 + 上传绿色线条曲线 |
| `drawTemperature` | 标签 + 大号温度数值 + 温度计指示条(颜色分级<50°C蓝/<70°C橙/>=70°C红) |

辅助方法 `formatSize(bytes)` 自动换算 B/KB/MB/GB，`formatSpeed(kbps)` 自动换算 KB/s/MB/s。

### 6.2 PomodoroWidget

`paintEvent()` 绘制：背景圆环(灰色 6px) -> 进度弧线(专注红/休息绿, 角度=`progress*360`) -> 时间文字 `MM:SS`(放大 2 倍) -> 状态文字 -> 番茄计数。

进度计算：`progress = 1.0 - m_remaining / m_totalSeconds`

### 6.3 DesktopWidget

`paintEvent()` 根据 `m_mode` 通过 `switch` 分发到对应的绘制方法。所有方法共用 `QPainter` 并开启抗锯齿（`QPainter::Antialiasing`）。背景统一绘制圆角矩形，使用半透明填充（紧凑模式 alpha=180，展开模式 alpha=200），保证在任意桌面壁纸上均可读。

各模式绘制内容：

| 方法 | 绘制内容 |
|------|----------|
| `drawCpuGauge` | 圆弧（背景灰 + 使用率弧，颜色分级同 6.1）+ 中央百分比文本 |
| `drawMemory` | 横向进度条 + 百分比文本（颜色分级同 6.1 `drawMemBar`） |
| `drawNetwork` | 上传/下载箭头图标 + 速率文本 + 右侧迷你折线图（复用 `m_netUpHistory`/`m_netDownHistory`） |
| `drawClock` | 主时间 `HH:MM` + 秒数（小号）+ 日期 + 星期 |
| `drawPomodoro` | 迷你进度环（角度=`progress*360`，专注红/休息绿）+ 剩余时间 |
| `drawWater` | 水滴图标 + 喝水进度（今日已喝次数/目标） |

紧凑模式下仅绘制核心元素（如 `drawCpuGauge` 只显示百分比弧线），展开模式补充历史曲线或附加文本。`m_updateTimer` 按模式需要触发重绘：时钟模式 1s，其余模式跟随 `monitorInterval`（默认 2s）。

---

## 7. DConfig 配置项

配置文件 `configs/edgebar.json`，遵循 `dsg.config` 格式。共 15 项配置：

| 配置键 | 类型 | 默认值 | 名称 | 说明 |
|--------|------|--------|------|------|
| `edgeSide` | string | `"right"` | 边缘位置 | 面板停靠位置(left/right) |
| `autoHide` | bool | `true` | 自动隐藏 | 鼠标离开时自动滑出 |
| `monitorInterval` | int | `2000` | 监控刷新间隔 | 采样间隔(毫秒) |
| `maxClipboardItems` | int | `50` | 剪贴板最大条数 | 历史记录上限 |
| `cpuSustainedSeconds` | int | `30` | CPU 持续告警阈值 | 进程 CPU 持续超阈值秒数，超过后发出告警 |
| `memPressureThreshold` | double | `0.30` | 内存压力阈值 | PSI avg10 超过该值判定为压力(Some/Full) |
| `notificationsEnabled` | bool | `true` | 通知开关 | 是否允许发送系统通知 |
| `soundEnabled` | bool | `true` | 提示音开关 | 是否播放提示音 |
| `autostartEnabled` | bool | `false` | 开机自启 | 是否随系统启动 EdgeBar |
| `darkWallpaper` | string | `""` | 深色壁纸路径 | 桌面小组件背景匹配用深色壁纸 |
| `lightWallpaper` | string | `""` | 浅色壁纸路径 | 桌面小组件背景匹配用浅色壁纸 |
| `waterIntervalMin` | int | `60` | 喝水提醒间隔 | 喝水提醒间隔(分钟) |
| `standIntervalMin` | int | `45` | 久坐提醒间隔 | 久坐起身提醒间隔(分钟) |
| `desktopWidgetEnabled` | bool | `false` | 桌面小组件开关 | 是否启用 DesktopWidget 悬浮展示 |
| `desktopWidgetMode` | int | `0` | 桌面小组件模式 | DesktopWidget 默认模式(0-5 对应 WidgetMode) |

安装路径：`/usr/share/dsg/configs/edgebar.json`

---

## 8. 构建与打包流程

### 8.1 CMake 构建

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build . -j$(nproc)
sudo cmake --install .
```

| 配置项 | 说明 |
|--------|------|
| `CMAKE_CXX_STANDARD 17` | 强制 C++17 |
| `CMAKE_AUTOMOC ON` | 自动元对象处理 |
| `find_package(QT NAMES Qt6 Qt5)` | 自动检测 Qt 版本 |
| `find_package(Dtk${DTK_SUFFIX}*)` | Qt6->DTK6, Qt5->DTK5 |
| `install(TARGETS edgebar DESTINATION bin)` | 安装可执行文件 |
| `install(FILES configs/edgebar.json DESTINATION share/dsg/configs/)` | 安装配置 |
| `install(FILES plugins/edgebar-search.conf DESTINATION lib/x86_64-linux-gnu/dde-grand-search-daemon/plugins/searcher/)` | 安装 dde-grand-search 搜索插件配置 |

> 备注：`GrandSearchAdaptor` 作为 dde-grand-search 框架的搜索插件，需将插件配置文件 `plugins/edgebar-search.conf` 安装到 `lib/x86_64-linux-gnu/dde-grand-search-daemon/plugins/searcher/`，供 dde-grand-search-daemon 加载并注册 `org.deepin.edgebar.SearchPlugin` 接口。

### 8.2 Debian 打包

```
debian/
├── control     # 依赖: cmake, debhelper-compat(=13), qtbase5-dev|qt6-base-dev,
│               #       libdtkcore-dev|libdtkcore6-dev, libdtkgui-dev|libdtkgui6-dev,
│               #       libdtkwidget-dev|libdtkwidget6-dev
├── changelog   # edgebar (1.0.0-1) unstable
├── rules       # dh $@, override_dh_auto_configure 传递 CMake 参数
└── source/format  # 3.0 (native)
```

打包命令：`dpkg-buildpackage -us -uc -b` 或 `debuild -us -uc -b`

安装目录：`usr/bin/edgebar`、`usr/share/applications/edgebar.desktop`、`usr/share/dsg/configs/edgebar.json`

### 8.3 运行时流程

`main.cpp`：DApplication 初始化 -> 注册 DLog 日志 -> 加载翻译 -> `setSingleInstance("org.deepin.edgebar")` 单实例检查 -> 构造 MainWindow -> `setEdgeSide(RightEdge)` -> `show()` -> `app.exec()` 事件循环。SystemMonitor 在 MainWindow 构造时 `start(2000)` 启动采集，边缘检测定时器 300ms 轮询。

---

## 9. 单元测试

基于 Qt Test 框架，共 6 个测试模块，覆盖核心层算法与关键组件行为：

| 测试模块 | 测试对象 | 主要测试点 |
|----------|----------|------------|
| `test_searchengine` | SearchEngine | `fuzzyScore`/`containsMatch` 模糊匹配评分：完全匹配、子序列匹配、连续/首字母/词边界加分、空输入返回 0 |
| `test_clipboardmanager` | ClipboardManager | 防抖（300ms 内多次变化只触发一次）、去重（相同文本移至顶部并更新时间戳）、置顶条目不被淘汰、`copyToClipboard` 防信号循环 |
| `test_systemmonitor` | SystemMonitor | CPU 使用率（基线/差值计算）、内存（MemTotal/MemAvailable 换算）、网络速率（时间间隔换算）、进程 CPU/RSS 计算、PSI 解析 |
| `test_notificationmanager` | NotificationManager | D-Bus freedesktop 通知接口调用、超时与 Action 回调、提示音播放路径 |
| `test_autostartmanager` | AutostartManager | `.desktop` 文件创建/删除、`isEnabled` 状态判定、`Hidden`/`X-GNOME-Autostart-enabled` 字段识别 |
| `test_pomodoro` | PomodoroWidget | 状态机转换（Idle->Focus->Break->Focus）、倒计时递减、番茄计数累加、暂停/恢复/重置 |
