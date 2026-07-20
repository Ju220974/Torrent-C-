cmake_minimum_required(VERSION 3.16)
project(Undertow VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Widgets)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(LIBTORRENT REQUIRED IMPORTED_TARGET libtorrent-rasterbar)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/core/Utils.cpp
    src/core/SessionManager.cpp
    src/models/TorrentTableModel.cpp
    src/models/ProgressDelegate.cpp
    src/ui/Theme.cpp
    src/ui/MainWindow.cpp
    src/ui/AddTorrentDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/SpeedGraphWidget.cpp
    src/ui/DetailsPanel.cpp
)

add_executable(undertow ${SOURCES})

target_include_directories(undertow PRIVATE
    src
    src/core
    src/models
    src/ui
)

target_link_libraries(undertow PRIVATE
    Qt6::Widgets
    PkgConfig::LIBTORRENT
    Threads::Threads
)

target_compile_options(undertow PRIVATE -Wall -Wextra -Wno-unused-parameter)

install(TARGETS undertow RUNTIME DESTINATION bin)
install(FILES resources/undertow.desktop DESTINATION share/applications)
