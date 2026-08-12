# EdgeBar 桌边栏

> deepin 桌面插件开发参赛作品

屏幕边缘滑出的智能面板，鼠标靠近屏幕边缘自动滑入，离开自动滑出。集成系统监控、剪贴板历史、快速启动和番茄钟四大功能。

## 功能特性

### 系统监控（自绘图表）
- **CPU 仪表盘**：圆形进度环 + 历史曲线，颜色随使用率变化（绿→橙→红）
- **内存进度条**：横向填充条 + 实时数值（已用/总量/百分比）
- **网络流量图**：双曲线（下载蓝/上传绿）+ 速率文本
- **温度指示**：温度计风格条 + 摄氏度数值

### 剪贴板历史
- 自动监听系统剪贴板，保存最近 50 条记录
- 防抖处理（300ms），避免重复
- 点击条目直接复制到剪贴板
- 支持搜索过滤、置顶（pin）、自动去重
- 时间戳显示

### 快速启动
- 模糊搜索引擎（子序列匹配 + 评分）
- 应用启动器（扫描 .desktop 文件）
- 系统命令（锁屏/休眠/重启/关机等）
- 点击即执行

### 番茄钟
- 25 分钟专注 → 5 分钟休息循环
- 第四轮后 15 分钟长休息
- 圆形进度环 + 倒计时数字
- 开始/暂停/重置控制

### 桌面集成
- DTK 毛玻璃 + 圆角窗口
- 深色/浅色主题自适应
- 边缘自动隐藏动画
- DConfig 配置持久化
- 单实例运行

## 使用方式

1. 启动后鼠标移动到屏幕右侧边缘
2. 面板自动滑出
3. 点击顶部标签切换功能：监控 / 剪贴板 / 启动 / 专注
4. 鼠标离开后面板自动滑回

## 架构设计

```
EdgeBar/
├── src/
│   ├── core/                        # 核心层
│   │   ├── SystemMonitor            # /proc 读取系统资源
│   │   ├── ClipboardManager         # 剪贴板历史管理
│   │   └── SearchEngine             # 模糊搜索引擎
│   ├── plugins/                     # 插件层
│   │   ├── ISearchPlugin            # 插件接口
│   │   ├── AppLauncher              # 应用启动器
│   │   └── SystemCommand            # 系统命令
│   └── ui/                          # UI 层
│       ├── MainWindow               # 边缘隐藏主窗口
│       ├── SystemMonitorWidget      # 自绘系统监控图表
│       ├── ClipboardWidget           # 剪贴板历史列表
│       ├── QuickLaunchWidget        # 搜索+启动
│       └── PomodoroWidget           # 番茄钟计时器
├── configs/edgebar.json             # DConfig 配置
├── debian/                          # Debian 打包
└── CMakeLists.txt
```

## 技术栈

- **语言**: C++17
- **框架**: Qt5/Qt6 + DTK (Deepin Tool Kit)
- **UI**: 自绘 QPainter 图表（无第三方图表库）
- **系统数据**: /proc/stat, /proc/meminfo, /proc/net/dev, /sys/class/thermal
- **配置**: DConfig (deepin 统一配置方案)
- **构建**: CMake
- **打包**: Debian (.deb)

## 编译

```bash
# 安装依赖
sudo apt install cmake qtbase5-dev libdtkcore-dev libdtkgui-dev libdtkwidget-dev

# 构建
./build.sh

# 安装
sudo cmake --install build
# 或
sudo dpkg -i edgebar_1.0.0-1_amd64.deb
```

## 许可证

GPLv3
