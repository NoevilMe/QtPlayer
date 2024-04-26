#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

namespace QWK {
class WidgetWindowAgent;
class StyleAgent;
} // namespace QWK

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    enum Theme {
        Dark,
        Light,
    };
    Q_ENUM(Theme)

    enum WindowType {
        Player,
        Monitor,
    };
    Q_ENUM(WindowType)

Q_SIGNALS:
    void themeChanged();

protected:
    bool event(QEvent *event) override;

    void closeEvent(QCloseEvent *event) override;

private:
    void installWindowAgent();

    void getAllDevices();

    void switchWindowType(WindowType type);

private:
    Ui::MainWindow *ui;
    QWK::WidgetWindowAgent *windowAgent;

    Theme currentTheme{};
    void loadStyleSheet(Theme theme);

    QWidget *pagePlayer;
    QWidget *pageMonitor;
};

#endif // PLAYERWINDOW_H
