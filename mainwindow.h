#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

namespace QWK {
class WidgetWindowAgent;
class StyleAgent;
}


class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum Theme {
        Dark,
        Light,
    };
    Q_ENUM(Theme)

Q_SIGNALS:
    void themeChanged();

protected:
    bool event(QEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_pushButtonList_toggled(bool checked);

private:
    void installWindowAgent();
private:
    Ui::MainWindow *ui;
     QWK::WidgetWindowAgent *windowAgent;

    Theme currentTheme{};
    void loadStyleSheet(Theme theme);
};
#endif // MAINWINDOW_H
