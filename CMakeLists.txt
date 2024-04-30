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
        av_def.h
        ui/mainwindow.cpp
        ui/mainwindow.h
        ui/mainwindow.ui
        ui/playerform.h
        ui/playerform.cpp
        ui/playerform.ui
        ui/multiplayerform.h
        ui/multiplayerform.cpp
        ui/multiplayerform.ui
        ui/openmediadialog.h
        ui/openmediadialog.cpp
        ui/openmediadialog.ui
        ui/yuvvideowidget.cpp
        ui/yuvvideowidget.h
        player/av_util.cpp
        player/av_util.h
        player/ffplayer.cpp
        player/ffplayer.h
        player/device.cpp
        player/device.h
        util/util.cpp
        util/util.h
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
