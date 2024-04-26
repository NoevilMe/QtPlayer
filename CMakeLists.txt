cmake_minimum_required(VERSION 3.5)

project(QtPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "")
set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "")

add_subdirectory(qwindowkit)
add_subdirectory(windowbar)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(QWindowKitDemo
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        skin/skin.qrc
    )
else()
    add_executable(QWindowKitDemo
        ${PROJECT_SOURCES}
        skin/skin.qrc
    )
endif()


# target_link_libraries(QWindowKitDemo PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
# target_link_libraries(QWindowKitDemo PRIVATE QWKCore QWKWidgets WidgetFrame)

qm_configure_target(QWindowKitDemo
    QT_LINKS Widgets
    LINKS QWKCore QWKWidgets WidgetFrame
)


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(QWindowKitDemo)
endif()
