QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
QT       += opengl openglwidgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    av_util.cpp \
    device.cpp \
    ffplayer.cpp \
    main.cpp \
    mainwindow.cpp \
    util.cpp \
    yuvvideowidget.cpp

HEADERS += \
    av_util.h \
    device.h \
    ffplayer.h \
    mainwindow.h \
    util.h \
    yuvvideowidget.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

include("$$PWD/qwindowkit/qwindowkit.pri")

message($$join(INCLUDEPATH, " "))

RESOURCES += \
    skin/skin.qrc

# FFmpeg
LIBS += -lavformat      \
        -lavdevice      \
        -lavcodec       \
        -lswresample    \
        -lswscale       \
        -lavutil

win32 {
    DEFINES += WIN32_LEAN_AND_MEAN

    ## opencv
    ##LIBS += -lopencv_core341        \
    ##        -lopencv_highgui341     \
    ##        -lopencv_imgcodecs341   \
    ##        -lopencv_imgproc341     \
    ##        -lopencv_videoio341     \

    INCLUDEPATH += "D:\\dev\\ThirdLib\\include"
    LIBS += "-LD:\\dev\\ThirdLib\\lib"

    ## Windows API
    LIBS += -lstrmiids -lole32 -loleaut32
    # LIBS += -lkernel32    \
    #         -luser32      \
    #         -lgdi32       \
    #         \
    #         -lopengl32    \
    #         -lglu32       \
    #         \
    #         -lole32       \
    #         -loleaut32    \
    #         -lstrmiids    \
    #         \
    #         -lws2_32      \
    #         -lsecur32     \

    # win32-msvc {
    #     if (contains(DEFINES, WIN64)) {
    #         DESTDIR = $$_PRO_FILE_PWD_/bin/msvc2015_x64
    #         LIBS += -L$$_PRO_FILE_PWD_/3rd/lib/msvc2015_x64
    #     } else {
    #         DESTDIR = $$_PRO_FILE_PWD_/bin/msvc2015_x86
    #         LIBS += -L$$_PRO_FILE_PWD_/3rd/lib/msvc2015_x86
    #     }
    # }

    # win32-g++ {
    #     QMAKE_CFLAGS += -std=c99
    #     QMAKE_CXXFLAGS += -std=c++11
    #     if (contains(DEFINES, WIN64)) {
    #         DESTDIR = $$_PRO_FILE_PWD_/bin/mingw64
    #         LIBS += -L$$_PRO_FILE_PWD_/3rd/lib/mingw64
    #     } else {
    #         DESTDIR = $$_PRO_FILE_PWD_/bin/mingw32
    #         LIBS += -L$$_PRO_FILE_PWD_/3rd/lib/mingw32
    #     }

    #     # for ffmpeg staticlib
    #     LIBS += -liconv \
    #     -lz     \
    #     -lbz2   \
    #     -llzma  \
    #     -lcrypto \
    #     -lbcrypt
    # }
}
