cmake_minimum_required(VERSION 3.5)

project(QtPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)

if(QT_VERSION VERSION_LESS 6.0)
    message("< Qt6.0")
else()
    message(">= Qt6.0")
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)
endif()


set(QWINDOWKIT_BUILD_EXAMPLES OFF CACHE BOOL "")
set(QWINDOWKIT_BUILD_STATIC ON CACHE BOOL "")

add_subdirectory(qwindowkit)
add_subdirectory(windowbar)


set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        av_util.cpp
        av_util.h
        ffplayer.cpp
        ffplayer.h
        yuvvideowidget.cpp
        yuvvideowidget.h
        util.cpp
        util.h
        device.cpp
        device.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT_NAME}
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        skin/skin.qrc
    )
else()
    add_executable(${PROJECT_NAME}
        ${PROJECT_SOURCES}
        skin/skin.qrc
    )
endif()


# target_link_libraries(QWindowKitDemo PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)
# target_link_libraries(QWindowKitDemo PRIVATE QWKCore QWKWidgets WidgetFrame)

target_include_directories(${PROJECT_NAME} PRIVATE "D:\\dev\\ThirdLib\\include")
target_link_directories(${PROJECT_NAME} PRIVATE "D:\\dev\\ThirdLib\\lib")
target_link_libraries(${PROJECT_NAME} PRIVATE avformat avdevice avcodec swresample swscale avutil)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE strmiids ole32 oleaut32)
endif()


qm_configure_target(${PROJECT_NAME}
    QT_LINKS Widgets OpenGL OpenGLWidgets
    LINKS QWKCore QWKWidgets WindowBar
)


if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(${PROJECT_NAME})
endif()
