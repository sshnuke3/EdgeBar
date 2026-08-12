cmake_minimum_required(VERSION 3.16)
project(EdgeBar VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter -Wno-deprecated-declarations")

# ---- Qt 版本检测 ----
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network)

message(STATUS "Using Qt${QT_VERSION_MAJOR}")

# ---- DTK ----
if(QT_VERSION_MAJOR EQUAL 6)
    set(DTK_SUFFIX 6)
else()
    set(DTK_SUFFIX "")
endif()

find_package(Dtk${DTK_SUFFIX}Core REQUIRED)
find_package(Dtk${DTK_SUFFIX}Gui REQUIRED)
find_package(Dtk${DTK_SUFFIX}Widget REQUIRED)

if(QT_VERSION_MAJOR EQUAL 6)
    set(DTK_LIBS Dtk${DTK_SUFFIX}::Core Dtk${DTK_SUFFIX}::Gui Dtk${DTK_SUFFIX}::Widget)
else()
    set(DTK_LIBS ${DtkWidget_LIBRARIES})
endif()

# ---- 源文件 ----
set(SOURCES
    src/main.cpp
    src/core/SystemMonitor.cpp
    src/core/ClipboardManager.cpp
    src/core/SearchEngine.cpp
    src/plugins/AppLauncher.cpp
    src/plugins/SystemCommand.cpp
    src/ui/MainWindow.cpp
    src/ui/SystemMonitorWidget.cpp
    src/ui/ClipboardWidget.cpp
    src/ui/QuickLaunchWidget.cpp
    src/ui/PomodoroWidget.cpp
)

# ---- 可执行目标 ----
add_executable(edgebar ${SOURCES})

target_include_directories(edgebar PRIVATE
    src
    src/core
    src/plugins
    src/ui
)

target_link_libraries(edgebar PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
    ${DTK_LIBS}
)

# ---- 安装 ----
install(TARGETS edgebar DESTINATION bin)
install(FILES edgebar.desktop DESTINATION share/applications)
install(FILES configs/edgebar.json DESTINATION share/dsg/configs/)
