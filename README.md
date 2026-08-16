# EdgeBar 桌边栏

> deepin 桌面插件开发参赛作品

屏幕边缘滑出的智能面板，鼠标靠近屏幕边缘自动滑入，离开自动滑出。集成系统监控、进程管理、剪贴板历史、快速启动、番茄钟、全局搜索插件和桌面小组件七大功能。

## 功能特性

### 系统监控（自绘图表）
- **CPU 仪表盘**：圆形进度环 + 历史曲线，颜色随使用率变化（绿→橙→红）
- **内存进度条**：横向填充条 + 实时数值（已用/总量/百分比）
- **网络流量图**：双曲线（下载蓝/上传绿）+ 速率文本
- **温度指示**：温度计风格条 + 摄氏度数值
- **内存压力监控**：基于 PSI（Pressure Stall Information）的内存压力检测

### 进程管理器
- 查看 CPU 占用最高的 15 个进程（PID / 进程名 / CPU% / 内存）
- 一键结束进程（SIGTERM），带确认对话框
- 2 秒自动刷新 + 手动刷新
- CPU 高占用进程标红（>80% 红色 / >50% 橙色）

### 剪贴板历史
- 自动监听系统剪贴板，保存最近 50 条记录
- 支持文本和图片历史（截图、复制图片）
- 图片去重：64×64 降采样 + MD5 hash
- 防抖处理（300ms），避免重复
- 点击条目直接复制到剪贴板
- 支持搜索过滤、置顶（pin）、删除、自动去重
- 时间戳显示

### 快速启动
- 模糊搜索引擎（子序列匹配 + 评分）
- 应用启动器（扫描 .desktop 文件）
- 系统命令（锁屏/休眠/重启/关机等）
- 右键系统快捷操作（亮度调节等）
- 点击即执行

### 番茄钟
- 25 分钟专注 → 5 分钟休息循环
- 第四轮后 15 分钟长休息
- 圆形进度环 + 倒计时数字
- 任务标签绑定 + 专注历史记录
- 开始/暂停/重置控制
- 面板隐藏时迷你倒计时浮窗

### 桌面小组件
- 独立悬浮于桌面的迷你组件，6 种模式切换：
  - **CPU 仪表**：环形 CPU 使用率
  - **内存仪表**：内存使用率
  - **网络监控**：上下行速率 + 迷你曲线图
  - **时钟**：时间 + 日期 + 星期
  - **番茄钟**：迷你番茄钟进度
  - **喝水提醒**：饮水进度
- 半透明毛玻璃背景 + 圆角
- 置顶显示，不遮挡工作区

### 全局搜索插件
- 集成 dde-grand-search 全局搜索框架
- 搜索范围：已安装应用 / 系统操作 / 剪贴板历史
- DBus 接口遵循 dde-grand-search V1.0 协议
- 搜索结果可直接打开应用、执行命令、复制剪贴板

### 健康提醒
- 喝水提醒（可配置间隔 30/45/60 分钟）
- 久坐提醒（可配置间隔 45/60/90 分钟）
- 桌面通知 + 音效

### 桌面集成
- DTK 毛玻璃 + 圆角窗口
- 深色/浅色主题自适应 + 壁纸随主题切换
- 边缘自动隐藏动画
- DConfig 配置持久化（15 项配置）
- 单实例运行 + 开机自启动
- 日志分类（`Q_LOGGING_CATEGORY`）+ 国际化翻译

## 使用方式

1. 启动后鼠标移动到屏幕右侧边缘
2. 面板自动滑出
3. 点击顶部标签切换功能：监控 / 剪贴板 / 启动 / 专注
4. 系统监控页右键弹出进程管理器
5. 鼠标离开后面板自动滑回
6. 桌面小组件独立悬浮于桌面，右键切换模式

## 架构设计

```
EdgeBar/
├── src/
│   ├── core/                        # 核心层
│   │   ├── SystemMonitor            # /proc 读取系统资源 + 进程管理
│   │   ├── ClipboardManager         # 剪贴板历史管理（文本+图片）
│   │   ├── SearchEngine             # 模糊搜索引擎
│   │   ├── NotificationManager      # 桌面通知管理
│   │   ├── AutostartManager         # 开机自启动管理
│   │   ├── IconHelper               # 主题图标查找
│   │   └── Logging                  # 日志分类
│   ├── plugins/                     # 插件层
│   │   ├── ISearchPlugin            # 插件接口
│   │   ├── AppLauncher              # 应用启动器
│   │   └── SystemCommand            # 系统命令
│   ├── search/                      # 全局搜索层
│   │   └── GrandSearchAdaptor       # dde-grand-search DBus 适配器
│   └── ui/                          # UI 层
│       ├── MainWindow               # 边缘隐藏主窗口
│       ├── SystemMonitorWidget      # 自绘系统监控图表
│       ├── ProcessManagerWidget     # 进程管理器对话框
│       ├── ClipboardWidget          # 剪贴板历史列表
│       ├── QuickLaunchWidget        # 搜索+启动
│       ├── PomodoroWidget           # 番茄钟计时器
│       ├── MiniCountdown            # 迷你倒计时浮窗
│       ├── DesktopWidget           # 桌面小组件（6 模式）
│       └── HealthReminderWidget     # 健康提醒
├── plugins/edgebar-search.conf      # dde-grand-search 插件配置
├── configs/edgebar.json             # DConfig 配置
├── tests/                           # 单元测试（6 个）
├── debian/                          # Debian 打包
└── CMakeLists.txt
```

## 技术栈

- **语言**: C++17
- **框架**: Qt5/Qt6 + DTK (Deepin Tool Kit)
- **UI**: 自绘 QPainter 图表（无第三方图表库）
- **系统数据**: /proc/stat, /proc/meminfo, /proc/net/dev, /sys/class/thermal, /proc/pressure
- **进程管理**: /proc/[pid]/stat, POSIX kill(SIGTERM)
- **全局搜索**: dde-grand-search V1.0 DBus 协议
- **配置**: DConfig (deepin 统一配置方案，15 项配置)
- **构建**: CMake
- **打包**: Debian (.deb)

## 编译

```bash
# 安装依赖
sudo apt install cmake qtbase5-dev qttools5-dev libdtkcore-dev libdtkgui-dev libdtkwidget-dev

# 构建
./build.sh

# 安装
sudo cmake --install build
# 或
sudo dpkg -i edgebar_1.0.0-1_amd64.deb
```

## 单元测试

```bash
cd build && ctest --output-on-failure
```

6 个测试模块：SearchEngine、ClipboardManager、SystemMonitor、NotificationManager、AutostartManager、PomodoroWidget

## 许可证

GPLv3
