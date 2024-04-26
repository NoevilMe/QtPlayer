#include "mainwindow.h"
#include "playerform.h"

#include "util.h"
#include <QApplication>

extern "C" {
#include <libavdevice/avdevice.h>
#include <libavutil/log.h>
}

int main(int argc, char *argv[]) {
    util::log::Init("play.log", 1024 * 1024 * 25, 20);
    util::log::EnableConsole();
    util::log::SetLevel("trace");

    avdevice_register_all();

    QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QApplication a(argc, argv);
    // MainWindow w;
    MainWindow w;
    // PlayerForm w;
    w.show();
    return a.exec();
}
