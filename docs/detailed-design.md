# EdgeBar 详细设计文档

| 项目 | 内容 |
|------|------|
| 项目名称 | EdgeBar（桌边栏） |
| 文档版本 | v1.0.0 |
| 编写日期 | 2025-08-12 |
| 对应概要设计 | outline-design.md v1.0.0 |

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

关键方法：`start(intervalMs=2000)` 启动定时器，`poll()` 统一采集入口依次调用 `readCpuStat()`/`readMemInfo()`/`readNetDev()`/`readTemperature()` 后发出 `statsUpdated()` 信号。`HISTORY_SIZE` 常量固定为 60。

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

### 1.2 插件层类设计

`ISearchPlugin` 为抽象接口，定义 5 个纯虚函数：`id()`、`name()`、`shouldActivate(query)`、`search(query)`、`activate(result)`。

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

---

## 7. DConfig 配置项

配置文件 `configs/edgebar.json`，遵循 `dsg.config` 格式。

| 配置键 | 类型 | 默认值 | 名称 | 说明 |
|--------|------|--------|------|------|
| `edgeSide` | string | `"right"` | 边缘位置 | 面板停靠位置(left/right) |
| `autoHide` | bool | `true` | 自动隐藏 | 鼠标离开时自动滑出 |
| `monitorInterval` | int | `2000` | 监控刷新间隔 | 采样间隔(毫秒) |
| `maxClipboardItems` | int | `50` | 剪贴板最大条数 | 历史记录上限 |

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
