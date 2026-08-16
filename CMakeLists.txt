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
    src/search/GrandSearchAdaptor.cpp
    src/ui/MainWindow.cpp
    src/ui/SystemMonitorWidget.cpp
    src/ui/ClipboardWidget.cpp
    src/ui/QuickLaunchWidget.cpp
    src/ui/PomodoroWidget.cpp
    src/ui/MiniCountdown.cpp
    src/ui/HealthReminderWidget.cpp
    src/ui/DesktopWidget.cpp
    src/ui/ProcessManagerWidget.cpp
)

# ---- 可执行目标 ----
add_executable(edgebar ${SOURCES})

target_include_directories(edgebar PRIVATE
    src
    src/core
    src/plugins
    src/search
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
install(FILES configs/edgebar.json DESTINATION share/dsg/configs/org.deepin.edgebar/)
# dde-grand-search 插件配置（Manual 模式）
install(FILES plugins/edgebar-search.conf
        DESTINATION lib/x86_64-linux-gnu/dde-grand-search-daemon/plugins/searcher/)
if(LINGUIST_FOUND AND QM_FILES)
    install(FILES ${QM_FILES} DESTINATION share/edgebar/translations)
endif()
install(FILES translations/edgebar_zh_CN.ts DESTINATION share/edgebar/translations)

# ---- 单元测试 ----
if(TEST_ENABLED)
    # 核心库源文件（不含 main.cpp）
    set(CORE_SOURCES
        src/core/Logging.cpp
        src/core/SystemMonitor.cpp
        src/core/ClipboardManager.cpp
        src/core/SearchEngine.cpp
        src/plugins/AppLauncher.cpp
        src/plugins/SystemCommand.cpp
    )

    # 包含 AutostartManager 的核心源文件（不含 NotificationManager，避免 DBus 依赖扩散）
    set(CORE_AUTOSTART_SOURCES
        ${CORE_SOURCES}
        src/core/AutostartManager.cpp
    )

    # NotificationManager 测试需要的额外 UI 源文件（引用 HealthReminderWidget/PomodoroWidget 信号）
    set(NOTIF_UI_SOURCES
        src/ui/HealthReminderWidget.cpp
        src/ui/PomodoroWidget.cpp
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
        Qt${QT_VERSION_MAJOR}::Gui
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

    # NotificationManager 测试（需要 DBus + DTK + UI 依赖）
    add_executable(test_notificationmanager tests/test_notificationmanager.cpp
        ${CORE_SOURCES}
        src/core/NotificationManager.cpp
        ${NOTIF_UI_SOURCES}
    )
    target_include_directories(test_notificationmanager PRIVATE src src/core src/plugins src/ui)
    target_link_libraries(test_notificationmanager PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
        Qt${QT_VERSION_MAJOR}::DBus
        Qt${QT_VERSION_MAJOR}::Test
        ${DTK_LIBS}
    )
    add_test(NAME notificationmanager COMMAND test_notificationmanager)

    # AutostartManager 测试（不含 NotificationManager，避免 DBus 依赖）
    add_executable(test_autostartmanager tests/test_autostartmanager.cpp ${CORE_AUTOSTART_SOURCES})
    target_include_directories(test_autostartmanager PRIVATE src src/core src/plugins src/ui)
    target_link_libraries(test_autostartmanager PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
        ${DTK_LIBS}
    )
    add_test(NAME autostartmanager COMMAND test_autostartmanager)

    # PomodoroWidget 测试（需要 DTK Widget）
    set(UI_TEST_SOURCES
        ${CORE_SOURCES}
        src/ui/PomodoroWidget.cpp
    )
    add_executable(test_pomodoro tests/test_pomodoro.cpp ${UI_TEST_SOURCES})
    target_include_directories(test_pomodoro PRIVATE src src/core src/plugins src/ui)
    target_link_libraries(test_pomodoro PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Test
        ${DTK_LIBS}
    )
    add_test(NAME pomodoro COMMAND test_pomodoro)

    # CI/无头环境使用 offscreen 平台运行测试
    set_tests_properties(searchengine clipboardmanager systemmonitor
                         notificationmanager autostartmanager pomodoro
                         PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
