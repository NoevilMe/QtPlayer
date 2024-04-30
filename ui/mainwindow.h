#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H

#include <QMainWindow>

namespace QWK {
class WidgetWindowAgent;
class StyleAgent;
} // namespace QWK

class PlayerForm;
class MultiPlayerForm;

namespace Ui {
class MainWindow;
}

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

    void openMediaActionTriggered(bool checked);

private:
    Ui::MainWindow *ui;
    QWK::WidgetWindowAgent *windowAgent;

    Theme currentTheme{};
    void loadStyleSheet(Theme theme);

    PlayerForm *pagePlayer;
    MultiPlayerForm *pageMonitor;
};

#endif // PLAYERWINDOW_H
