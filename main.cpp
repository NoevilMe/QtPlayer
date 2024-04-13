#include "mainwindow.h"

#include <QWKQuick/qwkquickglobal.h>

#include <QApplication>

int main(int argc, char *argv[])
{
    QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
