!defined(QMAKE_QWK_INCLUDED, var) {
    QMAKE_QWK_INCLUDED = 1

    INCLUDEPATH += $$PWD

    include("$$PWD/etc/share/qmake/QWKWidgets.pri")
    include("$$PWD/windowbar/windowbar.pri")
}
