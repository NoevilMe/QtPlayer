#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

namespace QWK {
class WidgetWindowAgent;
class StyleAgent;
} // namespace QWK

class MainWindow : public QMainWindow {
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
    void on_pushButtonFullScreen_toggled(bool checked);

private:
    void installWindowAgent();

private:
    Ui::MainWindow *ui;
    QWK::WidgetWindowAgent *windowAgent;

    // fullscreen
    QWidget *fsWidget_ = nullptr;
    QWidget *fsParent_ =nullptr;
    Qt::WindowFlags fsFlags_;

    Theme currentTheme{};
    void loadStyleSheet(Theme theme);

    // QObject interface
public:
    bool eventFilter(QObject *watched, QEvent *event) override;
};
#endif // MAINWINDOW_H
