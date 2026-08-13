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
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Network DBus)

# 测试框架（可选）
find_package(Qt${QT_VERSION_MAJOR}Test QUIET)
if(Qt${QT_VERSION_MAJOR}Test_FOUND)
    set(TEST_ENABLED TRUE)
    enable_testing()
    message(STATUS "Qt Test found, enabling unit tests")
else()
    message(STATUS "Qt Test not found, skipping unit tests")
endif()

# 翻译工具（可选）
find_package(Qt${QT_VERSION_MAJOR}LinguistTools QUIET)
if(Qt${QT_VERSION_MAJOR}LinguistTools_FOUND)
    message(STATUS "Qt LinguistTools found, enabling translations")
    set(LINGUIST_FOUND TRUE)
else()
    message(STATUS "Qt LinguistTools not found, skipping translation compilation")
    set(LINGUIST_FOUND FALSE)
endif()

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

# ---- 翻译 ----
set(TS_FILES
    translations/edgebar_zh_CN.ts
)
if(LINGUIST_FOUND)
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR}/src ${TS_FILES})
endif()

# ---- 源文件 ----
set(SOURCES
    src/main.cpp
    src/core/Logging.cpp
    src/core/SystemMonitor.cpp
    src/core/ClipboardManager.cpp
    src/core/SearchEngine.cpp
    src/core/NotificationManager.cpp
    src/core/AutostartManager.cpp
    src/plugins/AppLauncher.cpp
    src/plugins/SystemCommand.cpp
    src/ui/MainWindow.cpp
    src/ui/SystemMonitorWidget.cpp
    src/ui/ClipboardWidget.cpp
    src/ui/QuickLaunchWidget.cpp
    src/ui/PomodoroWidget.cpp
    src/ui/MiniCountdown.cpp
    src/ui/HealthReminderWidget.cpp
    src/ui/DesktopWidget.cpp
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
    Qt${QT_VERSION_MAJOR}::DBus
    ${DTK_LIBS}
)

# ---- 安装 ----
install(TARGETS edgebar DESTINATION bin)
install(FILES edgebar.desktop DESTINATION share/applications)
install(FILES configs/edgebar.json DESTINATION share/dsg/configs/)
if(LINGUIST_FOUND AND QM_FILES)
    install(FILES ${QM_FILES} DESTINATION share/edgebar/translations)
endif()
install(FILES translations/edgebar_zh_CN.ts DESTINATION share/edgebar/translations)

# ---- 单元测试 ----
if(TEST_ENABLED)
    # 核心库源文件（不含 main.cpp）
    # 核心库源文件（不含 main.cpp，不含 NotificationManager/AutostartManager
    # 因为后者依赖 Qt DBus，测试目标无需链接）
    set(CORE_SOURCES
        src/core/Logging.cpp
        src/core/SystemMonitor.cpp
        src/core/ClipboardManager.cpp
        src/core/SearchEngine.cpp
        src/plugins/AppLauncher.cpp
        src/plugins/SystemCommand.cpp
    )

    # SearchEngine 测试
    add_executable(test_searchengine tests/test_searchengine.cpp ${CORE_SOURCES})
    target_include_directories(test_searchengine PRIVATE src src/core src/plugins src/ui)
    target_link_libraries(test_searchengine PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Test
        ${DTK_LIBS}
    )
    add_test(NAME searchengine COMMAND test_searchengine)

    # ClipboardManager 测试
    add_executable(test_clipboardmanager tests/test_clipboardmanager.cpp ${CORE_SOURCES})
    target_include_directories(test_clipboardmanager PRIVATE src src/core src/plugins src/ui)
    target_link_libraries(test_clipboardmanager PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
        ${DTK_LIBS}
    )
    add_test(NAME clipboardmanager COMMAND test_clipboardmanager)

    # SystemMonitor 测试
    add_executable(test_systemmonitor tests/test_systemmonitor.cpp ${CORE_SOURCES})
    target_include_directories(test_systemmonitor PRIVATE src src/core src/plugins src/ui)
    target_link_libraries(test_systemmonitor PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::Test
        ${DTK_LIBS}
    )
    add_test(NAME systemmonitor COMMAND test_systemmonitor)
endif()
